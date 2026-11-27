//! Harmonic Compressor on OpenPluginSDK, with the identity of 1.5.3 so its sessions open in this
//! version. The rules for this code are in AGENTS.md.
//!
//! Signal path, as 1.5.3's `processBlock`: input gain -> oversampled detector (internal or external
//! sidechain, its high-pass, the active model, stereo-linked) -> gain and makeup -> ADAA harmonics ->
//! back to the session rate -> dry/wet mix against the dry signal run through the same resampler
//! filters -> transient designer -> Auto Gain (K-weighted loudness of the untouched input) -> Delta
//! (output minus the aligned dry, or minus the aligned untouched input with Auto Gain on) -> output gain.

pub mod dsp;
#[cfg(target_os = "macos")]
pub mod editor;
#[cfg(target_os = "macos")]
pub mod gfx;
pub mod multirate;
pub mod params;
pub mod state;

use std::sync::atomic::{AtomicU8, AtomicU32, Ordering};
use std::sync::{Arc, Mutex};

use dsp::{CompParams, Engine, Loudness, Smoothed, Transient, db_to_gain, gain_to_db};
use multirate::Multirate;
use openplugin::core::foreign_state::ForeignStateKind;
use openplugin::core::{AaxIds, AuIds, Vst3Ids};
use openplugin::dsp::{Ppm, PpmType, VuIec};
use openplugin::prelude::*;
use params::{COUNT, PARAMS, idx};
use openplugin::core::spsc;
pub use state::UiState;
use state::Snapshots;

/// The feature `test-identity` gives a build its own name and ids, so it can be installed beside
/// the shipped plugin for host tests. Never ship it.
const TEST_IDENTITY: bool = cfg!(feature = "test-identity");
const ID_AND_NAME: (&str, &str) = if TEST_IDENTITY {
    ("com.opengrid.harmoniccompressor.test", "Harmonic Compressor Test")
} else {
    ("com.opengrid.harmoniccompressor", "Harmonic Compressor")
};

pub static INFO: PluginInfo =
    PluginInfo::new(ID_AND_NAME.0, ID_AND_NAME.1, "OpenGrid", env!("CARGO_PKG_VERSION"), Category::Dynamics, &PARAMS)
        .with_url("https://github.com/MistaMin/HarmonicCompressor");

/// 1.5.3's buses: main mono or stereo, and a sidechain of the same width that a host may leave off.
/// The first layout is the one a host starts with, so it carries the sidechain.
static LAYOUTS: [IoConfig; 4] = [
    IoConfig { input: SpeakerLayout::Stereo, output: SpeakerLayout::Stereo, sidechain: Some(SpeakerLayout::Stereo) },
    IoConfig { input: SpeakerLayout::Stereo, output: SpeakerLayout::Stereo, sidechain: None },
    IoConfig { input: SpeakerLayout::Mono, output: SpeakerLayout::Mono, sidechain: Some(SpeakerLayout::Mono) },
    IoConfig { input: SpeakerLayout::Mono, output: SpeakerLayout::Mono, sidechain: None },
];

const FACTORS: [usize; 4] = [1, 2, 4, 8];

/// A sample as the engine may see it: NaN, infinity and anything below -600 dBFS become silence.
#[inline]
fn clean(x: f32) -> f32 {
    if x.is_finite() && x.abs() >= 1e-30 { x } else { 0.0 }
}

/// After the input stops: the harmonics' 3.5 Hz DC blocker decays below -120 dB in about 0.63 s.
const TAIL_SECONDS: f32 = 0.7;

/// One block's peaks for the Compression History: input and output (linear), gain reduction (dB),
/// and the block's length.
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct HistoryFrame {
    pub input: f32,
    pub output: f32,
    pub reduction: f32,
    pub seconds: f32,
}

#[derive(Default)]
struct AtomicF32(AtomicU32);
impl AtomicF32 {
    fn new(v: f32) -> Self {
        AtomicF32(AtomicU32::new(v.to_bits()))
    }
    fn get(&self) -> f32 {
        f32::from_bits(self.0.load(Ordering::Relaxed))
    }
    fn set(&self, v: f32) {
        self.0.store(v.to_bits(), Ordering::Relaxed);
    }
}

/// The meter scales the editor offers: PPM and RMS in dBFS, VU in dB VU.
pub const METER_PPM: usize = 0;
pub const METER_RMS: usize = 1;
pub const METER_VU: usize = 2;
/// 0 VU is a steady sine of -18 dBFS peak, as the SDK's `MeterBank` and 1.5.3's 0 dBu.
pub const VU_REFERENCE_DBFS: f32 = -18.0;

/// No editor is open: the level meters rest (a PPM's 8x rectifier is a fifth of the plugin's cost).
const METER_OFF: u8 = u8::MAX;

/// The level meters, input L, R and output L, R, run on every sample of the host rate, one scale at
/// a time, while an editor shows it:
/// PPM after IEC 60268-10 Type I (DIN 45406: 5 ms integration, 20 dB return in 1.7 s, a steady
/// sine reads its peak), RMS over 300 ms (one pole on the power, as the SDK's `MeterBank`; a sine
/// reads 3 dB under its peak) and VU after IEC 60268-17 (300 ms to 99 %, 1.3 % overshoot).
struct Levels {
    ppm: [Ppm; 4],
    vu: [VuIec; 4],
    power: [f64; 4],
    rms_alpha: f64,
}

impl Levels {
    fn new(sample_rate: f32) -> Self {
        Levels {
            ppm: [Ppm::new(sample_rate, PpmType::Din); 4],
            vu: std::array::from_fn(|_| VuIec::new(sample_rate)),
            power: [0.0; 4],
            rms_alpha: 1.0 - (-1.0 / (f64::from(sample_rate.max(1000.0)) * 0.3)).exp(),
        }
    }

    /// One block of the input (`at` 0) or the output (`at` 2) on `scale`; a missing second channel
    /// is silence.
    fn feed(&mut self, scale: usize, at: usize, io: &[&mut [f32]], n: usize) {
        for (ch, x) in io.iter().take(2).enumerate() {
            let k = at + ch;
            let x = &x[..n];
            match scale {
                METER_PPM => x.iter().for_each(|&s| {
                    self.ppm[k].next(clean(s));
                }),
                METER_VU => x.iter().for_each(|&s| {
                    self.vu[k].next(clean(s));
                }),
                _ => {
                    let power = &mut self.power[k];
                    for &s in x {
                        let s = f64::from(clean(s));
                        *power += (s * s - *power) * self.rms_alpha;
                    }
                    if *power < 1e-30 {
                        *power = 0.0; // ST-3: no subnormal state
                    }
                }
            }
        }
    }

    /// The `scale` meter's reading in its unit (dBFS, dBFS, dB VU), floored at -120 dBFS.
    fn reading(&self, scale: usize, k: usize) -> f32 {
        let db = |x: f64| (20.0 * x.max(1e-6).log10()) as f32;
        match scale {
            METER_PPM => db(f64::from(self.ppm[k].level())),
            // VuIec reads a sine's RMS; the VU scale counts its peak (+3.01 dB) re -18 dBFS
            METER_VU => self.vu[k].level_db() + 20.0 * std::f32::consts::SQRT_2.log10() - VU_REFERENCE_DBFS,
            _ => db(self.power[k].sqrt()),
        }
    }

    fn reset(&mut self) {
        self.ppm.iter_mut().for_each(Ppm::reset);
        self.vu.iter_mut().for_each(VuIec::reset);
        self.power = [0.0; 4];
    }
}

/// The readouts 1.5.3 published once per block for its editor (`lastGrDbForUI`, `lastInputPeakDb`,
/// `lastOutputPeakDb`, `autoGainAppliedDb`), and the level meters (`Levels`) that replace its
/// peak bars.
pub struct Meters {
    gr: AtomicF32,
    input: AtomicF32,
    output: AtomicF32,
    /// [scale][input L, R, output L, R], in the scale's unit.
    levels: [[AtomicF32; 4]; 3],
    /// The scale an open editor shows, or `METER_OFF`.
    watched: AtomicU8,
    auto_gain: AtomicF32,
}

impl Default for Meters {
    fn default() -> Self {
        Meters {
            gr: AtomicF32::new(0.0),
            input: AtomicF32::new(-60.0),
            output: AtomicF32::new(-60.0),
            levels: std::array::from_fn(|scale| std::array::from_fn(|_| AtomicF32::new(-120.0 - if scale == METER_VU { VU_REFERENCE_DBFS } else { 0.0 }))),
            watched: AtomicU8::new(METER_OFF),
            auto_gain: AtomicF32::new(0.0),
        }
    }
}

impl Meters {
    pub fn gr_db(&self) -> f32 {
        self.gr.get()
    }
    pub fn last_input_db(&self) -> f32 {
        self.input.get()
    }
    pub fn last_output_db(&self) -> f32 {
        self.output.get()
    }
    /// The `scale` meter's reading of input L, R (`k` 0, 1) or output L, R (2, 3).
    pub fn level(&self, scale: usize, k: usize) -> f32 {
        self.levels[scale.min(2)][k.min(3)].get()
    }
    /// An editor shows `scale` (it says so on every tick), or none is open: the engine runs that
    /// meter only. A scale that was not running starts at rest.
    pub fn watch(&self, scale: Option<usize>) {
        let now = scale.map_or(METER_OFF, |s| s.min(2) as u8);
        if self.watched.swap(now, Ordering::Relaxed) != now
            && let Some(s) = scale
        {
            let rest = Meters::default();
            for k in 0..4 {
                self.levels[s.min(2)][k].set(rest.level(s, k));
            }
        }
    }
    pub fn auto_gain_db(&self) -> f32 {
        self.auto_gain.get()
    }
}

/// What the engine shares with its editor. The audio thread writes the meters and the history; the
/// per-model memory and the look are only touched on the main thread (editor, state).
pub struct Shared {
    pub meters: Meters,
    pub history: Mutex<spsc::Consumer<HistoryFrame>>,
    pub history_drops: AtomicU32,
    /// Each model's remembered settings (1.5.3's per-model memory), saved with the session.
    pub snapshots: Mutex<Snapshots>,
    pub ui: Mutex<UiState>,
}

pub struct HarmonicCompressor {
    p: [f32; COUNT],
    shared: Arc<Shared>,
    history: spsc::Producer<HistoryFrame>,
    levels: Levels,
    /// The scale `levels` ran on in the last block.
    watched: u8,
    engine: Engine,
    /// The wet path, and two chains with the same filters for the dry and the untouched input.
    wet: Multirate,
    dry_chain: Multirate,
    reference_chain: Multirate,
    transient: Transient,
    loudness: Loudness,
    auto_gain: Smoothed,
    delta: Smoothed,
    delta_reference: Smoothed,
    input_gain: Smoothed,
    output_gain: Smoothed,
    /// Per channel, `max_block` long: the dry after input gain, the untouched input, and its copy
    /// for the loudness reference.
    dry: [Vec<f32>; 2],
    reference: [Vec<f32>; 2],
    loudness_input: [Vec<f32>; 2],
    sample_rate: f32,
    max_block: usize,
    external_sidechain_seen: bool,
    reference_running: bool,
    meters: MeterFrame,
}

impl Default for HarmonicCompressor {
    fn default() -> Self {
        // 1.5.3's history FIFO held 2048 blocks; with no editor draining it, blocks are dropped
        let (history, rx) = spsc::channel(2048);
        let shared = Arc::new(Shared {
            meters: Meters::default(),
            history: Mutex::new(rx),
            history_drops: AtomicU32::new(0),
            snapshots: Mutex::new(state::default_snapshots()),
            ui: Mutex::new(UiState::default()),
        });
        let mut plugin = Self {
            p: params::defaults(),
            shared,
            history,
            levels: Levels::new(48000.0),
            watched: METER_OFF,
            engine: Engine::default(),
            wet: Multirate::new(48000.0, 1),
            dry_chain: Multirate::new(48000.0, 1),
            reference_chain: Multirate::new(48000.0, 1),
            transient: Transient::default(),
            loudness: Loudness::default(),
            auto_gain: Smoothed::new(1.0, true),
            delta: Smoothed::new(0.0, false),
            delta_reference: Smoothed::new(0.0, false),
            input_gain: Smoothed::new(1.0, true),
            output_gain: Smoothed::new(1.0, true),
            dry: Default::default(),
            reference: Default::default(),
            loudness_input: Default::default(),
            sample_rate: 48000.0,
            max_block: 0,
            external_sidechain_seen: false,
            reference_running: false,
            meters: MeterFrame::SILENT,
        };
        // ST-9: a host may process before it prepares.
        plugin.prepare(48000.0, 512, 2);
        plugin
    }
}

impl HarmonicCompressor {
    fn on(&self, i: usize) -> bool {
        self.p[i] > 0.5
    }

    fn factor(&self) -> usize {
        FACTORS[(self.p[idx::OVERSAMPLE_MODE].round() as usize).min(3)]
    }

    fn linear(&self) -> bool {
        self.p[idx::OVERSAMPLE_FILTER].round() >= 1.0
    }

    /// Replaces every value at once (a state load).
    fn apply(&mut self, s: state::State) -> bool {
        self.p = s.values;
        if let Ok(mut snaps) = self.shared.snapshots.lock() {
            *snaps = s.snapshots;
        }
        if let Ok(mut ui) = self.shared.ui.lock() {
            *ui = s.ui;
        }
        true
    }

    /// One block of at most `max_block` frames.
    fn block(&mut self, io: &mut [&mut [f32]], sidechain: Option<&[&[f32]]>, n: usize) {
        let channels = io.len();
        let p = self.p;
        let on = |i: usize| p[i] > 0.5;
        let params = CompParams {
            threshold_db: p[idx::THRESHOLD],
            ratio: p[idx::RATIO],
            attack_ms: p[idx::ATTACK],
            release_ms: p[idx::RELEASE],
            knee_db: p[idx::KNEE],
            auto_release: on(idx::AUTO_RELEASE),
            auto_attack: on(idx::AUTO_ATTACK),
        };
        let mode = (p[idx::MODE].round() as usize).min(dsp::MODE_COUNT - 1);
        let (sc_enabled, sc_listen) = (on(idx::SC_ENABLED), on(idx::SC_LISTEN));
        let (auto_on, delta_on) = (on(idx::AUTO_GAIN), on(idx::DELTA));
        let harmonics = [p[idx::H2] / 100.0, p[idx::H3] / 100.0, p[idx::H4] / 100.0, p[idx::H5] / 100.0];

        // ST-2, ST-3: NaN, infinity and denormals at the input never reach a filter or envelope.
        for ch in io.iter_mut() {
            for x in ch[..n].iter_mut() {
                *x = clean(*x);
            }
        }

        // the untouched input: Auto Gain's loudness reference and Delta's dry with Auto Gain on
        for ((r, l), ch) in self.reference.iter_mut().zip(self.loudness_input.iter_mut()).zip(io.iter()) {
            r[..n].copy_from_slice(&ch[..n]);
            l[..n].copy_from_slice(&ch[..n]);
        }

        let in_db = p[idx::INPUT_GAIN];
        self.input_gain.set_target(db_to_gain(in_db));
        if self.input_gain.is_smoothing() || in_db.abs() > 1.0e-4 {
            for s in 0..n {
                let g = self.input_gain.next_value();
                for ch in io.iter_mut() {
                    ch[s] *= g;
                }
            }
        }

        let (factor, linear) = (self.factor(), self.linear());
        self.wet.set(factor, linear);
        self.dry_chain.set(factor, linear);
        self.reference_chain.set(factor, linear);
        self.engine.ensure_sample_rate(f64::from(self.sample_rate) * factor as f64);
        self.engine.update(mode, &params, p[idx::SC_HP_FREQ]);

        // A silent sidechain bus can mean nothing is routed: the main input is detected until the
        // sidechain first carries signal, and from then on a silent sidechain means no compression.
        // shortcut: unrouting a used sidechain keeps it selected until the host re-prepares, as in 1.5.3.
        if let Some(sc) = sidechain
            && !self.external_sidechain_seen
            && sc.iter().any(|c| c[..n].iter().any(|x| x.abs() > 1.0e-7))
        {
            self.external_sidechain_seen = true;
        }
        let external = sidechain.filter(|_| self.external_sidechain_seen);

        let makeup = db_to_gain(p[idx::MAKEUP]);
        let mix = p[idx::MIX];

        // Delta's untouched-input reference is heard only with Delta and Auto Gain on, so its filters
        // run only then; Delta's 20 ms fade-in covers their warm-up.
        let reference_needed =
            ((delta_on && !sc_listen) || self.delta.current() > 0.0) && (auto_on || self.delta_reference.current() > 0.0);
        if reference_needed && !self.reference_running {
            self.reference_chain.reset();
        }
        self.reference_running = reference_needed;

        let [d0, d1] = &mut self.dry;
        let [r0, r1] = &mut self.reference;
        for (dry, ch) in [&mut *d0, &mut *d1].into_iter().zip(io.iter()) {
            dry[..n].copy_from_slice(&ch[..n]);
        }
        let mut dry: [&mut [f32]; 2] = [&mut d0[..n], &mut d1[..n]];
        let mut reference: [&mut [f32]; 2] = [&mut r0[..n], &mut r1[..n]];
        self.dry_chain.process(&mut dry[..channels], n, |_| {});
        if reference_needed {
            self.reference_chain.process(&mut reference[..channels], n, |_| {});
        }

        let watched = self.shared.meters.watched.load(Ordering::Relaxed);
        if watched != self.watched {
            self.levels.reset();
            self.watched = watched;
        }
        let scale = usize::from(watched);
        if watched != METER_OFF {
            self.levels.feed(scale, 0, io, n);
        }
        let mut input_peak = [0.0f32; 2];
        for (peak, ch) in input_peak.iter_mut().zip(io.iter()) {
            *peak = ch[..n].iter().fold(0.0, |m, x| m.max(x.abs()));
        }

        // oversampled: detector, gain, makeup and harmonics; stereo-linked in every model
        let engine = &mut self.engine;
        let mut min_gain = 1.0f32;
        self.wet.process(io, n, |os| {
            let os_n = os.first().map_or(0, |c| c.len());
            for s in 0..os_n {
                let mut detector = 0.0f32;
                let mut sc_ch = [0.0f32; 2];
                for ch in 0..os.len() {
                    let raw = match external {
                        // the session-rate sidechain, interpolated at the oversampled position
                        Some(sc) => {
                            let key = sc[ch.min(sc.len() - 1)];
                            let pos = s as f32 / factor as f32;
                            let i0 = (pos as usize).min(n - 1);
                            let i1 = (i0 + 1).min(n - 1);
                            clean(key[i0] + (pos - i0 as f32) * (key[i1] - key[i0]))
                        }
                        None => os[ch][s],
                    };
                    let v = if sc_enabled { engine.sidechain.process(ch, raw) } else { raw };
                    if ch < 2 {
                        sc_ch[ch] = v;
                    }
                    detector = detector.max(v.abs());
                }
                let gr = engine.gain(detector);
                min_gain = min_gain.min(gr);
                for (ch, data) in os.iter_mut().enumerate() {
                    data[s] = if sc_listen { sc_ch[ch.min(1)] } else { engine.harmonics.process(data[s] * gr * makeup, harmonics, ch) };
                }
            }
        });

        // latency-aligned dry/wet
        if !sc_listen && mix < 1.0 {
            for (ch, dry) in io.iter_mut().zip(dry.iter()) {
                for (x, d) in ch[..n].iter_mut().zip(dry.iter()) {
                    *x = mix * *x + (1.0 - mix) * *d;
                }
            }
        }

        if !sc_listen {
            self.transient.process(io, n, on(idx::TRANSIENT_ENABLED), p[idx::TRANSIENT_ATTACK], p[idx::TRANSIENT_SUSTAIN], p[idx::TRANSIENT_SPEED]);
            // measured always, so engaging Auto Gain lands on a settled value; Listen freezes it
            let [l0, l1] = &mut self.loudness_input;
            let input: [&mut [f32]; 2] = [&mut l0[..n], &mut l1[..n]];
            self.loudness.process(&input[..channels], io, n);
        }

        let auto_db = if auto_on && !sc_listen { self.loudness.target_db() } else { 0.0 };
        self.auto_gain.set_target(db_to_gain(auto_db));
        if self.auto_gain.is_smoothing() || (self.auto_gain.current() - 1.0).abs() > 1.0e-6 {
            for s in 0..n {
                let g = self.auto_gain.next_value();
                for ch in io.iter_mut() {
                    ch[s] *= g;
                }
            }
        }

        // Delta: output minus the dry it is compared with, both switches crossfaded
        self.delta.set_target(if delta_on && !sc_listen { 1.0 } else { 0.0 });
        self.delta_reference.set_target(if auto_on { 1.0 } else { 0.0 });
        if self.delta.is_smoothing() || self.delta.current() > 0.0 {
            for s in 0..n {
                let amount = self.delta.next_value();
                let use_input = self.delta_reference.next_value();
                for (ch, x) in io.iter_mut().enumerate() {
                    let d = dry[ch][s];
                    x[s] -= amount * (d + use_input * (reference[ch][s] - d));
                }
            }
        } else {
            self.delta_reference.skip(n);
        }

        let out_db = p[idx::OUTPUT_GAIN];
        self.output_gain.set_target(db_to_gain(out_db));
        if self.output_gain.is_smoothing() || out_db.abs() > 1.0e-4 {
            for s in 0..n {
                let g = self.output_gain.next_value();
                for ch in io.iter_mut() {
                    ch[s] *= g;
                }
            }
        }

        // filter state decays below f32 range on silence; nothing under -600 dBFS reaches the host
        for ch in io.iter_mut() {
            for x in ch[..n].iter_mut() {
                if x.abs() < 1e-30 {
                    *x = 0.0;
                }
            }
        }

        if watched != METER_OFF {
            self.levels.feed(scale, 2, io, n);
        }
        let mut output_peak = [0.0f32; 2];
        for (peak, ch) in output_peak.iter_mut().zip(io.iter()) {
            *peak = ch[..n].iter().fold(0.0, |m, x| m.max(x.abs()));
        }
        let mut m = MeterFrame::SILENT;
        for ch in 0..2 {
            let src = ch.min(channels - 1);
            m.input_db[ch] = gain_to_db(input_peak[src], -100.0);
            m.output_db[ch] = gain_to_db(output_peak[src], -100.0);
        }
        let reduction = -gain_to_db(min_gain, -120.0);
        m.reduction_db = reduction;
        self.meters = m;
        self.publish(input_peak, output_peak, reduction, n);
    }

    /// The editor's readouts, as 1.5.3's processBlock published them after each block (a second
    /// channel that a mono layout lacks reads silence there).
    fn publish(&mut self, input_peak: [f32; 2], output_peak: [f32; 2], reduction: f32, n: usize) {
        let (input, output) = (input_peak[0].max(input_peak[1]), output_peak[0].max(output_peak[1]));
        let seconds = n as f32 / self.sample_rate;
        if self.history.push(HistoryFrame { input, output, reduction, seconds }).is_err() {
            self.shared.history_drops.fetch_add(1, Ordering::Relaxed);
        }
        let meters = &self.shared.meters;
        meters.gr.set(self.engine.last_gr_db());
        meters.input.set(gain_to_db(input, -72.0).max(-72.0));
        meters.output.set(gain_to_db(output, -72.0).max(-72.0));
        if let Some(levels) = meters.levels.get(usize::from(self.watched)) {
            for (k, level) in levels.iter().enumerate() {
                level.set(self.levels.reading(usize::from(self.watched), k));
            }
        }
        meters.auto_gain.set(gain_to_db(self.auto_gain.current(), -100.0));
    }
}

impl Plugin for HarmonicCompressor {
    fn info(&self) -> &'static PluginInfo {
        &INFO
    }

    // Main thread: may allocate. RT-6: everything that depends on the rate is recomputed here.
    fn prepare(&mut self, sample_rate: f32, max_block: usize, _channels: usize) {
        let sr = f64::from(sample_rate);
        let max_block = max_block.max(1);
        self.sample_rate = sample_rate;
        self.max_block = max_block;
        self.external_sidechain_seen = false;
        let (factor, linear) = (self.factor(), self.linear());
        for chain in [&mut self.wet, &mut self.dry_chain, &mut self.reference_chain] {
            *chain = Multirate::new(sr, max_block);
            chain.set(factor, linear);
        }
        self.engine.prepare(sr * factor as f64);
        self.transient.prepare(sr);
        self.levels = Levels::new(sample_rate);
        self.loudness.prepare(sr);
        self.auto_gain.reset(sr, 0.100);
        self.delta.reset(sr, 0.020);
        self.delta_reference.reset(sr, 0.050);
        self.input_gain.reset(sr, 0.010);
        self.output_gain.reset(sr, 0.010);
        for buffers in [&mut self.dry, &mut self.reference, &mut self.loudness_input] {
            *buffers = [vec![0.0; max_block], vec![0.0; max_block]];
        }
        self.reset();
    }

    fn reset(&mut self) {
        self.wet.reset();
        self.dry_chain.reset();
        self.reference_chain.reset();
        self.reference_running = false;
        self.transient.reset();
        self.levels.reset();
        self.loudness.reset();
        self.auto_gain.set_current_and_target(1.0);
        self.delta.set_current_and_target(if self.on(idx::DELTA) { 1.0 } else { 0.0 });
        self.delta_reference.set_current_and_target(if self.on(idx::AUTO_GAIN) { 1.0 } else { 0.0 });
        self.engine.reset();
        self.input_gain.set_current_and_target(db_to_gain(self.p[idx::INPUT_GAIN]));
        self.output_gain.set_current_and_target(db_to_gain(self.p[idx::OUTPUT_GAIN]));
        self.meters = MeterFrame::SILENT;
        self.shared.meters.auto_gain.set(0.0);
    }

    fn set_param(&mut self, id: &str, value: f32) -> bool {
        params::index_of(id).is_some_and(|i| self.set_param_index(i, value))
    }

    fn set_param_index(&mut self, index: usize, value: f32) -> bool {
        if index >= COUNT {
            return false;
        }
        self.p[index] = params::legal(index, value); // ST-4: clamps, rejects NaN, snaps as 1.5.3
        true
    }

    fn param(&self, id: &str) -> Option<f32> {
        params::index_of(id).map(|i| self.p[i])
    }

    fn meters(&self) -> MeterFrame {
        self.meters
    }

    fn latency(&self) -> usize {
        self.wet.latency_for(self.factor(), self.linear())
    }

    fn max_latency(&self) -> Option<usize> {
        FACTORS.iter().flat_map(|&f| [self.wet.latency_for(f, false), self.wet.latency_for(f, true)]).max()
    }

    fn tail(&self) -> Tail {
        Tail::from_samples((TAIL_SECONDS * self.sample_rate).ceil() as usize)
    }

    fn supported_layouts(&self) -> &'static [IoConfig] {
        &LAYOUTS
    }

    fn process(&mut self, io: &mut [&mut [f32]], frames: usize) {
        self.process_buses(io, None, frames);
    }

    fn process_buses(&mut self, io: &mut [&mut [f32]], sidechain: Option<&[&[f32]]>, frames: usize) {
        let channels = io.len().min(2);
        let frames = io.iter().take(channels).map(|c| c.len()).min().unwrap_or(0).min(frames);
        if channels == 0 || frames == 0 || self.max_block == 0 {
            return;
        }
        let sc_frames = sidechain.and_then(|sc| sc.iter().take(2).map(|c| c.len()).min()).unwrap_or(0);
        let (a, b) = io.split_at_mut(1);
        let mut start = 0;
        // ST-9: a host may send more frames than prepare announced; work in pieces of max_block.
        while start < frames {
            let end = (start + self.max_block).min(frames);
            let mut pair: [&mut [f32]; 2] = [&mut a[0][start..end], if channels == 2 { &mut b[0][start..end] } else { &mut [] }];
            let key: [&[f32]; 2] = match sidechain {
                Some(sc) if sc_frames >= end => [&sc[0][start..end], &sc[sc.len().min(2) - 1][start..end]],
                _ => [&[], &[]],
            };
            let key_channels = sidechain.map_or(0, |sc| sc.len().min(2));
            let sc = (sc_frames >= end && key_channels > 0).then_some(&key[..key_channels]);
            self.block(&mut pair[..channels], sc, end - start);
            start = end;
        }
    }

    fn note_ports(&self) -> NotePorts {
        NotePorts { input: false, output: false }
    }

    fn save_state(&mut self) -> Option<Vec<u8>> {
        let snapshots = self.shared.snapshots.lock().map(|s| *s).unwrap_or_else(|_| state::default_snapshots());
        let ui = self.shared.ui.lock().map(|u| *u).unwrap_or_default();
        Some(state::encode(&self.p, &snapshots, &ui))
    }

    #[cfg(target_os = "macos")]
    fn editor_state(&self) -> Option<openplugin::core::EditorState> {
        Some(self.shared.clone())
    }

    fn load_state(&mut self, data: &[u8]) -> bool {
        state::decode(data).is_some_and(|s| self.apply(s))
    }

    fn foreign_state_kinds(&self) -> &'static [ForeignStateKind] {
        &[ForeignStateKind::Vst3Component, ForeignStateKind::AuClassInfo, ForeignStateKind::AaxChunk]
    }

    fn load_foreign_state(&mut self, kind: ForeignStateKind, data: &[u8]) -> bool {
        let state = match kind {
            ForeignStateKind::AuClassInfo => state::decode_au_class_info(data),
            _ => state::decode_juce(data),
        };
        state.is_some_and(|s| self.apply(s))
    }

    // ---- identity of 1.5.3 ----
    // A test-identity build pins its AU code (Hcmt) and an LV2 URI with a scheme (a host cannot
    // save the bare id the SDK would derive); the SDK derives the rest from its id.
    fn vst3_ids(&self) -> Option<Vst3Ids> {
        if TEST_IDENTITY {
            return None;
        }
        Vst3Ids::from_hex("56535448636D706861726D6F6E696320", "56534548636D706861726D6F6E696320")
    }

    fn au_ids(&self) -> Option<AuIds> {
        Some(AuIds { subtype: if TEST_IDENTITY { *b"Hcmt" } else { *b"Hcmp" }, manufacturer: *b"OpGr" })
    }

    fn aax_ids(&self) -> Option<AaxIds> {
        // 1.5.3's AAX plug-in types: 'jcaa' plus the mono (1) or stereo (2) layout in and out.
        (!TEST_IDENTITY).then_some(AaxIds { manufacturer: *b"OpGr", product: *b"Hcmp", mono: *b"jcbb", stereo: *b"jccc" })
    }

    fn lv2_uri(&self) -> Option<&'static str> {
        Some(if TEST_IDENTITY { "urn:opengrid:harmoniccompressor:test" } else { "urn:opengrid:harmoniccompressor" })
    }
}

#[cfg(target_os = "macos")]
openplugin::targets::export_clap!(HarmonicCompressor, editor = editor::make_editor);
#[cfg(target_os = "macos")]
openplugin::targets::export_lv2!(HarmonicCompressor, editor = editor::make_editor);
#[cfg(target_os = "macos")]
openplugin::targets::export_shell!(HarmonicCompressor, editor = editor::make_editor);
#[cfg(not(target_os = "macos"))]
openplugin::targets::export_clap!(HarmonicCompressor);
#[cfg(not(target_os = "macos"))]
openplugin::targets::export_lv2!(HarmonicCompressor);
#[cfg(not(target_os = "macos"))]
openplugin::targets::export_shell!(HarmonicCompressor);
