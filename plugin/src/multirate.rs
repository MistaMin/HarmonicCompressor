//! Taken from HybridEQ 2.0 (plugin/src/multirate.rs), with clippy fixes in its tests.
//! 1x / 2x / 4x / 8x oversampling around the engine, with a minimum-phase ("IIR (Min)": low latency, no
//! pre-ringing) or a linear-phase ("FIR (Lin)": constant delay) anti-alias filter, as 1.6.x offered.
//! Each 2x stage is a polyphase FIR whose taps come from the SDK's designs (Kaiser low pass, and its
//! minimum-phase factoring). The first stage carries the steep transition; later ones are cheap.

use openplugin::dsp::{design_kaiser, fir_group_delay, FirKind, MinPhaseHalfband};

/// Passband edge as a share of the session rate; the stopband starts at its mirror image.
const PASSBAND: f64 = 0.45;
const STOPBAND_DB: f64 = 100.0;

/// History of the newest `k` samples, newest first, contiguous (two copies of the ring).
struct History {
    buf: Vec<f64>,
    pos: usize,
    k: usize,
}

impl History {
    fn new(k: usize) -> Self {
        Self { buf: vec![0.0; 2 * k], pos: 0, k }
    }
    fn clear(&mut self) {
        self.buf.fill(0.0);
        self.pos = 0;
    }
    #[inline]
    fn push(&mut self, x: f64) {
        self.pos = if self.pos == 0 { self.k - 1 } else { self.pos - 1 };
        self.buf[self.pos] = x;
        self.buf[self.pos + self.k] = x;
    }
    #[inline]
    fn dot(&self, taps: &[f64]) -> f64 {
        self.buf[self.pos..self.pos + self.k].iter().zip(taps).map(|(a, b)| a * b).sum()
    }
}

/// One 2x stage: polyphase interpolation (gain 2) and decimation with the same taps.
struct Stage {
    even: Vec<f64>,
    odd: Vec<f64>,
    up: History,
    down_even: History,
    down_odd: History,
}

impl Stage {
    fn new(taps: &[f64]) -> Self {
        let k = taps.len().div_ceil(2);
        let mut even = vec![0.0; k];
        let mut odd = vec![0.0; k];
        for (i, &t) in taps.iter().enumerate() {
            if i % 2 == 0 {
                even[i / 2] = t;
            } else {
                odd[i / 2] = t;
            }
        }
        Self { even, odd, up: History::new(k), down_even: History::new(k), down_odd: History::new(k) }
    }

    fn reset(&mut self) {
        self.up.clear();
        self.down_even.clear();
        self.down_odd.clear();
    }

    fn upsample(&mut self, input: &[f32], output: &mut [f32]) {
        for (i, &x) in input.iter().enumerate() {
            self.up.push(x as f64);
            output[2 * i] = (2.0 * self.up.dot(&self.even)) as f32;
            output[2 * i + 1] = (2.0 * self.up.dot(&self.odd)) as f32;
        }
    }

    fn downsample(&mut self, input: &[f32], output: &mut [f32]) {
        for (i, y) in output.iter_mut().enumerate() {
            // y[n] = sum h[k] x[2n - k]: even taps on even samples, odd taps on the sample before
            self.down_even.push(input[2 * i] as f64);
            let e = self.down_even.dot(&self.even);
            let o = self.down_odd.dot(&self.odd);
            self.down_odd.push(input[2 * i + 1] as f64);
            *y = (e + o) as f32;
        }
    }
}

/// The stages for one factor and filter kind, per channel.
struct Chain {
    stages: Vec<[Stage; 2]>,
    latency: usize,
}

fn design(session_rate: f64, stage: usize, linear: bool) -> (Vec<f64>, f64) {
    let rate = session_rate * (1 << stage) as f64; // this stage's input rate
    let pass = PASSBAND * session_rate;
    if linear {
        let taps: Vec<f64> = design_kaiser(FirKind::LowPass { cutoff_hz: rate / 2.0 }, 2.0 * rate, STOPBAND_DB, rate - 2.0 * pass)
            .expect("valid half-band design")
            .into_iter()
            .map(f64::from)
            .collect();
        // linear phase: (N - 1) / 2 at the 2x rate, one way
        let delay = (taps.len() - 1) as f64 / 2.0;
        (taps, delay)
    } else {
        let h = MinPhaseHalfband::design(rate, pass, STOPBAND_DB).expect("valid half-band design");
        let taps: Vec<f64> = h.taps().iter().map(|&v| v as f64).collect();
        // group delay at low frequencies, at the 2x rate, one way
        let delay = fir_group_delay(h.taps(), 100.0 / (2.0 * rate));
        (taps, delay)
    }
}

impl Chain {
    fn new(session_rate: f64, factor: usize, linear: bool) -> Self {
        let n = factor.trailing_zeros() as usize;
        let mut stages = Vec::with_capacity(n);
        let mut latency = 0.0;
        for s in 0..n {
            let (taps, delay) = design(session_rate, s, linear);
            // one way at the 2x rate of stage s, there and back, in session samples
            latency += delay / (1 << (s + 1)) as f64 * 2.0;
            stages.push([Stage::new(&taps), Stage::new(&taps)]);
        }
        Self { stages, latency: latency.round() as usize }
    }

    fn reset(&mut self) {
        for pair in &mut self.stages {
            for s in pair {
                s.reset();
            }
        }
    }
}

pub struct Multirate {
    /// [factor index 1..=3][linear]
    chains: Vec<[Chain; 2]>,
    /// per channel: working buffers for the up and down path, sized for 8x
    bufs: [[Vec<f32>; 2]; 2],
    factor: usize,
    linear: bool,
}

impl Multirate {
    /// Designs every factor and filter (allocates) for blocks of up to `max_block` session samples.
    pub fn new(session_rate: f64, max_block: usize) -> Self {
        let chains = [2usize, 4, 8].iter().map(|&f| [Chain::new(session_rate, f, false), Chain::new(session_rate, f, true)]).collect();
        let buf = || [vec![0.0f32; max_block * 8], vec![0.0f32; max_block * 8]];
        Self { chains, bufs: [buf(), buf()], factor: 1, linear: false }
    }

    /// Selects a factor and filter; a change resets the filters. Returns true when it changed.
    pub fn set(&mut self, factor: usize, linear: bool) -> bool {
        if factor == self.factor && linear == self.linear {
            return false;
        }
        self.factor = factor;
        self.linear = linear;
        if let Some(c) = self.chain_mut() {
            c.reset();
        }
        true
    }

    pub fn reset(&mut self) {
        for pair in &mut self.chains {
            for c in pair {
                c.reset();
            }
        }
    }

    /// Reported latency in session samples for a factor and filter.
    pub fn latency_for(&self, factor: usize, linear: bool) -> usize {
        match factor {
            2 => self.chains[0][linear as usize].latency,
            4 => self.chains[1][linear as usize].latency,
            8 => self.chains[2][linear as usize].latency,
            _ => 0,
        }
    }

    fn chain_mut(&mut self) -> Option<&mut Chain> {
        let i = match self.factor {
            2 => 0,
            4 => 1,
            8 => 2,
            _ => return None,
        };
        Some(&mut self.chains[i][self.linear as usize])
    }

    /// Runs `core` on `io` (up to two channels of `frames` session samples) at the selected rate.
    pub fn process(&mut self, io: &mut [&mut [f32]], frames: usize, core: impl FnOnce(&mut [&mut [f32]])) {
        let channels = io.len().min(2);
        if channels == 0 {
            return;
        }
        let i = match self.factor {
            2 => 0,
            4 => 1,
            8 => 2,
            _ => {
                let (a, b) = io.split_at_mut(1);
                match channels {
                    2 => core(&mut [&mut a[0][..frames], &mut b[0][..frames]]),
                    _ => core(&mut [&mut a[0][..frames]]),
                }
                return;
            }
        };
        let chain = &mut self.chains[i][self.linear as usize];
        let stages = chain.stages.len();
        // Up stages write x, y, x: the top rate ends in x for an odd count, y for an even one.
        let top_in_x = stages % 2 == 1;
        let n_top = frames << stages;
        for ch in 0..channels {
            let [x, y] = &mut self.bufs[ch];
            let mut n = frames;
            for (s, pair) in chain.stages.iter_mut().enumerate() {
                match s {
                    0 => pair[ch].upsample(&io[ch][..n], &mut x[..2 * n]),
                    s if s % 2 == 1 => pair[ch].upsample(&x[..n], &mut y[..2 * n]),
                    _ => pair[ch].upsample(&y[..n], &mut x[..2 * n]),
                }
                n *= 2;
            }
        }
        {
            let [b0, b1] = &mut self.bufs;
            let t = usize::from(!top_in_x);
            if channels == 2 {
                core(&mut [&mut b0[t][..n_top], &mut b1[t][..n_top]]);
            } else {
                core(&mut [&mut b0[t][..n_top]]);
            }
        }
        // Down: back through the stages in reverse, alternating buffers, the last into `io`.
        for ch in 0..channels {
            let [x, y] = &mut self.bufs[ch];
            let mut n = n_top;
            let mut in_x = top_in_x;
            for (s, pair) in chain.stages.iter_mut().enumerate().rev() {
                match (s, in_x) {
                    (0, true) => pair[ch].downsample(&x[..n], &mut io[ch][..n / 2]),
                    (0, false) => pair[ch].downsample(&y[..n], &mut io[ch][..n / 2]),
                    (_, true) => pair[ch].downsample(&x[..n], &mut y[..n / 2]),
                    (_, false) => pair[ch].downsample(&y[..n], &mut x[..n / 2]),
                }
                in_x = !in_x;
                n /= 2;
            }
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn tone(f: f64, rate: f64, n: usize) -> f32 {
        (2.0 * std::f64::consts::PI * f * n as f64 / rate).sin() as f32
    }

    #[test]
    fn passband_is_transparent() {
        for factor in [2, 4, 8] {
            for linear in [false, true] {
                for f in [100.0, 1000.0, 10000.0, 20000.0] {
                    let mut m = Multirate::new(48000.0, 512);
                    m.set(factor, linear);
                    let (mut sin, mut sout) = (0.0f64, 0.0f64);
                    for b in 0..60 {
                        let block: Vec<f32> = (0..512).map(|i| 0.5 * tone(f, 48000.0, b * 512 + i)).collect();
                        let mut io = [block.clone(), block.clone()];
                        m.process(&mut io.iter_mut().map(Vec::as_mut_slice).collect::<Vec<_>>(), 512, |_| {});
                        if b >= 20 {
                            sin += block.iter().map(|x| (*x as f64).powi(2)).sum::<f64>();
                            sout += io[0].iter().map(|x| (*x as f64).powi(2)).sum::<f64>();
                        }
                    }
                    let db = 10.0 * (sout / sin).log10();
                    assert!(db.abs() < 0.05, "factor {factor} linear {linear} {f} Hz: {db:.3} dB");
                }
            }
        }
    }

    #[test]
    fn content_above_the_session_nyquist_does_not_fold_back() {
        for factor in [2usize, 4, 8] {
            for linear in [false, true] {
                let mut m = Multirate::new(48000.0, 512);
                m.set(factor, linear);
                let rate = 48000.0 * factor as f64;
                let mut peak = 0.0f32;
                for b in 0..100 {
                    let mut io = [vec![0.0f32; 512], vec![0.0f32; 512]];
                    m.process(&mut io.iter_mut().map(Vec::as_mut_slice).collect::<Vec<_>>(), 512, |ch| {
                        for data in ch.iter_mut() {
                            for (i, x) in data.iter_mut().enumerate() {
                                *x = tone(30000.0, rate, b * 512 * factor + i);
                            }
                        }
                    });
                    if b > 10 {
                        peak = io[0].iter().fold(peak, |p, x| p.max(x.abs()));
                    }
                }
                assert!(peak < 3e-5, "factor {factor} linear {linear}: alias {:.1} dB", 20.0 * peak.log10());
            }
        }
    }

    #[test]
    fn linear_phase_latency_is_the_impulse_delay() {
        for factor in [2, 4, 8] {
            let mut m = Multirate::new(48000.0, 512);
            m.set(factor, true);
            let mut io = [vec![0.0f32; 512], vec![0.0f32; 512]];
            io[0][0] = 1.0;
            m.process(&mut io.iter_mut().map(Vec::as_mut_slice).collect::<Vec<_>>(), 512, |_| {});
            let at = io[0].iter().enumerate().fold((0, 0.0f32), |b, (i, x)| if x.abs() > b.1 { (i, x.abs()) } else { b }).0;
            assert!(at.abs_diff(m.latency_for(factor, true)) <= 1, "factor {factor}: peak at {at}, reported {}", m.latency_for(factor, true));
        }
    }
}
