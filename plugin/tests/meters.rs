//! Meter ballistics (plugin-qa "Meter ballistics checks"), measured on the readings the engine
//! publishes for the editor, with no editor: each rig asks for one scale as an editor's tick does
//! (the engine runs only the scale that is shown). The input meter is read at default settings
//! (input gain 0 dB). Standards, numbers from the cited documents as the SDK quotes them:
//!
//! - PPM: IEC 60268-10 Type I (DIN 45406), in dBFS: a steady sine reads its peak; a 10 ms burst
//!   of 5 kHz reads -1 dB and a 5 ms burst -2 dB (ITU-T J.15 table 1, +/- 0.5 dB); the return is
//!   20 dB in 1.7 s (+/- 0.3 s, ITU-R BS.645-2 table 1).
//! - VU: IEC 60268-17: 0 VU is a steady sine of -18 dBFS peak (the owner's and the SDK's
//!   calibration); a step reaches 99 % of its reading in 300 ms and overshoots 1 % to 1.5 %.
//! - RMS: no standard; a 300 ms one-pole on the power (the SDK's `MeterBank`), so a sine reads
//!   3 dB under its peak and the window does not depend on the rate or the block size.

use harmonic_compressor::{HarmonicCompressor, METER_PPM, METER_RMS, METER_VU, Shared};
use openplugin::prelude::*;
use std::sync::Arc;

struct Rig {
    plugin: HarmonicCompressor,
    shared: Arc<Shared>,
    scale: usize,
    rate: f32,
    block: usize,
    t: usize,
}

impl Rig {
    fn new(rate: f32, block: usize, scale: usize) -> Rig {
        let mut plugin = HarmonicCompressor::default();
        plugin.prepare(rate, block, 2);
        let shared = plugin.editor_state().expect("editor state").downcast::<Shared>().expect("Shared");
        shared.meters.watch(Some(scale));
        Rig { plugin, shared, scale, rate, block, t: 0 }
    }

    /// Runs `seconds` of `signal(time) -> [left, right]`; returns the readings (input L, R, output
    /// L, R) after each block.
    fn run(&mut self, seconds: f32, signal: impl Fn(f32) -> [f32; 2]) -> Vec<[f32; 4]> {
        let frames = (seconds * self.rate).round() as usize;
        let mut out = Vec::new();
        let mut done = 0;
        while done < frames {
            let n = self.block.min(frames - done);
            let mut l = vec![0.0f32; n];
            let mut r = vec![0.0f32; n];
            for i in 0..n {
                let [a, b] = signal((self.t + i) as f32 / self.rate);
                l[i] = a;
                r[i] = b;
            }
            let mut io: Vec<&mut [f32]> = vec![&mut l, &mut r];
            self.plugin.process(&mut io, n);
            self.t += n;
            done += n;
            let m = &self.shared.meters;
            out.push(std::array::from_fn(|k| m.level(self.scale, k)));
        }
        out
    }

    fn input(&self) -> f32 {
        self.shared.meters.level(self.scale, 0)
    }
}

fn sine(db: f32, hz: f32) -> impl Fn(f32) -> [f32; 2] {
    let a = 10f32.powf(db / 20.0);
    move |t| {
        let x = a * (std::f32::consts::TAU * hz * t).sin();
        [x, x]
    }
}

fn silence(_: f32) -> [f32; 2] {
    [0.0, 0.0]
}

#[test]
fn a_steady_sine_reads_its_reference_on_every_scale() {
    for rate in [44_100.0, 48_000.0, 96_000.0] {
        for hz in [100.0, 1_000.0, 10_000.0] {
            let [ppm, rms, vu] = [METER_PPM, METER_RMS, METER_VU].map(|scale| {
                let mut rig = Rig::new(rate, 256, scale);
                rig.run(3.0, sine(-18.0, hz));
                rig.input()
            });
            println!("calibration {rate} {hz}: PPM {ppm:.3} dBFS, RMS {rms:.3} dBFS, VU {vu:.3}");
            assert!((ppm + 18.0).abs() < 0.5, "{rate} Hz rate, {hz} Hz: PPM {ppm} dBFS");
            assert!((rms + 21.03).abs() < 0.2, "{rate} Hz rate, {hz} Hz: RMS {rms} dBFS");
            assert!(vu.abs() < 0.2, "{rate} Hz rate, {hz} Hz: VU {vu} (0 VU = -18 dBFS)");
        }
    }
}

#[test]
fn vu_rises_to_99_percent_in_300_ms_with_its_overshoot() {
    for rate in [48_000.0, 96_000.0] {
        let mut rig = Rig::new(rate, 16, METER_VU);
        let block_s = 16.0 / rate;
        let r = rig.run(1.5, sine(-18.0, 1_000.0));
        let lin = |v: f32| 10f32.powf(v / 20.0);
        let t99 = r.iter().position(|x| lin(x[0]) >= 0.99).expect("reaches 99 %") as f32 * block_s;
        let peak = r.iter().map(|x| lin(x[0])).fold(0.0, f32::max);
        let over = (peak - 1.0) * 100.0;
        println!("vu {rate}: 99 % at {:.1} ms, overshoot {over:.3} %", t99 * 1000.0);
        // the SDK's own VuIec test allows 10 ms on a rectified step; a sine's ripple adds about 1 ms
        assert!((t99 - 0.300).abs() <= 0.010, "{rate}: 99 % at {t99} s");
        assert!((1.0..=1.5).contains(&over), "{rate}: overshoot {over:.2} %");
    }
}

#[test]
fn ppm_reads_bursts_and_returns_as_type_i() {
    for rate in [44_100.0, 48_000.0, 96_000.0] {
        for (ms, expect) in [(10.0, -1.0), (5.0, -2.0)] {
            let mut rig = Rig::new(rate, 16, METER_PPM);
            let tone = sine(-10.0, 5_000.0);
            let r = rig.run(0.2, move |t| if t < ms / 1000.0 { tone(t) } else { [0.0, 0.0] });
            let max = r.iter().map(|x| x[0]).fold(-200.0, f32::max);
            let reading = max + 10.0;
            println!("ppm {rate}: {ms} ms burst {reading:.3} dB");
            assert!((reading - expect).abs() <= 0.5, "{rate}: {ms} ms burst reads {reading:.2} dB, {expect} expected");
        }
        let mut rig = Rig::new(rate, 64, METER_PPM);
        rig.run(1.0, sine(-6.0, 1_000.0));
        let start = rig.input();
        let r = rig.run(3.0, silence);
        let fallen = r.iter().position(|x| x[0] <= start - 20.0).expect("falls 20 dB") as f32 * 64.0 / rate;
        println!("ppm {rate}: 20 dB fall in {fallen:.3} s");
        assert!((fallen - 1.7).abs() <= 0.3, "{rate}: 20 dB fall in {fallen:.2} s");
    }
}

#[test]
fn readings_do_not_depend_on_block_size_or_rate() {
    let at_300ms = |rate: f32, block: usize| {
        [METER_PPM, METER_RMS, METER_VU].map(|scale| {
            let mut rig = Rig::new(rate, block, scale);
            rig.run(0.3, sine(-12.0, 1_000.0));
            rig.input()
        })
    };
    let reference = at_300ms(48_000.0, 64);
    println!("at 300 ms, 48 kHz / 64: {reference:?}");
    for (rate, block) in [(48_000.0, 480), (96_000.0, 64), (96_000.0, 960)] {
        let other = at_300ms(rate, block);
        println!("at 300 ms, {rate} / {block}: {other:?}");
        for s in 0..3 {
            assert!((reference[s] - other[s]).abs() < 0.1, "scale {s} at {rate} Hz, blocks of {block}: {} vs {}", other[s], reference[s]);
        }
    }
    // the RMS window is 300 ms: a step reads 1 - 1/e of its power then, -2 dB
    assert!((reference[METER_RMS] - (-15.01 - 1.99)).abs() < 0.15, "RMS at 300 ms: {}", reference[METER_RMS]);
}

#[test]
fn silence_overs_and_bad_samples_read_sanely() {
    for (scale, over) in [(METER_PPM, 6.0), (METER_RMS, 2.99), (METER_VU, 24.0)] {
        let mut rig = Rig::new(48_000.0, 256, scale);
        rig.run(0.5, silence);
        assert!(rig.input().is_finite() && rig.input() <= -78.0, "silence reads {} on scale {scale}", rig.input());
        rig.run(2.0, sine(6.0, 1_000.0));
        assert!((rig.input() - over).abs() < 0.5, "+6 dBFS reads {} on scale {scale}", rig.input());
        rig.run(0.1, |t| if t < 2.55 { [f32::NAN, f32::INFINITY] } else { [0.5, 0.5] });
        rig.run(3.0, silence);
        assert!(rig.input().is_finite() && rig.shared.meters.level(scale, 1).is_finite(), "scale {scale} after NaN");
    }
}

#[test]
fn each_channel_reads_its_own_level() {
    for left in [true, false] {
        let mut rig = Rig::new(48_000.0, 256, METER_PPM);
        let tone = sine(-6.0, 1_000.0);
        rig.run(2.0, move |t| if left { [tone(t)[0], 0.0] } else { [0.0, tone(t)[0]] });
        let m = &rig.shared.meters;
        let (loud, quiet) = if left { (0, 1) } else { (1, 0) };
        assert!((m.level(METER_PPM, loud) + 6.0).abs() < 0.5 && m.level(METER_PPM, quiet) <= -100.0, "left loud: {left}");
    }
}

/// With no editor the meters rest (they cost a fifth of the plugin); a scale starts from rest when
/// an editor asks for it.
#[test]
fn the_meters_run_only_for_an_editor() {
    let mut rig = Rig::new(48_000.0, 256, METER_PPM);
    rig.shared.meters.watch(None);
    rig.run(1.0, sine(-6.0, 1_000.0));
    assert!(rig.input() <= -100.0, "nobody watching: {}", rig.input());
    rig.shared.meters.watch(Some(METER_PPM));
    rig.run(1.0, sine(-6.0, 1_000.0));
    assert!((rig.input() + 6.0).abs() < 0.5, "watched: {}", rig.input());
    rig.shared.meters.watch(Some(METER_VU));
    assert!(rig.shared.meters.level(METER_VU, 0) < -50.0, "a new scale starts at rest");
}
