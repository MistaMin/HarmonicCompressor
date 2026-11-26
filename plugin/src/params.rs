//! The parameter table of Harmonic Compressor 1.5.3, in its order, with its string ids, ranges,
//! skews, defaults and value text, so its sessions and automation keep working:
//!
//! - ids: the string ids in AAX (as before); in CLAP, VST3 and Audio Unit 1.5.3's hash of the
//!   string id (CLAP the full 32 bits, VST3 and Audio Unit the low 31), read from the 1.5.3 bundles
//!   with `openplugin-port scan` (qa/2026-11-26-rust-port/scan);
//! - values: 1.5.3 showed hosts 0..1 travel for every continuous parameter (CLAP also for choices),
//!   so those are shown normalized there; switches were 0/1 everywhere.

use openplugin::core::{HostSpace, Skew};
use openplugin::prelude::*;

pub const COUNT: usize = 27;

/// Indices into the table (and into the plugin's value array).
pub mod idx {
    pub const MODE: usize = 0;
    pub const THRESHOLD: usize = 1;
    pub const RATIO: usize = 2;
    pub const ATTACK: usize = 3;
    pub const RELEASE: usize = 4;
    pub const KNEE: usize = 5;
    pub const MAKEUP: usize = 6;
    pub const MIX: usize = 7;
    pub const H2: usize = 8;
    pub const H3: usize = 9;
    pub const H4: usize = 10;
    pub const H5: usize = 11;
    pub const SC_ENABLED: usize = 12;
    pub const SC_LISTEN: usize = 13;
    pub const SC_HP_FREQ: usize = 14;
    pub const AUTO_RELEASE: usize = 15;
    pub const AUTO_ATTACK: usize = 16;
    pub const INPUT_GAIN: usize = 17;
    pub const OUTPUT_GAIN: usize = 18;
    pub const OVERSAMPLE_MODE: usize = 19;
    pub const OVERSAMPLE_FILTER: usize = 20;
    pub const TRANSIENT_ENABLED: usize = 21;
    pub const TRANSIENT_ATTACK: usize = 22;
    pub const TRANSIENT_SUSTAIN: usize = 23;
    pub const TRANSIENT_SPEED: usize = 24;
    pub const AUTO_GAIN: usize = 25;
    pub const DELTA: usize = 26;
}

pub const MODES: &[&str] = &["Red", "Hybrid", "Bridge", "Bus VCA", "76", "Opto Leveler", "VCA 160"];

/// 1.5.3's hash of a parameter's string id: h = 31 h + c over the characters.
pub const fn host_hash(id: &str) -> u32 {
    let b = id.as_bytes();
    let mut h: u32 = 0;
    let mut i = 0;
    while i < b.len() {
        h = h.wrapping_mul(31).wrapping_add(b[i] as u32);
        i += 1;
    }
    h
}

// 1.5.3's text: the number with the decimals of the parameter's interval, no unit.
fn text0(v: f32) -> String {
    format!("{v:.0}")
}
fn text1(v: f32) -> String {
    format!("{v:.1}")
}
fn text2(v: f32) -> String {
    format!("{v:.2}")
}
/// The leading number of the text, as 1.5.3 read typed values ("-18 dB" is -18).
fn parse(t: &str) -> Option<f32> {
    let t = t.trim();
    let end = t.char_indices().find(|&(i, c)| !(c.is_ascii_digit() || c == '.' || ((c == '-' || c == '+') && i == 0))).map_or(t.len(), |(i, _)| i);
    t[..end].parse().ok()
}

const fn pin(p: ParamInfo) -> ParamInfo {
    let hash = host_hash(p.id);
    p.with_clap_id(hash).with_vst3_id(hash & 0x7fff_ffff).with_au_id(hash & 0x7fff_ffff)
}

#[allow(clippy::too_many_arguments)] // one row of the table per call reads best as positional
const fn float(id: &'static str, name: &'static str, min: f32, max: f32, default: f32, unit: Unit, decimals: u8, skew: f32) -> ParamInfo {
    let text: fn(f32) -> String = match decimals {
        0 => text0,
        1 => text1,
        _ => text2,
    };
    let p = ParamInfo::new(id, name, min, max, default, unit).with_text(text, parse).with_normalized_in(HostSpace::CLAP.union(HostSpace::SHELLS));
    pin(if skew == 1.0 { p } else { p.with_taper(Taper::Skew(Skew::new(skew))) })
}

const fn choice(id: &'static str, name: &'static str, choices: &'static [&'static str], default: f32) -> ParamInfo {
    pin(ParamInfo::new(id, name, 0.0, (choices.len() - 1) as f32, default, Unit::Choice).with_choices(choices).with_normalized_in(HostSpace::CLAP))
}

const fn toggle(id: &'static str, name: &'static str) -> ParamInfo {
    pin(ParamInfo::new(id, name, 0.0, 1.0, 0.0, Unit::Toggle))
}

pub static PARAMS: [ParamInfo; COUNT] = [
    choice("algorithmMode", "Algorithm", MODES, 0.0),
    float("threshold", "Threshold", -60.0, 0.0, -18.0, Unit::Db, 1, 1.0),
    float("ratio", "Ratio", -2.0, 100.0, 4.0, Unit::None, 1, 0.5),
    float("attack", "Attack", 0.1, 300.0, 10.0, Unit::Ms, 1, 0.3),
    float("release", "Release", 5.0, 3000.0, 100.0, Unit::Ms, 0, 0.3),
    float("knee", "Knee", 0.0, 24.0, 6.0, Unit::Db, 1, 1.0),
    float("makeupGain", "Makeup Gain", -12.0, 24.0, 0.0, Unit::Db, 1, 1.0),
    float("mix", "Mix", 0.0, 1.0, 1.0, Unit::None, 2, 1.0),
    float("harmonic2", "2nd Harmonic", 0.0, 100.0, 0.0, Unit::Percent, 1, 1.0),
    float("harmonic3", "3rd Harmonic", 0.0, 100.0, 0.0, Unit::Percent, 1, 1.0),
    float("harmonic4", "4th Harmonic", 0.0, 100.0, 0.0, Unit::Percent, 1, 1.0),
    float("harmonic5", "5th Harmonic", 0.0, 100.0, 0.0, Unit::Percent, 1, 1.0),
    toggle("scEnabled", "Sidechain Enable"),
    toggle("scListen", "Sidechain Listen"),
    float("scHPFreq", "Sidechain HP Freq", 20.0, 500.0, 150.0, Unit::Hz, 0, 0.3),
    toggle("autoRelease", "Auto Release"),
    toggle("autoAttack", "Auto Attack"),
    // 1.5
    float("inputGain", "Input Gain", -24.0, 24.0, 0.0, Unit::Db, 1, 1.0),
    float("outputGain", "Output Gain", -24.0, 24.0, 0.0, Unit::Db, 1, 1.0),
    choice("oversampleMode", "Oversample", &["1x", "2x", "4x", "8x"], 1.0),
    choice("oversampleFilter", "Oversample Filter", &["Min Phase", "Lin Phase"], 0.0),
    toggle("transientEnabled", "Transient Enable"),
    float("transientAttack", "Transient Attack", -100.0, 100.0, 0.0, Unit::Percent, 1, 1.0),
    float("transientSustain", "Transient Sustain", -100.0, 100.0, 0.0, Unit::Percent, 1, 1.0),
    float("transientSpeed", "Transient Speed", 0.5, 50.0, 10.0, Unit::Ms, 1, 0.4),
    // 1.5.2
    toggle("autoGain", "Auto Gain"),
    toggle("delta", "Delta Monitor"),
];

/// 1.5.3's interval of each parameter (its text shows that many decimals).
static INTERVAL: [f32; COUNT] = [
    1.0, 0.1, 0.1, 0.1, 1.0, 0.1, 0.1, 0.01, 0.1, 0.1, 0.1, 0.1, 1.0, 1.0, 1.0, 1.0, 1.0, 0.1, 0.1, 1.0, 1.0, 1.0, 0.1, 0.1, 0.1, 1.0, 1.0,
];

/// The value 1.5.3's engine saw for a value set by a host or a state: clamped, NaN rejected, and
/// snapped to the interval (JUCE's `snapToLegalValue`). `index` must be below `COUNT`.
pub fn legal(index: usize, value: f32) -> f32 {
    let p = &PARAMS[index];
    let step = INTERVAL[index];
    let v = p.min + step * ((p.clamp(value) - p.min) / step + 0.5).floor();
    if v <= p.min { p.min } else { v.min(p.max) }
}

pub fn index_of(id: &str) -> Option<usize> {
    PARAMS.iter().position(|p| p.id == id)
}

pub fn defaults() -> [f32; COUNT] {
    std::array::from_fn(|i| PARAMS[i].default)
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::collections::HashMap;

    fn manifest(format: &str) -> serde_json::Value {
        let path = format!("{}/../qa/2026-11-26-rust-port/scan/harmonic-compressor.{format}.manifest.json", env!("CARGO_MANIFEST_DIR"));
        serde_json::from_str(&std::fs::read_to_string(path).expect("1.5.3 scan")).expect("json")
    }

    /// Every id, default and value text the 1.5.3 bundles reported (qa/2026-11-26-rust-port/scan).
    #[test]
    fn identity_and_travel_are_those_of_1_5_3() {
        assert_eq!(openplugin::core::identity::check_pins(&crate::HarmonicCompressor::default()), []);
        for (format, mask, space) in [("clap", u32::MAX, HostSpace::CLAP), ("vst3", 0x7fff_ffff, HostSpace::SHELLS), ("au", 0x7fff_ffff, HostSpace::SHELLS)] {
            let m = manifest(format);
            let scanned: HashMap<u64, &serde_json::Value> = m["plugin"]["params"].as_array().unwrap().iter().map(|p| (p["id"].as_u64().unwrap(), p)).collect();
            for p in &PARAMS {
                let s = scanned.get(&u64::from(host_hash(p.id) & mask)).unwrap_or_else(|| panic!("{format}: {} missing", p.id));
                assert_eq!(s["name"], p.name, "{format}");
                // VST3 automation is normalized for every parameter, whatever the host space
                let vst3 = format == "vst3";
                let (min, max, def) = if vst3 { (0.0, 1.0, p.to_normalized(p.default)) } else { p.host_range(space) };
                let (smin, smax, sdef) = (s["min"].as_f64().unwrap() as f32, s["max"].as_f64().unwrap() as f32, s["default"].as_f64().unwrap() as f32);
                // the AU shows choices as plain indices and every other parameter as 0..1
                assert!((min - smin).abs() < 1e-6 && (max - smax).abs() < 1e-6, "{format} {}: {min}..{max} vs {smin}..{smax}", p.id);
                assert!((def - sdef).abs() < 1e-4, "{format} {}: default {def} vs {sdef}", p.id);
                if p.unit == Unit::Choice || p.unit == Unit::Toggle {
                    continue;
                }
                for sample in s["samples"].as_array().unwrap() {
                    let v = sample["plain"].as_f64().unwrap() as f32;
                    let plain = if vst3 { p.from_normalized(v) } else { p.from_host(space, v) };
                    assert_eq!(p.format(plain), sample["text"].as_str().unwrap(), "{format} {}", p.id);
                }
            }
        }
    }

    #[test]
    fn typed_text_reads_the_leading_number() {
        assert_eq!(parse("-18 dB"), Some(-18.0));
        assert_eq!(parse(" 4.5"), Some(4.5));
        assert_eq!(parse("dB"), None);
    }
}
