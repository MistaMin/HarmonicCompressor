//! Every parameter changes the sound (plugin-qa rule 2, the audio leg). The null against 1.5.3
//! covers most at 1x; this also covers 2x to 8x and the oversampling filter.

use harmonic_compressor::HarmonicCompressor;
use harmonic_compressor::params::{PARAMS, idx};
use openplugin::prelude::*;

const SR: f32 = 48000.0;
const BLOCK: usize = 512;
const BLOCKS: usize = 280; // about 3 s; the last 2 s are compared

/// Loud noise in bursts, so the compressor, transient designer and loudness match all move.
fn render(settings: &[(usize, f32)]) -> (Vec<f32>, usize) {
    let mut plugin = HarmonicCompressor::default();
    for &(i, v) in settings {
        assert!(plugin.set_param_index(i, v));
    }
    plugin.prepare(SR, BLOCK, 2);
    let mut seed = 0x1234_5678u32;
    let mut out = Vec::with_capacity(BLOCK * BLOCKS);
    for b in 0..BLOCKS {
        let gain = if (b / 20) % 2 == 0 { 0.7 } else { 0.05 };
        let mut left = vec![0.0f32; BLOCK];
        for s in left.iter_mut() {
            seed = seed.wrapping_mul(1_664_525).wrapping_add(1_013_904_223);
            *s = ((seed >> 8) as f32 / (1u32 << 24) as f32 * 2.0 - 1.0) * gain;
        }
        let right = left.iter().map(|x| x * 0.8).collect();
        let mut buffers = [left, right];
        let mut io: Vec<&mut [f32]> = buffers.iter_mut().map(|c| c.as_mut_slice()).collect();
        plugin.process(&mut io, BLOCK);
        out.extend_from_slice(&buffers[0]);
    }
    (out, plugin.latency())
}

/// RMS of the latency-aligned difference over the RMS of the reference, in dB.
fn difference_db(a: &(Vec<f32>, usize), b: &(Vec<f32>, usize)) -> f64 {
    let start = SR as usize;
    let len = a.0.len() - start - a.1.max(b.1);
    let (mut d, mut r) = (0.0f64, 0.0f64);
    for n in start..start + len {
        let (x, y) = (a.0[n + a.1] as f64, b.0[n + b.1] as f64);
        d += (x - y) * (x - y);
        r += x * x;
    }
    10.0 * (d / r.max(1e-30)).log10()
}

#[test]
fn every_parameter_changes_the_output() {
    // Parameters that act only when another one is on.
    let needs = |i: usize| -> Vec<(usize, f32)> {
        match i {
            idx::SC_LISTEN | idx::SC_HP_FREQ => vec![(idx::SC_ENABLED, 1.0)],
            idx::TRANSIENT_ATTACK | idx::TRANSIENT_SUSTAIN => vec![(idx::TRANSIENT_ENABLED, 1.0)],
            // Enable alone is silent by design: Attack and Sustain default to 0 % (0 dB of gain).
            idx::TRANSIENT_ENABLED => vec![(idx::TRANSIENT_ATTACK, PARAMS[idx::TRANSIENT_ATTACK].max)],
            idx::TRANSIENT_SPEED => vec![(idx::TRANSIENT_ENABLED, 1.0), (idx::TRANSIENT_ATTACK, PARAMS[idx::TRANSIENT_ATTACK].max)],
            _ => vec![],
        }
    };
    let mut dead = Vec::new();
    for (i, p) in PARAMS.iter().enumerate() {
        let far = if p.max - p.default >= p.default - p.min { p.max } else { p.min };
        let base = needs(i);
        let mut moved = base.clone();
        moved.push((i, far));
        let db = difference_db(&render(&base), &render(&moved));
        println!("{:<18} {} -> {}: difference {db:.1} dB", p.id, p.default, far);
        if db.is_nan() || db < -60.0 {
            dead.push(p.id);
        }
    }
    assert!(dead.is_empty(), "parameters that do not change the output: {dead:?}");
}
