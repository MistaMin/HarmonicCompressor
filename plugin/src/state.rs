//! Plugin state: this version's own chunk, and the state 1.5.x saved.
//!
//! Own chunk: `HCMP`, a version byte, then `id=value` lines (plain units) and `@<mode>:id=value`
//! lines for each model's remembered settings, so a later table can add, drop or reorder parameters.
//!
//! 1.5.x saved `VC2!`, a 32-bit size and an XML document, `<Parameters><PARAM id=".." value=".."/>...
//! <ModeSnapshots><Mode threshold=".." .../>...</ModeSnapshots></Parameters>`, with plain values. CLAP
//! hands that blob as it is; the VST3 component state and the AAX chunk wrap it in a short header; the
//! Audio Unit keeps it under `jucePluginState` in its ClassInfo dictionary. Parameters the document
//! does not name (Auto Gain and Delta before 1.5.2) keep their defaults, and so do the remembered
//! settings of sessions from before 1.5.3.

use crate::dsp::{MODE_COUNT, mode_defaults};
use crate::params::{COUNT, PARAMS, defaults, idx, index_of, saved};
use openplugin::core::foreign_state::{plist, xml};

const MAGIC: &[u8; 4] = b"HCMP";
const VERSION: u8 = 1;

/// The parameters each model remembers, in 1.5.3's order (`snapshotIds`).
pub const SNAP: [usize; 16] = [
    idx::THRESHOLD,
    idx::RATIO,
    idx::ATTACK,
    idx::RELEASE,
    idx::KNEE,
    idx::MAKEUP,
    idx::MIX,
    idx::H2,
    idx::H3,
    idx::H4,
    idx::H5,
    idx::SC_HP_FREQ,
    idx::SC_ENABLED,
    idx::SC_LISTEN,
    idx::AUTO_RELEASE,
    idx::AUTO_ATTACK,
];

pub type Snapshots = [[f32; SNAP.len()]; MODE_COUNT];

/// The parameter defaults with each model's own attack, release and knee.
pub fn default_snapshots() -> Snapshots {
    std::array::from_fn(|m| {
        let (attack, release, knee) = mode_defaults(m);
        let mut s: [f32; SNAP.len()] = std::array::from_fn(|k| PARAMS[SNAP[k]].default);
        s[2] = attack;
        s[3] = release;
        s[4] = knee;
        s
    })
}

/// The editor's look, kept with the session as 1.5.3 kept it in its state's properties: colour mode,
/// curve shown, editor width and where the four modules were moved.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct UiState {
    pub colour_mode: i32,
    pub show_curve: bool,
    pub editor_width: i32,
    pub modules: [(i32, i32); 4],
    /// The meters' scale: `METER_PPM`, `METER_RMS` or `METER_VU` (new in the port; PPM by default).
    pub meter: i32,
}

impl Default for UiState {
    fn default() -> Self {
        UiState { colour_mode: 0, show_curve: true, editor_width: 1120, modules: [(68, 12), (274, 12), (482, 12), (676, 12)], meter: 0 }
    }
}

const MODULE_KEYS: [(&str, &str); 4] = [("modCompX", "modCompY"), ("modHarmX", "modHarmY"), ("modScX", "modScY"), ("modTransX", "modTransY")];

impl UiState {
    /// 1.5.3's property names and values.
    fn properties(&self) -> Vec<(&'static str, i32)> {
        let mut out = vec![("showCurve", i32::from(self.show_curve)), ("colourMode", self.colour_mode), ("editorWidth", self.editor_width), ("meterMode", self.meter)];
        for (m, (kx, ky)) in MODULE_KEYS.iter().enumerate() {
            out.push((kx, self.modules[m].0));
            out.push((ky, self.modules[m].1));
        }
        out
    }

    /// One property; unknown names and unreadable values are ignored, as 1.5.3 fell back to defaults.
    pub fn set(&mut self, key: &str, value: &str) {
        let Some(v) = value.trim().parse::<f64>().ok().filter(|v| v.is_finite()).map(|v| v as i32) else { return };
        match key {
            "showCurve" => self.show_curve = v != 0,
            "colourMode" => self.colour_mode = v.clamp(0, 3),
            "editorWidth" => self.editor_width = v,
            "meterMode" => self.meter = v.clamp(0, 2),
            _ => {
                for (m, (kx, ky)) in MODULE_KEYS.iter().enumerate() {
                    if key == *kx {
                        self.modules[m].0 = v;
                    } else if key == *ky {
                        self.modules[m].1 = v;
                    }
                }
            }
        }
    }
}

pub struct State {
    pub values: [f32; COUNT],
    pub snapshots: Snapshots,
    pub ui: UiState,
}

pub fn encode(values: &[f32; COUNT], snapshots: &Snapshots, ui: &UiState) -> Vec<u8> {
    let mut out = MAGIC.to_vec();
    out.push(VERSION);
    for (p, v) in PARAMS.iter().zip(values) {
        out.extend_from_slice(format!("{}={v}\n", p.id).as_bytes());
    }
    for (m, snap) in snapshots.iter().enumerate() {
        for (k, v) in snap.iter().enumerate() {
            out.extend_from_slice(format!("@{m}:{}={v}\n", PARAMS[SNAP[k]].id).as_bytes());
        }
    }
    for (key, v) in ui.properties() {
        out.extend_from_slice(format!("ui.{key}={v}\n").as_bytes());
    }
    out
}

/// Every value from this version's chunk or 1.5.x's blob, or `None` when the bytes are neither or
/// damaged (nothing is applied then).
pub fn decode(data: &[u8]) -> Option<State> {
    match data.strip_prefix(MAGIC) {
        Some(body) => decode_own(body),
        None => decode_juce(data),
    }
}

fn finite(v: &str) -> Option<f32> {
    v.parse::<f32>().ok().filter(|v| v.is_finite())
}

fn decode_own(body: &[u8]) -> Option<State> {
    let (&version, text) = body.split_first()?;
    if version != VERSION {
        return None;
    }
    let mut state = State { values: defaults(), snapshots: default_snapshots(), ui: UiState::default() };
    for line in std::str::from_utf8(text).ok()?.lines() {
        let (key, v) = line.split_once('=')?;
        if let Some(prop) = key.strip_prefix("ui.") {
            state.ui.set(prop, v);
            continue;
        }
        let v = finite(v)?;
        if let Some(rest) = key.strip_prefix('@') {
            let (m, id) = rest.split_once(':')?;
            let m: usize = m.parse().ok()?;
            // a model or a remembered parameter this build does not have is dropped
            if let (Some(snap), Some(k)) = (state.snapshots.get_mut(m), SNAP.iter().position(|&i| PARAMS[i].id == id)) {
                snap[k] = v;
            }
        } else if let Some(i) = index_of(key) {
            state.values[i] = saved(i, v);
        }
    }
    Some(state)
}

/// 1.5.x's blob (`VC2!`, size, XML), found near the front of a CLAP, VST3 or AAX state.
pub fn decode_juce(data: &[u8]) -> Option<State> {
    let at = data.get(..data.len().min(256))?.windows(4).position(|w| w == b"VC2!")?;
    let rest = &data[at + 4..];
    let size = u32::from_le_bytes(rest.get(..4)?.try_into().ok()?) as usize;
    let doc = rest.get(4..4 + size)?;
    let end = doc.iter().rposition(|b| *b == b'>')?;
    let root = xml::parse(&doc[..=end]).ok()?;
    if root.name != "Parameters" {
        return None;
    }
    let mut state = State { values: defaults(), snapshots: default_snapshots(), ui: UiState::default() };
    for (key, _) in UiState::default().properties() {
        if let Some(v) = root.attr(key) {
            state.ui.set(key, v);
        }
    }
    let mut found = 0;
    for param in root.children_named("PARAM") {
        let (Some(id), Some(v)) = (param.attr("id"), param.attr("value").and_then(finite)) else {
            return None;
        };
        if let Some(i) = index_of(id) {
            state.values[i] = saved(i, v);
            found += 1;
        }
    }
    if let Some(modes) = root.child("ModeSnapshots") {
        for (snap, mode) in state.snapshots.iter_mut().zip(modes.children_named("Mode")) {
            for (k, &i) in SNAP.iter().enumerate() {
                if let Some(v) = mode.attr(PARAMS[i].id).and_then(finite) {
                    snap[k] = v;
                }
            }
        }
    }
    (found > 0).then_some(state)
}

/// 1.5.x's Audio Unit ClassInfo: the blob is under `jucePluginState`.
pub fn decode_au_class_info(data: &[u8]) -> Option<State> {
    decode_juce(plist::parse(data).ok()?.get("jucePluginState")?.as_data()?)
}

#[cfg(test)]
mod tests {
    use super::*;

    fn capture(path: &str) -> Vec<u8> {
        std::fs::read(format!("{}/../qa/2026-11-26-rust-port/capture/{path}", env!("CARGO_MANIFEST_DIR"))).expect("captured 1.5.3 state")
    }

    #[test]
    fn own_state_round_trips() {
        let mut v = defaults();
        // a threshold from a 1.5.3 session, off the whole-dB grid, and values on it
        v[idx::THRESHOLD] = saved(idx::THRESHOLD, -23.7);
        assert!((v[idx::THRESHOLD] + 23.7).abs() < 1e-5, "1.5.3's tenth is kept: {}", v[idx::THRESHOLD]);
        v[idx::KNEE] = 9.0;
        v[idx::MODE] = 5.0;
        v[idx::DELTA] = 1.0;
        let v: [f32; COUNT] = std::array::from_fn(|i| saved(i, v[i]));
        assert_eq!(saved(idx::THRESHOLD, v[idx::THRESHOLD]), v[idx::THRESHOLD], "snapping is idempotent");
        let mut s = default_snapshots();
        s[3][0] = -40.5;
        let ui = UiState { colour_mode: 2, show_curve: false, editor_width: 800, modules: [(16, 13), (436, 13), (16, 420), (419, 420)], meter: 2 };
        let bytes = encode(&v, &s, &ui);
        let back = decode(&bytes).expect("own state");
        assert_eq!(back.values, v);
        assert_eq!(back.snapshots, s);
        assert_eq!(back.ui, ui);
        assert_eq!(encode(&back.values, &back.snapshots, &back.ui), bytes, "resaved byte for byte");
        assert!(decode(b"junk").is_none());
        assert!(decode(b"HCMP\x09").is_none(), "an unknown version is refused");
        assert!(decode(b"HCMP\x01threshold=NaN\n").is_none(), "a non-number is refused");
    }

    /// The states 1.5.3 saved in every format (qa/2026-11-26-rust-port/capture): the capture set
    /// Threshold to 0.25 (-45 dB), Algorithm to 0.5 (Bus VCA) and Auto Gain on.
    #[test]
    fn states_of_1_5_3_load() {
        let clap = decode(&capture("harmonic-compressor.clap.blobs/001-set.clap-state.bin")).expect("CLAP state");
        assert_eq!(clap.values[idx::THRESHOLD], -45.0);
        assert_eq!(clap.values[idx::MODE], 3.0);
        assert_eq!(clap.values[idx::AUTO_GAIN], 1.0);
        assert_eq!(clap.values[idx::OVERSAMPLE_MODE], 1.0);
        // untouched per-model memory is 1.5.3's seed (its makeup default was saved as 1.8e-7)
        for (a, b) in clap.snapshots.iter().flatten().zip(default_snapshots().iter().flatten()) {
            assert!((a - b).abs() < 1e-5, "{a} vs {b}");
        }
        let vst3 = decode_juce(&capture("harmonic-compressor.vst3.blobs/001-set.vst3-component.bin")).expect("VST3 state");
        assert_eq!(vst3.values, clap.values);
        let au = decode_au_class_info(&capture("harmonic-compressor.au.blobs/001-set.au-classinfo.bin")).expect("AU state");
        // the Audio Unit shows Algorithm as a plain index, so the capture's 0.5 chose Red there
        assert_eq!(au.values[idx::MODE], 0.0);
        let mut expected = clap.values;
        expected[idx::MODE] = 0.0;
        assert_eq!(au.values, expected);
        let def = decode(&capture("harmonic-compressor.clap.blobs/000-default.clap-state.bin")).expect("default");
        for (i, (a, b)) in def.values.iter().zip(defaults()).enumerate() {
            assert!((a - b).abs() <= 1e-5 * b.abs().max(1.0), "{}: {a} vs {b}", PARAMS[i].id);
        }
    }

    #[test]
    fn damaged_states_are_refused_without_a_panic() {
        let set = capture("harmonic-compressor.clap.blobs/001-set.clap-state.bin");
        for n in 0..set.len() {
            let _ = decode(&set[..n]);
        }
        assert!(decode(&set[..set.len() / 2]).is_none());
        let mut bad = set.clone();
        bad[4] = 0xff; // size beyond the data
        assert!(decode(&bad).is_none());
        let doc = String::from_utf8_lossy(&set).replace("value=\"-45.0\"", "value=\"x\"");
        assert!(decode(doc.as_bytes()).is_none(), "an unreadable value refuses the whole state");
    }

    /// 1.5.3's editor kept its look as properties of the `Parameters` element.
    #[test]
    fn the_editor_look_loads_from_1_5_3() {
        let set = capture("harmonic-compressor.clap.blobs/001-set.clap-state.bin");
        let doc = String::from_utf8_lossy(&set).replacen(
            "<Parameters>",
            "<Parameters showCurve=\"0\" colourMode=\"2\" editorWidth=\"800\" modHarmX=\"436\" modHarmY=\"13\">",
            1,
        );
        let mut bytes = doc.into_bytes();
        let size = (bytes.len() - 8) as u32;
        bytes[4..8].copy_from_slice(&size.to_le_bytes());
        let s = decode(&bytes).expect("state with the editor's look");
        assert_eq!(s.ui.colour_mode, 2);
        assert!(!s.ui.show_curve);
        assert_eq!(s.ui.editor_width, 800);
        assert_eq!(s.ui.modules[1], (436, 13));
        assert_eq!(s.ui.modules[0], UiState::default().modules[0], "a module 1.5.3 never moved keeps its place");
        assert_eq!(s.values[idx::THRESHOLD], -45.0);
    }

    #[test]
    fn remembered_settings_load_from_1_5_3() {
        let set = capture("harmonic-compressor.clap.blobs/001-set.clap-state.bin");
        let doc = String::from_utf8_lossy(&set).replacen("<Mode threshold=\"-18.0\"", "<Mode threshold=\"-33.5\"", 1);
        let mut bytes = doc.into_bytes();
        let size = (bytes.len() - 8) as u32; // the replacement keeps the length
        bytes[4..8].copy_from_slice(&size.to_le_bytes());
        let s = decode(&bytes).expect("edited state");
        assert_eq!(s.snapshots[0][0], -33.5);
    }
}
