//! The 1.5.3 signal path, ported from the JUCE sources (Source/CompressorCore.h and Source/DSP/*.h):
//! the seven compressor models, the ADAA harmonics, the sidechain high-pass, the transient designer,
//! the loudness matcher behind Auto Gain, and JUCE's SmoothedValue. Arithmetic is kept in the C++
//! order and precision so the port nulls against 1.5.3 at 1x.

use std::sync::OnceLock;

// ---------------------------------------------------------------- helpers (juce::Decibels, jmap)

#[inline]
pub fn gain_to_db(gain: f32, floor: f32) -> f32 {
    if gain > 0.0 { (gain.log10() * 20.0).max(floor) } else { floor }
}

#[inline]
pub fn db_to_gain(db: f32) -> f32 {
    if db > -100.0 { 10f32.powf(db * 0.05) } else { 0.0 }
}

#[inline]
fn jmap(t: f32, lo: f32, hi: f32) -> f32 {
    lo + t * (hi - lo)
}

// ---------------------------------------------------------------- juce::SmoothedValue

/// JUCE's `SmoothedValue<float>`, linear or multiplicative, with the same step arithmetic.
#[derive(Clone, Copy)]
pub struct Smoothed {
    current: f32,
    target: f32,
    step: f32,
    countdown: i32,
    steps: i32,
    multiplicative: bool,
}

impl Smoothed {
    pub const fn new(value: f32, multiplicative: bool) -> Self {
        Self { current: value, target: value, step: 0.0, countdown: 0, steps: 0, multiplicative }
    }

    pub fn reset(&mut self, sample_rate: f64, seconds: f64) {
        self.steps = (seconds * sample_rate).floor() as i32;
        self.set_current_and_target(self.target);
    }

    pub fn set_current_and_target(&mut self, v: f32) {
        self.target = v;
        self.current = v;
        self.countdown = 0;
    }

    pub fn set_target(&mut self, v: f32) {
        if v == self.target {
            return;
        }
        if self.steps <= 0 {
            self.set_current_and_target(v);
            return;
        }
        self.target = v;
        self.countdown = self.steps;
        self.step = if self.multiplicative {
            ((self.target.abs().ln() - self.current.abs().ln()) / self.countdown as f32).exp()
        } else {
            (self.target - self.current) / self.countdown as f32
        };
    }

    pub fn is_smoothing(&self) -> bool {
        self.countdown > 0
    }

    pub fn current(&self) -> f32 {
        self.current
    }

    #[inline]
    pub fn next_value(&mut self) -> f32 {
        if !self.is_smoothing() {
            return self.target;
        }
        self.countdown -= 1;
        if self.countdown > 0 {
            if self.multiplicative { self.current *= self.step } else { self.current += self.step }
        } else {
            self.current = self.target;
        }
        self.current
    }

    pub fn skip(&mut self, n: usize) {
        let n = n.min(i32::MAX as usize) as i32;
        if n >= self.countdown {
            self.set_current_and_target(self.target);
            return;
        }
        if self.multiplicative {
            self.current *= f64::from(self.step).powi(n) as f32;
        } else {
            self.current += self.step * n as f32;
        }
        self.countdown -= n;
    }
}

// ---------------------------------------------------------------- the gain computers

pub const RED: usize = 0;
pub const HYBRID: usize = 1;
pub const BRIDGE: usize = 2;
pub const BUS_VCA: usize = 3;
pub const FET76: usize = 4;
pub const OPTO: usize = 5;
pub const VCA160: usize = 6;
pub const MODE_COUNT: usize = 7;

#[derive(Clone, Copy, Debug)]
pub struct CompParams {
    pub threshold_db: f32,
    pub ratio: f32,
    pub attack_ms: f32,
    pub release_ms: f32,
    pub knee_db: f32,
    pub auto_release: bool,
    pub auto_attack: bool,
}

impl Default for CompParams {
    fn default() -> Self {
        Self { threshold_db: -18.0, ratio: 4.0, attack_ms: 10.0, release_ms: 100.0, knee_db: 6.0, auto_release: false, auto_attack: false }
    }
}

/// Each model's hardware-style attack, release and knee (`AlgorithmDefaults`).
pub fn mode_defaults(mode: usize) -> (f32, f32, f32) {
    match mode {
        RED => (10.0, 300.0, 6.0),
        HYBRID => (5.0, 150.0, 6.0),
        BRIDGE => (2.0, 400.0, 0.0),
        BUS_VCA => (3.0, 100.0, 0.0),
        FET76 => (0.2, 200.0, 0.0),
        OPTO => (116.54, 91.85, 0.0),
        VCA160 => (34.5, 34.5, 0.0),
        _ => (10.0, 100.0, 6.0),
    }
}

fn slope_of(ratio: f32) -> f32 {
    if ratio >= 99.5 || ratio.abs() < 0.01 { 1.0 } else { 1.0 - 1.0 / ratio }
}

fn soft_knee_gr(input_db: f32, p: &CompParams) -> f32 {
    let slope = slope_of(p.ratio);
    let knee = p.knee_db.max(0.0);
    let over = input_db - p.threshold_db;
    if knee <= 0.0001 {
        return if over > 0.0 { -over * slope } else { 0.0 };
    }
    let half = knee * 0.5;
    if over <= -half {
        return 0.0;
    }
    if over >= half {
        return -over * slope;
    }
    let x = over + half;
    -(slope * x * x) / (2.0 * knee)
}

#[inline]
fn time_coeff(ms: f32, min_ms: f32, sr: f32) -> f32 {
    (-1.0 / (0.001 * ms.max(min_ms) * sr)).exp()
}

/// Red: feed-forward, peak detection, soft knee, a transient-preserving relief term.
#[derive(Default)]
struct Red {
    sr: f32,
    p: CompParams,
    env_db: f32,
    transient: f32,
    auto_slow: f32,
    att: f32,
    rel: f32,
    tr: f32,
}

impl Red {
    fn reset(&mut self) {
        self.env_db = 0.0;
        self.transient = 0.0;
        // 1.5.3 kept this across a reset; a reset now clears it (ST-7).
        self.auto_slow = 0.0;
    }
    fn update(&mut self, p: &CompParams) {
        self.p = *p;
        self.att = time_coeff(p.attack_ms, 0.1, self.sr);
        self.rel = time_coeff(p.release_ms, 0.1, self.sr);
        self.tr = time_coeff(1.5, 0.1, self.sr);
    }
    fn process(&mut self, x: f32) -> (f32, f32) {
        let rect = x.abs();
        self.transient = self.tr * self.transient + (1.0 - self.tr) * rect;
        let punch = ((rect - self.transient) * 6.0).clamp(0.0, 3.0);
        let input_db = gain_to_db(rect + 1.0e-12, -100.0);
        let target = soft_knee_gr(input_db, &self.p) + punch;
        let rising = target < self.env_db;
        let (mut rel, mut att) = (self.rel, self.att);
        if self.p.auto_release || self.p.auto_attack {
            self.auto_slow = 0.9997 * self.auto_slow + 0.0003 * self.env_db.abs();
            let a = (self.auto_slow / 6.0).clamp(0.0, 1.0);
            if self.p.auto_release {
                rel = time_coeff(jmap(a, self.p.release_ms * 0.4, self.p.release_ms * 2.0), 0.1, self.sr);
            }
            if self.p.auto_attack {
                att = time_coeff(jmap(a, self.p.attack_ms * 0.3, self.p.attack_ms * 1.2), 0.1, self.sr);
            }
        }
        let c = if rising { att } else { rel };
        self.env_db = c * self.env_db + (1.0 - c) * target.min(0.0);
        (db_to_gain(self.env_db), self.env_db)
    }
}

/// Hybrid: a feed-forward detector blended 75/25 with a feedback path.
#[derive(Default)]
struct Hybrid {
    sr: f32,
    p: CompParams,
    ff_db: f32,
    fb_db: f32,
    auto_slow: f32,
    att: f32,
    rel: f32,
}

impl Hybrid {
    const FEEDBACK: f32 = 0.25;
    fn reset(&mut self) {
        self.ff_db = 0.0;
        self.fb_db = 0.0;
        self.auto_slow = 0.0;
    }
    fn update(&mut self, p: &CompParams) {
        self.p = *p;
        self.att = time_coeff(p.attack_ms, 0.1, self.sr);
        self.rel = time_coeff(p.release_ms, 0.1, self.sr);
    }
    /// Reduction only: a ratio between 0 and 1 would otherwise be an unbounded boost.
    fn reduction_db(&self, level: f32) -> f32 {
        let over = gain_to_db(level + 1.0e-12, -100.0) - self.p.threshold_db;
        if over > 0.0 { (-over * slope_of(self.p.ratio)).min(0.0) } else { 0.0 }
    }
    fn process(&mut self, x: f32) -> (f32, f32) {
        let target = self.reduction_db(x.abs());
        let (mut rel, mut att) = (self.rel, self.att);
        if self.p.auto_release || self.p.auto_attack {
            self.auto_slow = 0.9997 * self.auto_slow + 0.0003 * self.ff_db.abs();
            let a = (self.auto_slow / 6.0).clamp(0.0, 1.0);
            if self.p.auto_release {
                rel = time_coeff(jmap(a, self.p.release_ms * 0.3, self.p.release_ms * 2.5), 0.1, self.sr);
            }
            if self.p.auto_attack {
                att = time_coeff(jmap(a, self.p.attack_ms * 0.3, self.p.attack_ms * 1.2), 0.1, self.sr);
            }
        }
        let c = if target < self.ff_db { att } else { rel };
        self.ff_db = c * self.ff_db + (1.0 - c) * target;
        let blended = self.ff_db * (1.0 - Self::FEEDBACK) + self.fb_db * Self::FEEDBACK;
        let gain = db_to_gain(blended);
        // the detector level after this gain, on the release time; it acts from the next sample
        self.fb_db = self.rel * self.fb_db + (1.0 - self.rel) * self.reduction_db(x.abs() * gain);
        (gain, blended)
    }
}

/// Opto Leveler: a reduced optical cell (see dsp/CIRCUIT_INTEGRATION.md).
struct Opto {
    sr: f64,
    cell: [f32; 3],
    attack: [f32; 3],
    release: [f32; 3],
    light: f32,
    light_coeff: f32,
    gain: f32,
    drive: f32,
    limit: bool,
}

impl Default for Opto {
    fn default() -> Self {
        Self { sr: 44100.0, cell: [0.0; 3], attack: [0.0; 3], release: [0.0; 3], light: 0.0, light_coeff: 0.0, gain: 1.0, drive: 1.0, limit: false }
    }
}

impl Opto {
    fn coeff(&self, seconds: f64) -> f32 {
        (-1.0 / (self.sr * seconds)).exp() as f32
    }
    fn prepare(&mut self, sr: f64) {
        self.sr = sr;
        self.light_coeff = self.coeff(0.001);
        self.attack = [self.coeff(0.11654), self.coeff(0.11654), self.coeff(1.0)];
        self.release = [self.coeff(0.01333), self.coeff(0.09185), self.coeff(0.69596)];
        self.reset();
    }
    fn reset(&mut self) {
        self.cell = [0.0; 3];
        self.light = 0.0;
        self.gain = 1.0;
    }
    fn update(&mut self, p: &CompParams) {
        self.drive = db_to_gain(-p.threshold_db);
        // the ratio knob selects the source's Compress / Limit configurations
        self.limit = p.ratio >= 10.0;
    }
    fn process(&mut self, x: f32) -> (f32, f32) {
        let r7: f32 = if self.limit { 2700.0 } else { 0.0 };
        let junction = self.gain + (1.0 - self.gain) * r7 / (68000.0 + r7);
        let raw = (x.abs() * self.drive * junction).clamp(0.0, 10.0).powf(5.1);
        self.light = self.light_coeff * self.light + (1.0 - self.light_coeff) * raw;
        for i in 0..3 {
            let c = if self.light > self.cell[i] { self.attack[i] } else { self.release[i] };
            self.cell[i] = c * self.cell[i] + (1.0 - c) * self.light;
        }
        let state = 0.65467 * self.cell[0] + 0.2131 * self.cell[1] + 0.13223 * self.cell[2];
        let conductance = 2.0e-7 + 0.001 * state.max(0.0).powf(0.9);
        self.gain = (1.0 + (68000.0 + r7) * 2.0e-7) / (1.0 + (68000.0 + r7) * conductance);
        (self.gain, gain_to_db(self.gain, -120.0))
    }
}

/// VCA 160: a mean-square detector and a log-domain VCA.
#[derive(Default)]
struct RmsVca {
    p: CompParams,
    power: f32,
    coeff: f32,
}

impl RmsVca {
    fn prepare(&mut self, sr: f64) {
        // constant-current log-cap release of about 126 dB/s
        let tau = 10.0 / 10f64.ln() / 126.0;
        self.coeff = (-1.0 / (sr * tau)).exp() as f32;
        self.reset();
    }
    fn reset(&mut self) {
        self.power = 1e-12;
    }
    fn process(&mut self, x: f32) -> (f32, f32) {
        self.power = self.coeff * self.power + (1.0 - self.coeff) * x * x;
        self.power = self.power.max(1e-12);
        let level = 10.0 * self.power.log10();
        let over = level - self.p.threshold_db;
        let slope = 1.0 - 1.0 / self.p.ratio.clamp(1.0, 100.0);
        // paired -6 mV/dB detector, 1/2 divider, +2.98 mV/dB VCA
        let control = if over >= 0.0 { over } else { 0.01 * over };
        let gain_db = (-slope * (3.0 / 2.98) * control).clamp(-100.0, 6.0);
        (db_to_gain(gain_db), gain_db)
    }
}

/// Bridge, Bus VCA and 76: reduced feedback circuits (see dsp/CIRCUIT_INTEGRATION.md).
struct Circuit {
    kind: usize,
    sr: f32,
    p: CompParams,
    envelope: f32,
    memory: f32,
    gain: f32,
    drive: f32,
    attack: f32,
    release: f32,
    memory_coeff: f32,
}

impl Circuit {
    fn new(kind: usize) -> Self {
        Self { kind, sr: 44100.0, p: CompParams::default(), envelope: 0.0, memory: 0.0, gain: 1.0, drive: 1.0, attack: 0.0, release: 0.0, memory_coeff: 0.0 }
    }
    fn coeff(&self, ms: f32) -> f32 {
        (-1.0 / (self.sr * 0.001 * ms.max(0.02))).exp()
    }
    fn reset(&mut self) {
        self.envelope = 0.0;
        self.memory = 0.0;
        self.gain = 1.0;
    }
    fn update(&mut self, p: &CompParams) {
        self.p = *p;
        let default_attack = match self.kind {
            FET76 => 0.2,
            BRIDGE => 2.0,
            _ => 3.0,
        };
        self.attack = self.coeff(if p.auto_attack { default_attack } else { p.attack_ms });
        self.release = self.coeff(p.release_ms);
        // Bridge C36/R81 recovery memory: 47 uF * 120 kOhm
        self.memory_coeff = self.coeff(if self.kind == BRIDGE { 5640.0 } else { 1200.0 });
        self.drive = db_to_gain(-p.threshold_db);
    }
    fn process(&mut self, x: f32) -> (f32, f32) {
        let ratio = self.p.ratio.clamp(1.0, 100.0);
        let mut detected = x.abs() * self.drive;
        if self.kind == FET76 {
            // preamp feedback rectifier, ratio-dependent GR gain
            let gr_gain = if ratio >= 99.0 {
                10.15
            } else if ratio >= 20.0 {
                9.08
            } else if ratio >= 12.0 {
                5.23
            } else if ratio >= 8.0 {
                3.23
            } else if ratio >= 4.0 {
                2.0
            } else {
                1.0
            };
            detected *= self.gain * gr_gain;
        } else {
            // Bus model sidechain/main control tracking = 8.89/9.53
            detected *= if self.kind == BUS_VCA { self.gain.powf(8.89 / 9.53) } else { self.gain };
        }
        self.memory = self.memory_coeff * self.memory + (1.0 - self.memory_coeff) * self.envelope;
        let mut target = detected;
        if self.p.auto_release && target < self.envelope {
            target = target.max(self.memory);
        }
        let c = if target > self.envelope { self.attack } else { self.release };
        self.envelope = c * self.envelope + (1.0 - c) * target;
        if self.kind == FET76 {
            // small-signal BF245B channel conductance, R3 27k and R4 180R
            let gate = (-3.5 + 1.6 * self.envelope).clamp(-3.5, 0.0);
            let conductance = 2.0 * 0.0012 * (gate + 2.5).max(0.0);
            self.gain = 1.0 / (1.0 + 27000.0 * conductance / (1.0 + 180.0 * conductance));
        } else {
            let over = gain_to_db(self.envelope, -120.0).max(0.0);
            let tracking = if self.kind == BUS_VCA { 8.89 / 9.53 } else { 1.0 };
            let slope = (ratio - 1.0) / tracking;
            self.gain = db_to_gain(-(over * slope).min(80.0));
        }
        (self.gain, gain_to_db(self.gain, -120.0))
    }
}

// ---------------------------------------------------------------- ADAA harmonics

struct RailTable {
    g: [f64; RailTable::SIZE + 1],
    f: [f64; RailTable::SIZE + 1],
}

impl RailTable {
    const SIZE: usize = 4096;
    const MAX: f64 = 8.0;
    const STEP: f64 = Self::MAX / Self::SIZE as f64;

    fn value(t: f64) -> f64 {
        t / (1.0 + t.abs().powf(8.0)).powf(0.125)
    }

    fn build() -> Self {
        let mut t = Self { g: [0.0; Self::SIZE + 1], f: [0.0; Self::SIZE + 1] };
        for i in 0..=Self::SIZE {
            t.f[i] = Self::value(i as f64 * Self::STEP);
        }
        for i in 0..Self::SIZE {
            let mut acc = 0.0;
            let t0 = i as f64 * Self::STEP;
            let h = Self::STEP / 16.0;
            for k in 0..16 {
                let a = t0 + k as f64 * h;
                let m = a + 0.5 * h;
                let b = a + h;
                acc += (h / 6.0) * (Self::value(a) + 4.0 * Self::value(m) + Self::value(b));
            }
            t.g[i + 1] = t.g[i] + acc;
        }
        t
    }

    /// Integral of `value` over [0, t], t >= 0 (Hermite over the Simpson table).
    fn integral(&self, t: f64) -> f64 {
        if t >= Self::MAX {
            return self.g[Self::SIZE] + (t - Self::MAX);
        }
        let u = t / Self::STEP;
        let i = (u as usize).min(Self::SIZE - 1);
        let s = u - i as f64;
        let (s2, s3) = (s * s, s * s * s);
        (2.0 * s3 - 3.0 * s2 + 1.0) * self.g[i] + (s3 - 2.0 * s2 + s) * Self::STEP * self.f[i] + (-2.0 * s3 + 3.0 * s2) * self.g[i + 1] + (s3 - s2) * Self::STEP * self.f[i + 1]
    }
}

static RAIL: OnceLock<RailTable> = OnceLock::new();

/// Builds the table on first use. `Harmonics::default` calls it, so the audio thread only reads it.
fn rail() -> &'static RailTable {
    RAIL.get_or_init(RailTable::build)
}

/// First-order ADAA of the residual f(x) - slope x (zero delay, linear part unaveraged).
#[derive(Default, Clone, Copy)]
struct Residual {
    x1: f64,
    fr1: f64,
    r1: f64,
    primed: bool,
}

impl Residual {
    fn step(&mut self, x: f64, f: f64, big_f: f64, slope: f64) -> f64 {
        let lin = slope * x;
        let r = f - lin;
        let fr = big_f - 0.5 * slope * x * x;
        if !self.primed {
            *self = Self { x1: x, fr1: fr, r1: r, primed: true };
            return f;
        }
        let dx = x - self.x1;
        let mut mean = 0.5 * (r + self.r1);
        if dx.abs() > 1.0e-5 * 1.0f64.max(x.abs()).max(self.x1.abs()) {
            mean = (fr - self.fr1) / dx;
            let (lo, hi) = (r.min(self.r1), r.max(self.r1));
            let tol = (hi - lo) + 1.0e-6 * (1.0 + r.abs());
            if !(mean >= lo - tol && mean <= hi + tol) {
                mean = 0.5 * (r + self.r1);
            }
        }
        self.x1 = x;
        self.fr1 = fr;
        self.r1 = r;
        lin + mean
    }
}

/// 2nd to 5th harmonic shaper with ADAA, a smooth rail limiter and a 3.5 Hz DC blocker.
pub struct Harmonics {
    rail: &'static RailTable,
    dc_r: f32,
    shaper: [Residual; 2],
    limiter: [Residual; 2],
    dc_x1: [f32; 2],
    dc_y1: [f32; 2],
}

impl Default for Harmonics {
    fn default() -> Self {
        Self { rail: rail(), dc_r: 0.9995, shaper: [Residual::default(); 2], limiter: [Residual::default(); 2], dc_x1: [0.0; 2], dc_y1: [0.0; 2] }
    }
}

impl Harmonics {
    pub fn prepare(&mut self, sr: f64) {
        self.dc_r = (-2.0 * std::f64::consts::PI * 3.5 / sr).exp() as f32;
        self.reset();
    }

    pub fn reset(&mut self) {
        self.shaper = [Residual::default(); 2];
        self.limiter = [Residual::default(); 2];
        self.dc_x1 = [0.0; 2];
        self.dc_y1 = [0.0; 2];
    }

    /// `h2..h5` are 0..1.
    #[inline]
    pub fn process(&mut self, x: f32, h: [f32; 4], channel: usize) -> f32 {
        let ch = channel.min(1);
        let xd = f64::from(x);
        let c2 = 0.50 * f64::from(h[0]);
        let c3 = 0.50 * f64::from(h[1]);
        let c4 = 0.25 * f64::from(h[2]);
        let c5 = 0.20 * f64::from(h[3]);
        let active = (c2 + c3 + c4 + c5) > 1.0e-7;
        let slope = 1.0 - c3 - c5;
        let antideriv = |u: f64| {
            let u2 = u * u;
            let u4 = u2 * u2;
            (c2 / 3.0) * (u2 * u) + c3 * (0.25 * u4 - 0.5 * u2) + (c4 / 5.0) * (u4 * u) + c5 * ((u4 * u2) / 6.0 - 0.5 * u2)
        };
        let (g_val, g_int) = if !active {
            (0.0, 0.0)
        } else if xd > 1.0 {
            (c2 + c4, antideriv(1.0) + (c2 + c4) * (xd - 1.0))
        } else if xd < -1.0 {
            (c2 + c4, antideriv(-1.0) + (c2 + c4) * (xd + 1.0))
        } else {
            let x2 = xd * xd;
            let x3 = x2 * xd;
            (c2 * x2 + c3 * (x3 - xd) + c4 * (x2 * x2) + c5 * (x3 * x2 - xd), antideriv(xd))
        };
        let shaped = self.shaper[ch].step(xd, xd + g_val, 0.5 * xd * xd + g_int, slope);
        // the limiter stays primed while harmonics are off, so automation is click-free
        let limited = self.limiter[ch].step(shaped, RailTable::value(shaped), self.rail.integral(shaped.abs()), 1.0);
        let in_s = limited as f32;
        let out = in_s - self.dc_x1[ch] + self.dc_r * self.dc_y1[ch];
        self.dc_x1[ch] = in_s;
        self.dc_y1[ch] = out;
        if active { out } else { x }
    }
}

// ---------------------------------------------------------------- sidechain high-pass

/// 2nd-order Butterworth high-pass, 20 to 500 Hz, on the detector input.
pub struct SidechainHp {
    sr: f64,
    last_sr: f64,
    hz: f32,
    b0: f32,
    b1: f32,
    b2: f32,
    a1: f32,
    a2: f32,
    z1: [f32; 2],
    z2: [f32; 2],
}

impl Default for SidechainHp {
    fn default() -> Self {
        Self { sr: 48000.0, last_sr: -1.0, hz: 150.0, b0: 1.0, b1: 0.0, b2: 0.0, a1: 0.0, a2: 0.0, z1: [0.0; 2], z2: [0.0; 2] }
    }
}

impl SidechainHp {
    pub fn prepare(&mut self, sr: f64) {
        self.sr = sr.max(8000.0);
        self.last_sr = -1.0;
        self.reset();
        self.set_frequency(self.hz);
    }

    pub fn reset(&mut self) {
        self.z1 = [0.0; 2];
        self.z2 = [0.0; 2];
    }

    pub fn set_sample_rate(&mut self, sr: f64) {
        let sr = sr.max(8000.0);
        if (sr - self.sr).abs() > 0.5 {
            self.sr = sr;
            self.update();
        }
    }

    pub fn set_frequency(&mut self, hz: f32) {
        let hz = hz.clamp(20.0, 500.0);
        if (hz - self.hz).abs() > 0.01 || (self.sr - self.last_sr).abs() > 0.5 {
            self.hz = hz;
            self.update();
        }
    }

    fn update(&mut self) {
        self.last_sr = self.sr;
        let w0 = 2.0 * std::f64::consts::PI * f64::from(self.hz) / self.sr;
        let cosw = w0.cos();
        let alpha = w0.sin() / 2f64.sqrt();
        let a0 = 1.0 + alpha;
        self.b0 = (((1.0 + cosw) * 0.5) / a0) as f32;
        self.b1 = (-(1.0 + cosw) / a0) as f32;
        self.b2 = self.b0;
        self.a1 = ((-2.0 * cosw) / a0) as f32;
        self.a2 = ((1.0 - alpha) / a0) as f32;
    }

    #[inline]
    pub fn process(&mut self, channel: usize, x: f32) -> f32 {
        let ch = channel.min(1);
        let y = self.b0 * x + self.z1[ch];
        self.z1[ch] = self.b1 * x - self.a1 * y + self.z2[ch];
        self.z2[ch] = self.b2 * x - self.a2 * y;
        y
    }
}

// ---------------------------------------------------------------- the compressor engine

/// The seven models, the harmonics and the sidechain filter, at the oversampled rate.
pub struct Engine {
    sr: f64,
    mode: usize,
    red: Red,
    hybrid: Hybrid,
    bridge: Circuit,
    bus_vca: Circuit,
    fet: Circuit,
    opto: Opto,
    vca160: RmsVca,
    pub harmonics: Harmonics,
    pub sidechain: SidechainHp,
    last_gr_db: f32,
}

impl Default for Engine {
    fn default() -> Self {
        Self {
            sr: 44100.0,
            mode: RED,
            red: Red::default(),
            hybrid: Hybrid::default(),
            bridge: Circuit::new(BRIDGE),
            bus_vca: Circuit::new(BUS_VCA),
            fet: Circuit::new(FET76),
            opto: Opto::default(),
            vca160: RmsVca::default(),
            harmonics: Harmonics::default(),
            sidechain: SidechainHp::default(),
            last_gr_db: 0.0,
        }
    }
}

impl Engine {
    fn prepare_models(&mut self, sr: f64) {
        let srf = sr as f32;
        self.red.sr = srf;
        self.red.reset();
        self.hybrid.sr = srf;
        self.hybrid.reset();
        for c in [&mut self.bridge, &mut self.bus_vca, &mut self.fet] {
            c.sr = srf;
            c.reset();
        }
        self.opto.prepare(sr);
        self.vca160.prepare(sr);
        self.harmonics.prepare(sr);
        self.last_gr_db = 0.0;
    }

    pub fn prepare(&mut self, sr: f64) {
        self.sr = sr;
        self.prepare_models(sr);
        self.sidechain.prepare(sr);
    }

    pub fn reset(&mut self) {
        self.red.reset();
        self.hybrid.reset();
        self.bridge.reset();
        self.bus_vca.reset();
        self.fet.reset();
        self.opto.reset();
        self.vca160.reset();
        self.harmonics.reset();
        self.sidechain.reset();
        self.last_gr_db = 0.0;
    }

    /// A new oversampling factor re-prepares (and so resets) the models, as 1.5.3 did.
    pub fn ensure_sample_rate(&mut self, sr: f64) {
        if (sr - self.sr).abs() > 0.5 {
            self.sr = sr;
            self.prepare_models(sr);
            self.sidechain.set_sample_rate(sr);
        }
    }

    /// Once per block: only the active model gets the new settings.
    pub fn update(&mut self, mode: usize, p: &CompParams, sc_hz: f32) {
        self.mode = mode.min(MODE_COUNT - 1);
        match self.mode {
            RED => self.red.update(p),
            HYBRID => self.hybrid.update(p),
            BRIDGE => self.bridge.update(p),
            BUS_VCA => self.bus_vca.update(p),
            FET76 => self.fet.update(p),
            OPTO => self.opto.update(p),
            _ => self.vca160.p = *p,
        }
        self.sidechain.set_frequency(sc_hz);
    }

    /// Linear gain for one detector sample.
    #[inline]
    pub fn gain(&mut self, detector: f32) -> f32 {
        let (g, db) = match self.mode {
            RED => self.red.process(detector),
            HYBRID => self.hybrid.process(detector),
            BRIDGE => self.bridge.process(detector),
            BUS_VCA => self.bus_vca.process(detector),
            FET76 => self.fet.process(detector),
            OPTO => self.opto.process(detector),
            _ => self.vca160.process(detector),
        };
        self.last_gr_db = db;
        g
    }

    /// The active model's last gain change in dB (negative for reduction).
    pub fn last_gr_db(&self) -> f32 {
        self.last_gr_db
    }
}

// ---------------------------------------------------------------- transient designer

/// Level-independent, stereo-linked differential-envelope transient designer.
pub struct Transient {
    sr: f64,
    fast: f32,
    med: f32,
    slow: f32,
    gain_db: f32,
}

impl Default for Transient {
    fn default() -> Self {
        Self { sr: 48000.0, fast: 0.0, med: 0.0, slow: 0.0, gain_db: 0.0 }
    }
}

impl Transient {
    pub fn prepare(&mut self, sr: f64) {
        self.sr = sr.max(8000.0);
        self.reset();
    }

    pub fn reset(&mut self) {
        self.fast = 0.0;
        self.med = 0.0;
        self.slow = 0.0;
        self.gain_db = 0.0;
    }

    fn coeff(&self, ms: f32) -> f32 {
        time_coeff(ms, 0.05, self.sr as f32)
    }

    pub fn process(&mut self, io: &mut [&mut [f32]], frames: usize, enabled: bool, attack_pct: f32, sustain_pct: f32, speed_ms: f32) {
        if io.is_empty() || frames == 0 {
            return;
        }
        let scale = (speed_ms / 10.0).clamp(0.05, 5.0);
        let (fast_att, fast_rel) = (self.coeff(0.35 * scale), self.coeff(11.0 * scale));
        let (med_att, med_rel) = (self.coeff(14.0 * scale), self.coeff(58.0 * scale));
        let (slow_att, slow_rel) = (self.coeff(24.0 * scale), self.coeff(320.0 * scale));
        let smooth = self.coeff(0.6);
        let att_amount = (attack_pct * 0.01).clamp(-1.0, 1.0);
        let sus_amount = (sustain_pct * 0.01).clamp(-1.0, 1.0);
        let step = |env: f32, x: f32, a: f32, r: f32| {
            let c = if x > env { a } else { r };
            c * env + (1.0 - c) * x
        };
        for s in 0..frames {
            let det = io.iter().fold(0.0f32, |d, ch| d.max(ch[s].abs()));
            self.fast = step(self.fast, det, fast_att, fast_rel);
            self.med = step(self.med, det, med_att, med_rel);
            self.slow = step(self.slow, det, slow_att, slow_rel);
            let mut target = 0.0;
            if enabled {
                let fast_db = 20.0 * (self.fast + 1.0e-6).log10();
                let med_db = 20.0 * (self.med + 1.0e-6).log10();
                let slow_db = 20.0 * (self.slow + 1.0e-6).log10();
                // smooth floor gate (-66..-50 dBFS): silence and low hiss stay untouched
                let gate = ((slow_db + 66.0) / 16.0).clamp(0.0, 1.0);
                let att_db = att_amount * 15.0f32.min((fast_db - med_db).max(0.0) * 1.65);
                let sus_db = sus_amount * 15.0f32.min((slow_db - fast_db).max(0.0) * 1.85);
                target = (gate * (att_db + sus_db)).clamp(-18.0, 18.0);
            }
            self.gain_db = smooth * self.gain_db + (1.0 - smooth) * target;
            if enabled || self.gain_db.abs() > 1.0e-4 {
                let g = db_to_gain(self.gain_db);
                for ch in io.iter_mut() {
                    ch[s] *= g;
                }
            }
        }
    }
}

// ---------------------------------------------------------------- loudness match (Auto Gain)

#[derive(Clone, Copy)]
struct KCoeffs {
    shelf_b: [f64; 3],
    shelf_a: [f64; 3],
    hp_b: [f64; 3],
    hp_a: [f64; 3],
}

impl KCoeffs {
    /// ITU-R BS.1770 K-weighting from the analogue prototype, for any rate.
    fn design(sr: f64) -> Self {
        let pi = std::f64::consts::PI;
        let (f0, gain_db, q) = (1681.974450955533, 3.999843853973347, 0.7071752369554196);
        let k = (pi * f0 / sr).tan();
        let vh = 10f64.powf(gain_db / 20.0);
        let vb = vh.powf(0.4996667741545416);
        let a0 = 1.0 + k / q + k * k;
        let shelf_b = [(vh + vb * k / q + k * k) / a0, 2.0 * (k * k - vh) / a0, (vh - vb * k / q + k * k) / a0];
        let shelf_a = [1.0, 2.0 * (k * k - 1.0) / a0, (1.0 - k / q + k * k) / a0];
        let (f0, q) = (38.13547087602444, 0.5003270373238773);
        let k = (pi * f0 / sr).tan();
        let a0 = 1.0 + k / q + k * k;
        Self { shelf_b, shelf_a, hp_b: [1.0, -2.0, 1.0], hp_a: [1.0, 2.0 * (k * k - 1.0) / a0, (1.0 - k / q + k * k) / a0] }
    }
}

#[derive(Default, Clone, Copy)]
struct Biquad64 {
    z1: f64,
    z2: f64,
}

impl Biquad64 {
    #[inline]
    fn run(&mut self, x: f64, b: &[f64; 3], a: &[f64; 3]) -> f64 {
        let y = b[0] * x + self.z1;
        self.z1 = b[1] * x - a[1] * y + self.z2;
        self.z2 = b[2] * x - a[2] * y;
        y
    }
}

#[derive(Default, Clone, Copy)]
struct KFilter {
    shelf: Biquad64,
    hp: Biquad64,
}

impl KFilter {
    #[inline]
    fn process(&mut self, x: f32, c: &KCoeffs) -> f64 {
        let s = self.shelf.run(f64::from(x), &c.shelf_b, &c.shelf_a);
        self.hp.run(s, &c.hp_b, &c.hp_a)
    }
}

const WINDOW: usize = 30;

/// Short-term (3 s) K-weighted loudness of the input and of the processed signal, in 100 ms chunks
/// gated at -70 LUFS on the input; the gain that makes the second as loud as the first.
pub struct Loudness {
    coeffs: KCoeffs,
    chunk_len: usize,
    reference: [KFilter; 2],
    processed: [KFilter; 2],
    ref_ring: [f64; WINDOW],
    proc_ring: [f64; WINDOW],
    count: usize,
    pos: usize,
    chunk_ref: f64,
    chunk_proc: f64,
    fill: usize,
    target_db: f32,
}

impl Default for Loudness {
    fn default() -> Self {
        let mut l = Self {
            coeffs: KCoeffs::design(48000.0),
            chunk_len: 4800,
            reference: [KFilter::default(); 2],
            processed: [KFilter::default(); 2],
            ref_ring: [0.0; WINDOW],
            proc_ring: [0.0; WINDOW],
            count: 0,
            pos: 0,
            chunk_ref: 0.0,
            chunk_proc: 0.0,
            fill: 0,
            target_db: 0.0,
        };
        l.reset();
        l
    }
}

fn lufs(mean_square: f64) -> f64 {
    -0.691 + 10.0 * mean_square.max(1.0e-20).log10()
}

impl Loudness {
    pub fn prepare(&mut self, sr: f64) {
        let sr = sr.max(8000.0);
        self.coeffs = KCoeffs::design(sr);
        self.chunk_len = ((0.1 * sr).round() as usize).max(1);
        self.reset();
    }

    pub fn reset(&mut self) {
        self.reference = [KFilter::default(); 2];
        self.processed = [KFilter::default(); 2];
        self.ref_ring = [0.0; WINDOW];
        self.proc_ring = [0.0; WINDOW];
        self.count = 0;
        self.pos = 0;
        self.chunk_ref = 0.0;
        self.chunk_proc = 0.0;
        self.fill = 0;
        self.target_db = 0.0;
    }

    pub fn target_db(&self) -> f32 {
        self.target_db
    }

    /// Time-aligned reference and processed (uncompensated) blocks of the same channel count.
    pub fn process(&mut self, reference: &[&mut [f32]], processed: &[&mut [f32]], frames: usize) {
        let channels = reference.len().min(processed.len()).min(2);
        for s in 0..frames {
            for ch in 0..channels {
                // BS.1770: left and right mean squares are summed with weight 1
                let r = self.reference[ch].process(reference[ch][s], &self.coeffs);
                let p = self.processed[ch].process(processed[ch][s], &self.coeffs);
                self.chunk_ref += r * r;
                self.chunk_proc += p * p;
            }
            self.fill += 1;
            if self.fill >= self.chunk_len {
                self.finish_chunk();
            }
        }
    }

    fn finish_chunk(&mut self) {
        let ref_ms = self.chunk_ref / self.fill as f64;
        let proc_ms = self.chunk_proc / self.fill as f64;
        self.chunk_ref = 0.0;
        self.chunk_proc = 0.0;
        self.fill = 0;
        // absolute gate on the reference: silence says nothing about the processing gain
        if lufs(ref_ms) < -70.0 {
            return;
        }
        self.ref_ring[self.pos] = ref_ms;
        self.proc_ring[self.pos] = proc_ms;
        self.pos = (self.pos + 1) % WINDOW;
        self.count = (self.count + 1).min(WINDOW);
        let (mut ref_sum, mut proc_sum) = (0.0, 0.0);
        for i in 0..self.count {
            ref_sum += self.ref_ring[i];
            proc_sum += self.proc_ring[i];
        }
        let ref_lufs = lufs(ref_sum / self.count as f64) as f32;
        let proc_lufs = lufs(proc_sum / self.count as f64) as f32;
        self.target_db = (ref_lufs - proc_lufs).clamp(-24.0, 24.0);
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn harmonics_at_zero_are_unity() {
        let mut h = Harmonics::default();
        h.prepare(96000.0);
        for i in 0..1000 {
            let x = (i as f32 * 0.01).sin() * 1.5;
            assert_eq!(h.process(x, [0.0; 4], 0), x);
        }
    }

    #[test]
    fn rail_integral_matches_the_closed_form_far_out() {
        let t = rail();
        // value(u) -> 1 for large u, so the integral grows by (b - a) past the table
        assert!((t.integral(9.0) - t.integral(8.0) - 1.0).abs() < 1e-12);
        assert!((t.integral(0.5) - 0.125).abs() < 1e-3, "{}", t.integral(0.5));
    }

    #[test]
    fn every_model_reduces_a_loud_tone() {
        for mode in 0..MODE_COUNT {
            let mut e = Engine::default();
            e.prepare(96000.0);
            let (attack, release, knee) = mode_defaults(mode);
            let p = CompParams { threshold_db: -30.0, ratio: 8.0, attack_ms: attack, release_ms: release, knee_db: knee, ..CompParams::default() };
            e.update(mode, &p, 150.0);
            let mut g = 1.0;
            for i in 0..96000 {
                g = e.gain((i as f32 * 0.03).sin().abs() * 0.9);
            }
            assert!(g < 0.7 && g.is_finite(), "mode {mode}: gain {g}");
        }
    }

    #[test]
    fn smoothing_matches_juce() {
        let mut s = Smoothed::new(0.0, false);
        s.reset(1000.0, 0.01); // 10 steps
        s.set_target(1.0);
        let v: Vec<f32> = (0..11).map(|_| s.next_value()).collect();
        assert!((v[0] - 0.1).abs() < 1e-6 && v[9] == 1.0 && v[10] == 1.0 && !s.is_smoothing());
        let mut m = Smoothed::new(1.0, true);
        m.reset(1000.0, 0.01);
        m.set_target(4.0);
        for _ in 0..5 {
            m.next_value();
        }
        assert!((m.current() - 2.0).abs() < 1e-5, "{}", m.current());
    }
}
