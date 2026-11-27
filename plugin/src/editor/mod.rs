//! 1.5.3's editor (`Source/PluginEditor.cpp` and `Source/UI/`) on OpenPluginSDK: the same 1120 x 760
//! canvas, components, colours, text and behaviour, drawn through JUCE's CoreGraphics calls
//! (`crate::gfx`) so the pixels match. Layout constants, colours and strings are 1.5.3's.
//!
//! Differences that a plugin cannot reproduce through the SDK's editor contract: popup menus are
//! drawn inside the editor (JUCE opened them as separate windows with a system shadow, which could
//! reach past the editor's edge), and the pointer keeps its arrow (JUCE showed a hand over the module
//! headers and the curve's handles).

pub mod scope;
pub mod theme;
pub mod widgets;

use std::sync::Arc;

use openplugin::core::editor::{AccessAction, AccessNode, AccessRole, DamageRect, Modifiers, SizeRange};
use openplugin::core::{Editor, EditorHost, EditorState, Frame};
use openplugin::prelude::Taper;

use crate::gfx::*;
use crate::params::{COUNT, PARAMS, idx, legal, whole};
use crate::state::SNAP;
use crate::{Meters, Shared, UiState};
use scope::{Curve, Handle, History, fixed};
use theme::{THEMES, Theme};
use widgets::{HEADER, KnobLook, ModuleLook, c};

pub const W: i32 = 1120;
pub const H: i32 = 760;

const ALGORITHM_NAMES: [&str; 7] = ["Red", "Hybrid", "Bridge", "Bus VCA", "76", "Opto Leveler", "VCA 160"];
const ALGORITHM_COLOURS: [u32; 7] = [0xffb3382c, 0xff3fa796, 0xffcda434, 0xffc9702e, 0xff4a86c8, 0xffb58a47, 0xff377f9d];
const OVERSAMPLE_NAMES: [&str; 4] = ["1x", "2x", "4x", "8x"];
const FILTER_NAMES: [&str; 2] = ["Min Phase", "Lin Phase"];
const HARMONIC_PRESETS: [(&str, &str, [f32; 4]); 6] = [
    ("Off (Clean)", "Off", [0.0, 0.0, 0.0, 0.0]),
    ("Tube (Even)", "Tube", [35.0, 0.0, 15.0, 0.0]),
    ("Tape (Odd)", "Tape", [0.0, 35.0, 0.0, 15.0]),
    ("Triode", "Tri", [45.0, 12.0, 8.0, 0.0]),
    ("Pentode", "Pent", [10.0, 45.0, 5.0, 12.0]),
    ("Full Warmth", "Warm", [30.0, 30.0, 15.0, 15.0]),
];
const CUSTOM: usize = HARMONIC_PRESETS.len();
const SC_MODES: [&str; 4] = ["Off", "HPF On", "Listen (HPF)", "Listen (Dry)"];
const SC_TAGS: [&str; 4] = ["Off", "HPF", "LstHP", "LstDry"];
/// The meters' scales (`crate::METER_PPM`, `METER_RMS`, `METER_VU`): the selector and its menu.
const METER_TAGS: [&str; 3] = ["PPM dBFS", "RMS dBFS", "VU"];
const METER_NAMES: [&str; 3] = ["PPM (dBFS)", "RMS (dBFS)", "VU (0 VU = -18 dBFS)"];

// ---- formatting, as 1.5.3's editor formats ---------------------------------------------------------

/// `juce::String (double)`: the standard library's default format (six significant digits).
pub fn general(v: f64) -> String {
    if v == 0.0 {
        return if v.is_sign_negative() { "-0".into() } else { "0".into() };
    }
    let trim = |s: String| if s.contains('.') { s.trim_end_matches('0').trim_end_matches('.').to_string() } else { s };
    let sci = format!("{v:.5e}");
    let (mantissa, exp) = sci.split_once('e').unwrap_or((&sci, "0"));
    let exp: i32 = exp.parse().unwrap_or(0);
    if !(-4..6).contains(&exp) {
        format!("{}e{}{:02}", trim(mantissa.to_string()), if exp < 0 { '-' } else { '+' }, exp.abs())
    } else {
        trim(format!("{:.*}", (5 - exp) as usize, v))
    }
}

// The display rules (owner, 2026-11-27): whole dB, ratios and percents, at most three digits, no
// 0.x value. 1.5.3 showed tenths ("-17.5 dB", "100.0%", "0.1 ms").
fn sign(v: f32, on: bool) -> &'static str {
    if on && v > 0.0 { "+" } else { "" }
}
fn db_text(v: f32, signed: bool) -> String {
    format!("{}{} dB", sign(v, signed), whole(v))
}
fn ratio_text(v: f32) -> String {
    if v >= 99.5 { "\u{221e}:1".into() } else { format!("{}:1", whole(v)) }
}
fn ms_text(v: f32) -> String {
    if v < 1.0 {
        format!("{} \u{b5}s", round_to_int(v * 1000.0))
    } else if v >= 1000.0 {
        format!("{} s", fixed(v / 1000.0, 2))
    } else if v >= 100.0 {
        format!("{} ms", round_to_int(v))
    } else {
        format!("{} ms", fixed(v, 1))
    }
}
fn compact_ms(v: f32) -> String {
    if v < 1.0 {
        format!("{}\u{b5}", round_to_int(v * 1000.0))
    } else if v >= 1000.0 {
        format!("{}s", fixed(v / 1000.0, 1))
    } else if v >= 20.0 {
        format!("{}m", round_to_int(v))
    } else {
        format!("{}m", fixed(v, 1))
    }
}
fn percent_text(v: f32, signed: bool) -> String {
    format!("{}{}%", sign(v, signed), whole(v))
}
fn format_value(param: usize, v: f32) -> String {
    match param {
        idx::THRESHOLD | idx::KNEE => db_text(v, false),
        idx::INPUT_GAIN | idx::OUTPUT_GAIN | idx::MAKEUP => db_text(v, true),
        idx::RATIO => ratio_text(v),
        idx::MIX => format!("{}%", round_to_int(v * 100.0)),
        idx::ATTACK | idx::RELEASE | idx::TRANSIENT_SPEED => ms_text(v),
        idx::H2 | idx::H3 | idx::H4 | idx::H5 => percent_text(v, false),
        idx::SC_HP_FREQ => format!("{} Hz", round_to_int(v)),
        _ => percent_text(v, true), // transient attack and sustain
    }
}

/// `NormalisableRange::convertTo0to1` with the parameter's skew.
fn proportion(param: usize, v: f32) -> f32 {
    let p = &PARAMS[param];
    let x = ((v - p.min) / (p.max - p.min)).clamp(0.0, 1.0);
    match p.taper {
        Taper::Skew(s) if s.get() != 1.0 => x.powf(s.get()),
        _ => x,
    }
}

// ---- the component layout of 1.5.3 -----------------------------------------------------------------

#[derive(Clone, Copy, PartialEq, Eq, Debug)]
enum Parent {
    InMeter,
    InGain,
    OutGain,
    KnobBar,
    Surface,
    Module(usize),
}

struct KnobSpec {
    param: usize,
    label: &'static str,
    colour: u32,
    parent: Parent,
    bounds: IRect,
    rotary: bool,
    secondary: bool,
}

const fn knob(param: usize, label: &'static str, colour: u32, parent: Parent, b: [i32; 4], rotary: bool, secondary: bool) -> KnobSpec {
    KnobSpec { param, label, colour, parent, bounds: IRect::new(b[0], b[1], b[2], b[3]), rotary, secondary }
}

/// `addKnob` calls, bounds and colours from `Designs/HarmonicCompressor.csv`.
const KNOBS: [KnobSpec; 17] = [
    knob(idx::INPUT_GAIN, "GAIN", 0xff2e86c1, Parent::InGain, [6, 26, 84, 44], false, true),
    knob(idx::OUTPUT_GAIN, "GAIN", 0xff3ccf5a, Parent::OutGain, [6, 26, 84, 44], false, false),
    knob(idx::THRESHOLD, "THRESHOLD", 0xffc7a66a, Parent::KnobBar, [12, 6, 170, 64], true, false),
    knob(idx::RATIO, "RATIO", 0xffc7a66a, Parent::KnobBar, [196, 6, 170, 64], true, false),
    knob(idx::MAKEUP, "MAKEUP", 0xff5899c4, Parent::KnobBar, [380, 6, 170, 64], true, true),
    knob(idx::MIX, "MIX", 0xff5899c4, Parent::KnobBar, [564, 6, 170, 64], true, true),
    knob(idx::ATTACK, "ATTACK", 0xffc7a66a, Parent::Module(0), [10, 6, 122, 42], false, false),
    knob(idx::RELEASE, "RELEASE", 0xffc7a66a, Parent::Module(0), [142, 6, 122, 42], false, false),
    knob(idx::KNEE, "KNEE", 0xff5899c4, Parent::Module(0), [274, 6, 122, 42], false, true),
    knob(idx::H2, "2ND", 0xffcda434, Parent::Module(1), [166, 5, 122, 42], false, false),
    knob(idx::H3, "3RD", 0xffc9702e, Parent::Module(1), [298, 5, 122, 42], false, true),
    knob(idx::H4, "4TH", 0xffb3382c, Parent::Module(1), [166, 51, 122, 42], false, false),
    knob(idx::H5, "5TH", 0xff3fa796, Parent::Module(1), [298, 51, 122, 42], false, true),
    knob(idx::SC_HP_FREQ, "HP FREQ", 0xff4a86c8, Parent::Module(2), [152, 6, 144, 42], false, true),
    knob(idx::TRANSIENT_ATTACK, "ATTACK", 0xff2c9973, Parent::Module(3), [88, 7, 112, 42], false, false),
    knob(idx::TRANSIENT_SUSTAIN, "SUSTAIN", 0xff2d8bba, Parent::Module(3), [208, 7, 112, 42], false, true),
    knob(idx::TRANSIENT_SPEED, "SPEED", 0xff6a8caf, Parent::Module(3), [328, 7, 116, 42], false, false),
];

#[derive(Clone, Copy, PartialEq, Eq, Debug)]
enum KeyId {
    AutoAttack,
    AutoRelease,
    TransientHeader,
    TransientModule,
    Curve,
    AutoGain,
    Delta,
}

const KEYS: [(KeyId, &str, Parent, IRect); 7] = [
    (KeyId::AutoAttack, "AUTO ATTACK", Parent::Module(0), IRect::new(10, 52, 122, 22)),
    (KeyId::AutoRelease, "AUTO RELEASE", Parent::Module(0), IRect::new(142, 52, 122, 22)),
    (KeyId::TransientHeader, "TRANSIENT", Parent::Surface, IRect::new(781, 9, 90, 27)),
    (KeyId::TransientModule, "ACTIVE", Parent::Module(3), IRect::new(10, 15, 68, 26)),
    (KeyId::Curve, "CURVE", Parent::Surface, IRect::new(876, 9, 72, 27)),
    (KeyId::AutoGain, "AUTO GAIN", Parent::KnobBar, IRect::new(750, 8, 126, 28)),
    (KeyId::Delta, "DELTA", Parent::KnobBar, IRect::new(750, 40, 126, 28)),
];

#[derive(Clone, Copy, PartialEq, Eq, Debug)]
enum DropId {
    Algorithm,
    Oversample,
    Filter,
    Colour,
    Harmonics,
    Sidechain,
    Meter,
}

const DROPS: [(DropId, Parent, IRect); 7] = [
    (DropId::Algorithm, Parent::Surface, IRect::new(451, 9, 150, 27)),
    (DropId::Oversample, Parent::Surface, IRect::new(629, 9, 52, 27)),
    (DropId::Filter, Parent::Surface, IRect::new(685, 9, 90, 27)),
    (DropId::Colour, Parent::Surface, IRect::new(996, 9, 112, 27)),
    (DropId::Harmonics, Parent::Module(1), IRect::new(10, 8, 146, 28)),
    (DropId::Sidechain, Parent::Module(2), IRect::new(10, 13, 132, 28)),
    // new: where the output meter has its GR readout
    (DropId::Meter, Parent::InMeter, IRect::new(6, 22, 84, 18)),
];

const INPUT_METER: IRect = IRect::new(12, 48, 96, 618);
const IN_GAIN: IRect = IRect::new(12, 672, 96, 76);
const SCOPE: IRect = IRect::new(116, 48, 888, 618);
const KNOB_BAR: IRect = IRect::new(116, 672, 888, 76);
const OUT_METER: IRect = IRect::new(1012, 48, 96, 618);
const OUT_GAIN: IRect = IRect::new(1012, 672, 96, 76);
/// The toolbar's controls (the title and the trace key are painted once with the chassis).
const TOOLBAR: IRect = IRect::new(440, 6, 672, 34);
/// The parts that repaint on their own, as 1.5.3's components did: the meters, the gain panels, the
/// history field, the knob bar and the toolbar.
const REGIONS: [IRect; 7] = [INPUT_METER, IN_GAIN, SCOPE, KNOB_BAR, OUT_METER, OUT_GAIN, TOOLBAR];

type Size = (i32, i32);

/// The four floating modules: title, closed and open size.
const MODULES: [(&str, Size, Size); 4] = [
    ("COMPRESSION", (198, 28), (406, 108)),
    ("HARMONICS", (200, 28), (430, 126)),
    ("SIDECHAIN", (186, 28), (306, 82)),
    ("TRANSIENT", (204, 28), (454, 84)),
];

/// `TransferCurveDisplay` bounds inside the scope (`CompressionScope::resized`).
fn curve_bounds() -> IRect {
    let r = scope::plot_rect(SCOPE.w, SCOPE.h);
    let side = r.h.min(r.w * 0.50);
    IRect::new(
        round_to_int(r.x - scope::PAD_LEFT),
        round_to_int(r.y - scope::PAD_TOP),
        round_to_int(side + scope::PAD_LEFT + scope::PAD_RIGHT),
        round_to_int(r.h + scope::PAD_TOP + scope::PAD_BOTTOM),
    )
}

#[derive(Clone, Copy, PartialEq, Debug)]
enum Hit {
    Knob(usize),
    Key(usize),
    Drop(usize),
    Module(usize),
    ModuleBody,
    Curve(Handle),
    Resizer,
    Nothing,
}

#[derive(Clone, Copy, Default)]
struct KnobState {
    position: f64,
    velocity: f64,
    seeded: bool,
}

impl KnobState {
    /// `glui::Motion::step`: critically damped, 18 rad/s.
    fn step(&mut self, target: f64, seconds: f64) {
        let seconds = seconds.clamp(0.0, 0.1);
        let frequency = 18.0;
        let offset = self.position - target;
        let decay = (-frequency * seconds).exp();
        let term = self.velocity + frequency * offset;
        self.position = target + (offset + term * seconds) * decay;
        self.velocity = (self.velocity - frequency * term * seconds) * decay;
    }
}

#[derive(Clone, Copy)]
struct ModuleState {
    x: i32,
    y: i32,
    open: bool,
}

/// What the held mouse button is doing.
#[derive(Clone, Copy)]
enum Press {
    None,
    Knob { k: usize, start: (f32, f32), last: (f32, f32), value_on_down: f32, last_dragged: f32 },
    Key(usize),
    Drop(usize),
    Module { m: usize, start: (i32, i32), top_left: (i32, i32), dragged: bool },
    Curve { h: Handle, start: (f32, f32), threshold: f32, ratio: f32, makeup: f32 },
    Resizer { start: (f32, f32), size: (u32, u32) },
    Popup(Option<usize>),
    Swallowed,
}

#[derive(Clone, Copy, PartialEq, Eq, Debug)]
enum MenuKind {
    Algorithm,
    Oversample,
    Filter,
    Colour,
    Harmonics,
    Sidechain,
    Meter,
}

struct MenuItem {
    text: String,
    ticked: bool,
    separator: bool,
    choice: usize,
}

struct Popup {
    kind: MenuKind,
    items: Vec<MenuItem>,
    bounds: IRect, // surface coordinates
    highlighted: Option<usize>,
}

pub struct HcEditor {
    host: EditorHost,
    shared: Option<Arc<Shared>>,
    v: [f32; COUNT],
    size: (u32, u32),
    canvas: Option<Canvas>,
    ui: UiState,
    knobs: [KnobState; 17],
    modules: [ModuleState; 4],
    /// Back to front.
    module_order: [usize; 4],
    history: History,
    curve_range: (f32, f32),
    harmonics_custom: bool,
    hover: Hit,
    press: Press,
    pointer: (f32, f32),
    modifiers: Modifiers,
    popup: Option<Popup>,
    scope_clock: f32,
    /// What each region was last painted from, the global state (look, size, menu) and the scale.
    painted: Option<(u64, [u64; 7], f32)>,
    /// The history field's underlay: its key, device rectangle and pixels.
    underlay: Option<(u64, IRect, Vec<u32>)>,
    resize_wanted: bool,
}

pub fn make_editor(host: EditorHost) -> Option<Box<dyn Editor>> {
    Some(Box::new(HcEditor::new(host)))
}

impl HcEditor {
    pub fn new(host: EditorHost) -> Self {
        let ui = UiState::default();
        let mut e = HcEditor {
            host,
            shared: None,
            v: crate::params::defaults(),
            size: (W as u32, H as u32),
            canvas: None,
            ui,
            knobs: [KnobState::default(); 17],
            modules: [ModuleState { x: 0, y: 0, open: false }; 4],
            module_order: [0, 1, 2, 3],
            history: History::default(),
            curve_range: (-60.0, 0.0),
            harmonics_custom: false,
            hover: Hit::Nothing,
            press: Press::None,
            pointer: (-1.0, -1.0),
            modifiers: Modifiers::default(),
            popup: None,
            scope_clock: 0.0,
            painted: None,
            underlay: None,
            resize_wanted: false,
        };
        e.apply_ui();
        e
    }

    /// The controls a screen reader can reach, in reading order: those painted now.
    fn access_items(&self) -> Vec<Hit> {
        let mut out = vec![Hit::Drop(6)];
        for d in [0, 1, 2] {
            out.push(Hit::Drop(d));
        }
        out.extend([Hit::Key(2), Hit::Key(4), Hit::Drop(3)]);
        out.extend([Hit::Knob(0), Hit::Knob(1)]);
        out.extend([2, 3, 4, 5].map(Hit::Knob));
        out.extend([Hit::Key(5), Hit::Key(6)]);
        for m in 0..4 {
            if !self.modules[m].open {
                continue;
            }
            let p = Parent::Module(m);
            out.extend((0..DROPS.len()).filter(|&d| DROPS[d].1 == p).map(Hit::Drop));
            out.extend((0..KEYS.len()).filter(|&k| KEYS[k].2 == p).map(Hit::Key));
            out.extend((0..KNOBS.len()).filter(|&k| KNOBS[k].parent == p).map(Hit::Knob));
        }
        out
    }

    /// A control's rectangle in editor points.
    fn editor_rect(&self, p: Parent, b: IRect) -> DamageRect {
        let (ox, oy) = self.origin(p);
        let (s, tx, ty) = self.surface_transform();
        DamageRect::new((ox + b.x) as f32 * s + tx, (oy + b.y) as f32 * s + ty, b.w as f32 * s, b.h as f32 * s)
    }

    /// Steps a selector to its next or previous choice, as choosing it from its menu would.
    fn step_drop(&mut self, d: usize, up: bool) {
        let (kind, current, count) = match DROPS[d].0 {
            DropId::Algorithm => (MenuKind::Algorithm, self.mode(), 7),
            DropId::Oversample => (MenuKind::Oversample, self.oversample(), 4),
            DropId::Filter => (MenuKind::Filter, round_to_int(self.v[idx::OVERSAMPLE_FILTER]).clamp(0, 1) as usize, 2),
            DropId::Colour => (MenuKind::Colour, self.ui.colour_mode as usize, 4),
            DropId::Harmonics => (MenuKind::Harmonics, self.harmonics_preset(), CUSTOM + 1),
            DropId::Sidechain => (MenuKind::Sidechain, self.sidechain_mode(), 4),
            DropId::Meter => (MenuKind::Meter, self.meter_scale(), 3),
        };
        let next = if up { (current + 1).min(count - 1) } else { current.saturating_sub(1) };
        if next != current {
            self.choose(kind, next);
        }
    }

    /// Paints everything on the next frame (a host that lost the picture, or a benchmark).
    pub fn invalidate(&mut self) {
        self.painted = None;
    }

    /// `FloatingModule::setOpen`, for tools that render a given state.
    pub fn set_module_open(&mut self, m: usize, open: bool) {
        if m < 4 && self.modules[m].open != open {
            self.modules[m].open = open;
            if open {
                self.bring_to_front(m);
            }
            self.clamp_module(m);
            self.save_modules();
        }
    }

    /// The saved look: colour mode, curve, size and module positions (1.5.3 read them when the
    /// editor opened).
    fn apply_ui(&mut self) {
        for (m, s) in self.modules.iter_mut().enumerate() {
            s.x = self.ui.modules[m].0;
            s.y = self.ui.modules[m].1;
        }
        for m in 0..4 {
            self.clamp_module(m);
        }
        let w = self.ui.editor_width.clamp(560, 1600);
        self.size = (w as u32, round_to_int(w as f32 * H as f32 / W as f32) as u32);
    }

    fn theme(&self) -> &'static Theme {
        &THEMES[self.ui.colour_mode.clamp(0, 3) as usize]
    }
    fn on(&self, param: usize) -> bool {
        self.v[param] > 0.5
    }
    fn mode(&self) -> usize {
        (round_to_int(self.v[idx::MODE]).clamp(0, 6)) as usize
    }
    fn circuit_timing(&self) -> bool {
        matches!(self.mode(), 5 | 6)
    }
    fn oversample(&self) -> usize {
        round_to_int(self.v[idx::OVERSAMPLE_MODE]).clamp(0, 3) as usize
    }

    /// `applySkin`'s accents.
    fn algo_tint(&self) -> Colour {
        c(self.theme().accent_primary).interpolated_with(c(ALGORITHM_COLOURS[self.mode()]), 0.28)
    }

    fn knob_enabled(&self, k: usize) -> bool {
        let p = KNOBS[k].param;
        let fixed_knob = self.circuit_timing() || (p == idx::KNEE && self.mode() != 0);
        !(matches!(p, idx::ATTACK | idx::RELEASE | idx::KNEE) && fixed_knob)
    }
    fn knob_active(&self, k: usize) -> bool {
        match KNOBS[k].param {
            idx::ATTACK => !self.on(idx::AUTO_ATTACK),
            idx::RELEASE => !self.on(idx::AUTO_RELEASE),
            idx::SC_HP_FREQ => self.on(idx::SC_ENABLED),
            idx::TRANSIENT_ATTACK | idx::TRANSIENT_SUSTAIN | idx::TRANSIENT_SPEED => self.on(idx::TRANSIENT_ENABLED),
            _ => true,
        }
    }
    fn key_enabled(&self, k: usize) -> bool {
        !matches!(KEYS[k].0, KeyId::AutoAttack | KeyId::AutoRelease) || !self.circuit_timing()
    }
    fn key_on(&self, k: usize) -> bool {
        match KEYS[k].0 {
            KeyId::AutoAttack => self.on(idx::AUTO_ATTACK),
            KeyId::AutoRelease => self.on(idx::AUTO_RELEASE),
            KeyId::TransientHeader | KeyId::TransientModule => self.on(idx::TRANSIENT_ENABLED),
            KeyId::Curve => self.ui.show_curve,
            KeyId::AutoGain => self.on(idx::AUTO_GAIN),
            KeyId::Delta => self.on(idx::DELTA),
        }
    }
    fn key_lamp(&self, k: usize) -> Colour {
        let t = self.theme();
        match KEYS[k].0 {
            KeyId::AutoAttack | KeyId::AutoRelease => {
                if t.light_surface {
                    self.algo_tint()
                } else {
                    self.algo_tint().brighter(0.15)
                }
            }
            KeyId::TransientHeader | KeyId::TransientModule => c(t.threshold_ref),
            KeyId::Curve | KeyId::AutoGain => c(t.accent_secondary),
            KeyId::Delta => c(t.lamp_warn),
        }
    }
    /// The Auto Gain key shows the gain it applies while on.
    fn key_text(&self, k: usize) -> String {
        if KEYS[k].0 == KeyId::AutoGain && self.on(idx::AUTO_GAIN) {
            let db = self.meters().map_or(0.0, |m| m.auto_gain_db());
            return format!("AUTO {} dB", scope::signed(db));
        }
        KEYS[k].1.to_string()
    }

    fn drop_enabled(&self, d: usize) -> bool {
        DROPS[d].0 != DropId::Filter || self.oversample() > 0
    }
    fn drop_accent(&self, d: usize) -> Colour {
        let t = self.theme();
        match DROPS[d].0 {
            DropId::Algorithm => self.algo_tint(),
            DropId::Oversample | DropId::Filter | DropId::Sidechain => c(t.accent_secondary),
            DropId::Colour => c(t.accent_primary),
            DropId::Harmonics | DropId::Meter => c(t.lamp_warn),
        }
    }
    fn drop_text(&self, d: usize) -> String {
        match DROPS[d].0 {
            DropId::Algorithm => ALGORITHM_NAMES[self.mode()].into(),
            DropId::Oversample => OVERSAMPLE_NAMES[self.oversample()].into(),
            DropId::Filter => FILTER_NAMES[round_to_int(self.v[idx::OVERSAMPLE_FILTER]).clamp(0, 1) as usize].into(),
            DropId::Colour => self.theme().name.into(),
            DropId::Harmonics => {
                let i = self.harmonics_preset();
                if i == CUSTOM { "Custom".into() } else { HARMONIC_PRESETS[i].0.into() }
            }
            DropId::Sidechain => SC_MODES[self.sidechain_mode()].into(),
            DropId::Meter => METER_TAGS[self.meter_scale()].into(),
        }
    }
    fn meter_scale(&self) -> usize {
        self.ui.meter.clamp(0, 2) as usize
    }

    /// `detectHarmonicsPresetIndex`.
    fn harmonics_preset(&self) -> usize {
        let h = [self.v[idx::H2], self.v[idx::H3], self.v[idx::H4], self.v[idx::H5]];
        for (i, p) in HARMONIC_PRESETS.iter().enumerate() {
            if i == 0 && self.harmonics_custom {
                continue;
            }
            if h.iter().zip(p.2).all(|(a, b)| (a - b).abs() <= 0.25) {
                return i;
            }
        }
        CUSTOM
    }
    /// `currentSidechainModeIndex`.
    fn sidechain_mode(&self) -> usize {
        match (self.on(idx::SC_ENABLED), self.on(idx::SC_LISTEN)) {
            (false, false) => 0,
            (true, false) => 1,
            (true, true) => 2,
            (false, true) => 3,
        }
    }

    fn module_accent(&self, m: usize) -> Colour {
        let t = self.theme();
        match m {
            0 => self.algo_tint(),
            1 => c(t.lamp_warn),
            2 => c(t.accent_secondary),
            _ => c(t.threshold_ref),
        }
    }
    fn module_highlighted(&self, m: usize) -> bool {
        match m {
            1 => self.v[idx::H2] + self.v[idx::H3] + self.v[idx::H4] + self.v[idx::H5] > 0.25,
            2 => self.on(idx::SC_ENABLED) || self.on(idx::SC_LISTEN),
            3 => self.on(idx::TRANSIENT_ENABLED),
            _ => false,
        }
    }
    /// `updateModuleSummaries`.
    fn module_summary(&self, m: usize) -> String {
        let v = &self.v;
        match m {
            0 => format!(
                "{}{} {}{} K{}",
                if self.on(idx::AUTO_ATTACK) { "A" } else { "" },
                compact_ms(v[idx::ATTACK]),
                if self.on(idx::AUTO_RELEASE) { "A" } else { "" },
                compact_ms(v[idx::RELEASE]),
                whole(v[idx::KNEE])
            ),
            1 => {
                let i = self.harmonics_preset();
                let tag = if i == CUSTOM { "Cst" } else { HARMONIC_PRESETS[i].1 };
                format!(
                    "{tag} {}/{}/{}/{}",
                    round_to_int(v[idx::H2]),
                    round_to_int(v[idx::H3]),
                    round_to_int(v[idx::H4]),
                    round_to_int(v[idx::H5])
                )
            }
            2 => format!("{} {}Hz", SC_TAGS[self.sidechain_mode()], round_to_int(v[idx::SC_HP_FREQ])),
            _ => {
                let signed = |x: i32| format!("{}{x}", if x > 0 { "+" } else { "" });
                format!(
                    "{}{}/{} {}",
                    if self.on(idx::TRANSIENT_ENABLED) { "ON " } else { "OFF " },
                    signed(round_to_int(v[idx::TRANSIENT_ATTACK])),
                    signed(round_to_int(v[idx::TRANSIENT_SUSTAIN])),
                    compact_ms(v[idx::TRANSIENT_SPEED])
                )
            }
        }
    }
    fn module_size(&self, m: usize) -> (i32, i32) {
        if self.modules[m].open { MODULES[m].2 } else { MODULES[m].1 }
    }
    fn module_bounds(&self, m: usize) -> IRect {
        let (w, h) = self.module_size(m);
        IRect::new(self.modules[m].x, self.modules[m].y, w, h)
    }
    /// `FloatingModule::clampToParent`, inside the scope with a 6-unit margin.
    fn clamp_module(&mut self, m: usize) {
        let (w, h) = self.module_size(m);
        let max_x = 6.max(SCOPE.w - w - 6);
        let max_y = 6.max(SCOPE.h - h - 6);
        self.modules[m].x = self.modules[m].x.clamp(6, max_x);
        self.modules[m].y = self.modules[m].y.clamp(6, max_y);
    }
    fn bring_to_front(&mut self, m: usize) {
        let mut order: Vec<usize> = self.module_order.iter().copied().filter(|&o| o != m).collect();
        order.push(m);
        self.module_order.copy_from_slice(&order);
    }
    fn save_modules(&mut self) {
        for m in 0..4 {
            self.ui.modules[m] = (self.modules[m].x, self.modules[m].y);
        }
        let modules = self.ui.modules;
        self.store_ui(|ui| ui.modules = modules);
    }
    /// Changes one setting of the saved look. Only that setting: a state the host loaded while the
    /// editor was open keeps the rest (1.5.3 wrote each property on its own).
    fn store_ui(&self, change: impl FnOnce(&mut UiState)) {
        if let Some(s) = &self.shared
            && let Ok(mut ui) = s.ui.lock()
        {
            change(&mut ui);
        }
    }

    fn meters(&self) -> Option<&Meters> {
        self.shared.as_deref().map(|s| &s.meters)
    }

    /// Where a parent's children start, in surface coordinates.
    fn origin(&self, p: Parent) -> (i32, i32) {
        match p {
            Parent::InMeter => (INPUT_METER.x, INPUT_METER.y),
            Parent::InGain => (IN_GAIN.x, IN_GAIN.y),
            Parent::OutGain => (OUT_GAIN.x, OUT_GAIN.y),
            Parent::KnobBar => (KNOB_BAR.x, KNOB_BAR.y),
            Parent::Surface => (0, 0),
            Parent::Module(m) => (SCOPE.x + self.modules[m].x, SCOPE.y + self.modules[m].y + HEADER),
        }
    }
    fn visible(&self, p: Parent) -> bool {
        match p {
            Parent::Module(m) => self.modules[m].open,
            _ => true,
        }
    }

    /// The surface's scale and offset in the editor (`resized`).
    fn surface_transform(&self) -> (f32, f32, f32) {
        let (w, h) = (self.size.0 as f32, self.size.1 as f32);
        let s = (w / W as f32).min(h / H as f32);
        (s, (w - W as f32 * s) * 0.5, (h - H as f32 * s) * 0.5)
    }
    fn to_surface(&self, x: f32, y: f32) -> (f32, f32) {
        let (s, tx, ty) = self.surface_transform();
        ((x - tx) / s, (y - ty) / s)
    }

    fn curve(&self) -> Curve {
        let b = curve_bounds();
        let p = &PARAMS[idx::MAKEUP];
        Curve {
            area: Rect::new(scope::PAD_LEFT, scope::PAD_TOP, b.w as f32 - scope::PAD_LEFT - scope::PAD_RIGHT, b.h as f32 - scope::PAD_TOP - scope::PAD_BOTTOM),
            min_db: self.curve_range.0,
            max_db: self.curve_range.1,
            threshold: self.v[idx::THRESHOLD],
            ratio: self.v[idx::RATIO],
            knee: if self.mode() == 0 { self.v[idx::KNEE] } else { 0.0 },
            makeup: self.v[idx::MAKEUP],
            makeup_norm: (self.v[idx::MAKEUP] - p.min) / (p.max - p.min),
        }
    }

    /// `TransferCurveDisplay::setDisplayRange`.
    fn set_curve_range(&mut self, min_db: f32, max_db: f32) {
        let max = max_db.clamp(-30.0, 12.0);
        let min = min_db.clamp(-78.0, max - 18.0);
        if (min - self.curve_range.0).abs() > 0.02 || (max - self.curve_range.1).abs() > 0.02 {
            self.curve_range = (min, max);
        }
    }

    // ---- hit testing (front to back, as JUCE finds the component under the mouse) ----

    fn hit(&self, ex: f32, ey: f32) -> Hit {
        let (ew, eh) = (self.size.0 as i32, self.size.1 as i32);
        let corner = IRect::new(ew - 18, eh - 18, 18, 18);
        if corner.contains(ex, ey) {
            let (lx, ly) = ((ex - corner.x as f32) as i32, (ey - corner.y as f32) as i32);
            if ly >= 18 - (18 * lx / 18) - 18 / 4 {
                return Hit::Resizer;
            }
        }
        let (x, y) = self.to_surface(ex, ey);
        if !(0.0..W as f32).contains(&x) || !(0.0..H as f32).contains(&y) {
            return Hit::Nothing;
        }
        let inside = |p: Parent, b: IRect| {
            let (ox, oy) = self.origin(p);
            self.visible(p) && b.translated(ox, oy).contains(x, y)
        };
        let key_at = |p: Parent| (0..KEYS.len()).rev().find(|&k| KEYS[k].2 == p && inside(p, KEYS[k].3)).map(Hit::Key);
        let drop_at = |p: Parent| (0..DROPS.len()).rev().find(|&d| DROPS[d].1 == p && inside(p, DROPS[d].2)).map(Hit::Drop);
        let knob_at = |p: Parent| (0..KNOBS.len()).rev().find(|&k| KNOBS[k].parent == p && inside(p, KNOBS[k].bounds)).map(Hit::Knob);

        // toolbar, added last to the surface
        for (i, item) in [(3, 'd'), (4, 'k'), (2, 'k'), (2, 'd'), (1, 'd'), (0, 'd')] {
            let hit = if item == 'd' {
                inside(Parent::Surface, DROPS[i].2).then_some(Hit::Drop(i))
            } else {
                inside(Parent::Surface, KEYS[i].3).then_some(Hit::Key(i))
            };
            if let Some(h) = hit {
                return h;
            }
        }
        if OUT_GAIN.contains(x, y) {
            return knob_at(Parent::OutGain).unwrap_or(Hit::Nothing);
        }
        if OUT_METER.contains(x, y) {
            return Hit::Nothing;
        }
        if KNOB_BAR.contains(x, y) {
            return key_at(Parent::KnobBar).or_else(|| knob_at(Parent::KnobBar)).unwrap_or(Hit::Nothing);
        }
        if SCOPE.contains(x, y) {
            for &m in self.module_order.iter().rev() {
                let b = self.module_bounds(m).translated(SCOPE.x, SCOPE.y);
                if !b.contains(x, y) {
                    continue;
                }
                if self.modules[m].open && y >= (b.y + HEADER) as f32 {
                    let p = Parent::Module(m);
                    return key_at(p).or_else(|| knob_at(p)).or_else(|| drop_at(p)).unwrap_or(Hit::ModuleBody);
                }
                return Hit::Module(m);
            }
            if self.ui.show_curve {
                let b = curve_bounds();
                let (lx, ly) = (x - (SCOPE.x + b.x) as f32, y - (SCOPE.y + b.y) as f32);
                if let Some(h) = self.curve().hit(lx, ly) {
                    return Hit::Curve(h);
                }
            }
            return Hit::Nothing;
        }
        if IN_GAIN.contains(x, y) {
            return knob_at(Parent::InGain).unwrap_or(Hit::Nothing);
        }
        if INPUT_METER.contains(x, y) {
            return drop_at(Parent::InMeter).unwrap_or(Hit::Nothing);
        }
        Hit::Nothing
    }

    // ---- gestures ----

    fn set_param(&mut self, param: usize, plain: f32) {
        let plain = legal(param, plain);
        self.host.set(param, plain);
        self.v[param] = plain;
    }
    fn perform(&mut self, param: usize, plain: f32) {
        let plain = legal(param, plain);
        if plain != self.v[param] {
            self.host.perform(param, plain);
            self.v[param] = plain;
        }
    }

    /// `HarmonicCompressorAudioProcessor::selectMode`: remember the old model's settings, switch, and
    /// recall the new model's.
    fn select_mode(&mut self, new: usize) {
        let old = self.mode();
        if new == old {
            return;
        }
        let Some(shared) = self.shared.clone() else {
            self.set_param(idx::MODE, new as f32);
            return;
        };
        let recalled = {
            let Ok(mut snaps) = shared.snapshots.lock() else { return };
            for (k, &p) in SNAP.iter().enumerate() {
                snaps[old][k] = self.v[p];
            }
            snaps[new]
        };
        self.set_param(idx::MODE, new as f32);
        for (k, &p) in SNAP.iter().enumerate() {
            self.set_param(p, recalled[k]);
        }
    }

    fn knob_drag(&mut self, k: usize, x: f32, y: f32) {
        let Press::Knob { start, last, value_on_down, mut last_dragged, .. } = self.press else { return };
        let spec = &KNOBS[k];
        let p = spec.param;
        let velocity = self.modifiers.ctrl || self.modifiers.alt || self.modifiers.command;
        let from = |pos: f32| PARAMS[p].from_normalized(pos.clamp(0.0, 1.0));
        if velocity {
            let diff = if spec.rotary { (x - last.0) + (last.1 - y) } else { x - last.0 };
            let max_speed = 200.0f64.max(if spec.rotary { 0.0 } else { (spec.bounds.w - 12) as f64 });
            let mut speed = (diff.abs() as f64).clamp(0.0, max_speed);
            if speed != 0.0 {
                speed = 0.2 * (1.0 + (std::f64::consts::PI * (1.5 + 0.5f64.min((speed - 1.0).max(0.0) / max_speed))).sin());
                if diff < 0.0 {
                    speed = -speed;
                }
                last_dragged = from(proportion(p, last_dragged) + speed as f32);
            }
        } else if spec.rotary {
            let diff = (x - start.0) + (start.1 - y);
            last_dragged = from(proportion(p, value_on_down) + diff / 170.0);
        } else {
            // the track inset by the thumb radius (Knob's getSliderLayout)
            let width = (spec.bounds.w - 2 * widgets::THUMB_RADIUS) as f32;
            last_dragged = from((x - widgets::THUMB_RADIUS as f32) / width);
        }
        self.press = Press::Knob { k, start, last: (x, y), value_on_down, last_dragged };
        self.perform(p, last_dragged);
    }

    fn knob_local(&self, k: usize, sx: f32, sy: f32) -> (f32, f32) {
        let (ox, oy) = self.origin(KNOBS[k].parent);
        (sx - (ox + KNOBS[k].bounds.x) as f32, sy - (oy + KNOBS[k].bounds.y) as f32)
    }

    fn click_key(&mut self, k: usize) {
        let on = !self.key_on(k);
        let value = if on { 1.0 } else { 0.0 };
        match KEYS[k].0 {
            KeyId::AutoAttack => self.set_param(idx::AUTO_ATTACK, value),
            KeyId::AutoRelease => self.set_param(idx::AUTO_RELEASE, value),
            KeyId::TransientHeader | KeyId::TransientModule => self.set_param(idx::TRANSIENT_ENABLED, value),
            KeyId::AutoGain => self.set_param(idx::AUTO_GAIN, value),
            KeyId::Delta => self.set_param(idx::DELTA, value),
            KeyId::Curve => {
                self.ui.show_curve = on;
                self.store_ui(|ui| ui.show_curve = on);
            }
        }
    }

    fn open_menu(&mut self, d: usize) {
        let (kind, items): (MenuKind, Vec<MenuItem>) = match DROPS[d].0 {
            DropId::Algorithm => (MenuKind::Algorithm, self.items(&ALGORITHM_NAMES, self.mode())),
            DropId::Oversample => (MenuKind::Oversample, self.items(&OVERSAMPLE_NAMES, self.oversample())),
            DropId::Filter => (MenuKind::Filter, self.items(&FILTER_NAMES, round_to_int(self.v[idx::OVERSAMPLE_FILTER]).clamp(0, 1) as usize)),
            DropId::Colour => {
                let names: Vec<&str> = THEMES.iter().map(|t| t.name).collect();
                (MenuKind::Colour, self.items(&names, self.ui.colour_mode as usize))
            }
            DropId::Harmonics => {
                let active = self.harmonics_preset();
                let names: Vec<&str> = HARMONIC_PRESETS.iter().map(|p| p.0).collect();
                let mut items = self.items(&names, active);
                items.push(MenuItem { text: String::new(), ticked: false, separator: true, choice: 0 });
                items.push(MenuItem { text: "Custom".into(), ticked: active == CUSTOM, separator: false, choice: CUSTOM });
                (MenuKind::Harmonics, items)
            }
            DropId::Sidechain => (MenuKind::Sidechain, self.items(&SC_MODES, self.sidechain_mode())),
            DropId::Meter => (MenuKind::Meter, self.items(&METER_NAMES, self.meter_scale())),
        };
        // LookAndFeel_V2: items 22 high (17 x 1.3), separators 10; ideal width text + 44; a 2-unit border
        let font = Font::new(17.0, false);
        let mut width = 0;
        let mut height = 0;
        for it in &items {
            if it.separator {
                width = width.max(50);
                height += 10;
            } else {
                width = width.max(Graphics::text_width(&it.text, font).ceil() as i32 + 44);
                height += 22;
            }
        }
        let (w, h) = (width + 4, height + 4);
        let (ox, oy) = self.origin(DROPS[d].1);
        let target = DROPS[d].2.translated(ox, oy);
        let below = H - target.bottom();
        let above = target.y;
        let y = if h < below - 30 || below >= above { target.bottom() } else { target.y - h };
        // shortcut: JUCE's menu is a window of its own and may pass the editor's edge; here it stays inside
        let x = target.x.min(W - w);
        self.popup = Some(Popup { kind, items, bounds: IRect::new(x, y, w, h), highlighted: None });
    }

    fn items(&self, names: &[&str], ticked: usize) -> Vec<MenuItem> {
        names.iter().enumerate().map(|(i, n)| MenuItem { text: (*n).into(), ticked: i == ticked, separator: false, choice: i }).collect()
    }

    fn popup_item_at(&self, x: f32, y: f32) -> Option<usize> {
        let p = self.popup.as_ref()?;
        if !p.bounds.contains(x, y) {
            return None;
        }
        let mut top = p.bounds.y + 2;
        for (i, it) in p.items.iter().enumerate() {
            let h = if it.separator { 10 } else { 22 };
            if !it.separator && y >= top as f32 && y < (top + h) as f32 {
                return Some(i);
            }
            top += h;
        }
        None
    }

    fn choose(&mut self, kind: MenuKind, choice: usize) {
        match kind {
            MenuKind::Algorithm => self.select_mode(choice),
            MenuKind::Oversample => self.set_param(idx::OVERSAMPLE_MODE, choice as f32),
            MenuKind::Filter => self.set_param(idx::OVERSAMPLE_FILTER, choice as f32),
            MenuKind::Colour => {
                self.ui.colour_mode = choice as i32;
                self.store_ui(|ui| ui.colour_mode = choice as i32);
            }
            MenuKind::Meter => {
                self.ui.meter = choice as i32;
                self.store_ui(|ui| ui.meter = choice as i32);
            }
            MenuKind::Harmonics => {
                if choice == CUSTOM {
                    self.harmonics_custom = true;
                } else {
                    self.harmonics_custom = false;
                    let targets = HARMONIC_PRESETS[choice].2;
                    for (p, t) in [idx::H2, idx::H3, idx::H4, idx::H5].into_iter().zip(targets) {
                        self.set_param(p, t);
                    }
                }
            }
            MenuKind::Sidechain => {
                let enabled = choice == 1 || choice == 2;
                let listen = choice == 2 || choice == 3;
                if enabled != self.on(idx::SC_ENABLED) {
                    self.set_param(idx::SC_ENABLED, if enabled { 1.0 } else { 0.0 });
                }
                if listen != self.on(idx::SC_LISTEN) {
                    self.set_param(idx::SC_LISTEN, if listen { 1.0 } else { 0.0 });
                }
            }
        }
    }

    // ---- painting, in JUCE's component order ----

    fn child(g: &mut Graphics, b: IRect, alpha: f32, paint: impl FnOnce(&mut Graphics)) {
        // Component::paintComponentAndChildren paints only children the clip reaches
        if !g.clip_intersects(b) {
            return;
        }
        g.save_state();
        g.reduce_clip_region(b);
        g.set_origin(b.x, b.y);
        if alpha < 1.0 {
            g.begin_transparency_layer(alpha);
            paint(g);
            g.end_transparency_layer();
        } else {
            paint(g);
        }
        g.restore_state();
    }

    fn paint_knob(&self, g: &mut Graphics, k: usize) {
        let spec = &KNOBS[k];
        let t = self.theme();
        let enabled = self.knob_enabled(k);
        let text = format_value(spec.param, self.v[spec.param]);
        let look = KnobLook {
            label: spec.label,
            text: &text,
            design: c(spec.colour),
            secondary: spec.secondary,
            position: self.knobs[k].position as f32,
            hovered: self.hover == Hit::Knob(k),
            dragging: matches!(self.press, Press::Knob { k: pk, .. } if pk == k),
            enabled: self.knob_active(k) && enabled,
        };
        let b = spec.bounds;
        Self::child(g, b, if enabled { 1.0 } else { 0.45 }, |g| {
            if spec.rotary {
                widgets::rotary(g, t, b.w, b.h, &look);
            } else {
                widgets::horizontal_slider(g, t, b.w, b.h, &look);
            }
        });
    }

    fn key_state(&self, k: usize) -> (bool, bool) {
        let pressed = matches!(self.press, Press::Key(pk) if pk == k);
        let over_now = self.hover == Hit::Key(k);
        let down = pressed && over_now;
        (over_now || down, down)
    }

    fn paint_key(&self, g: &mut Graphics, k: usize) {
        let b = KEYS[k].3;
        let (over, down) = self.key_state(k);
        let t = self.theme();
        let (label, lamp, on, enabled) = (self.key_text(k), self.key_lamp(k), self.key_on(k), self.key_enabled(k));
        Self::child(g, b, 1.0, |g| widgets::key(g, t, b.w, b.h, &label, lamp, on, over && enabled, down && enabled, enabled));
    }

    fn paint_drop(&self, g: &mut Graphics, d: usize) {
        let b = DROPS[d].2;
        let pressed = matches!(self.press, Press::Drop(pd) if pd == d);
        let over_now = self.hover == Hit::Drop(d);
        let enabled = self.drop_enabled(d);
        let (over, down) = (enabled && (over_now || (pressed && over_now)), enabled && pressed && over_now);
        let t = self.theme();
        let (text, accent) = (self.drop_text(d), self.drop_accent(d));
        let alpha = if DROPS[d].0 == DropId::Filter && !enabled { 0.55 } else { 1.0 };
        Self::child(g, b, alpha, |g| widgets::dropdown(g, t, b.w, b.h, &text, accent, over, down, enabled));
    }

    fn paint_children(&self, g: &mut Graphics, p: Parent) {
        self.paint_children_knobs(g, p);
        for (k, key) in KEYS.iter().enumerate() {
            if key.2 == p {
                self.paint_key(g, k);
            }
        }
        for (d, drop) in DROPS.iter().enumerate() {
            if drop.1 == p {
                self.paint_drop(g, d);
            }
        }
    }

    /// One module's children in their order of adding (selector or key before knobs where 1.5.3 did).
    fn paint_module_children(&self, g: &mut Graphics, m: usize) {
        let p = Parent::Module(m);
        match m {
            0 => {
                self.paint_children_knobs(g, p);
                self.paint_key(g, 0);
                self.paint_key(g, 1);
            }
            1 => {
                self.paint_drop(g, 4);
                self.paint_children_knobs(g, p);
            }
            2 => {
                self.paint_drop(g, 5);
                self.paint_children_knobs(g, p);
            }
            _ => {
                self.paint_key(g, 3);
                self.paint_children_knobs(g, p);
            }
        }
    }
    fn paint_children_knobs(&self, g: &mut Graphics, p: Parent) {
        for (k, spec) in KNOBS.iter().enumerate() {
            if spec.parent == p {
                self.paint_knob(g, k);
            }
        }
    }

    /// The history field's moving part and its children (curve, modules), in the scope's coordinates.
    fn paint_scope_over(&self, g: &mut Graphics, meters: &Meters) {
        let t = self.theme();
        scope::scope_over(g, t, SCOPE.w, SCOPE.h, &self.history, self.v[idx::THRESHOLD]);
        if self.ui.show_curve {
            let b = curve_bounds();
            let live = meters.last_input_db();
            let active = match self.press {
                Press::Curve { h, .. } => Some(h),
                _ => match self.hover {
                    Hit::Curve(h) => Some(h),
                    _ => None,
                },
            };
            Self::child(g, b, 1.0, |g| scope::curve(g, t, &self.curve(), active, live));
        }
        for &m in &self.module_order {
            let b = self.module_bounds(m);
            let summary = self.module_summary(m);
            let look = ModuleLook {
                title: MODULES[m].0,
                summary: &summary,
                accent: self.module_accent(m),
                open: self.modules[m].open,
                highlighted: self.module_highlighted(m),
                hover_header: self.hover == Hit::Module(m),
            };
            Self::child(g, b, 1.0, |g| {
                widgets::module(g, t, b.w, b.h, &look);
                if self.modules[m].open {
                    Self::child(g, IRect::new(0, HEADER, b.w, 0.max(b.h - HEADER)), 1.0, |g| self.paint_module_children(g, m));
                }
            });
        }
    }

    /// The surface transform and the history field's clip, as the full paint sets them.
    fn in_scope(&self, g: &mut Graphics, paint: impl FnOnce(&mut Graphics)) {
        let (s, tx, ty) = self.surface_transform();
        g.save_state();
        g.add_transform(Affine::scale(s).translated(tx, ty));
        g.reduce_clip_region(IRect::new(0, 0, W, H));
        Self::child(g, SCOPE, 1.0, paint);
        g.restore_state();
    }

    /// Everything under the history field's traces, inside its region: the chassis and the field's
    /// look, grid and labels.
    fn paint_scope_under(&self, g: &mut Graphics) {
        let t = self.theme();
        g.fill_all(c(t.chassis_bottom));
        let (s, tx, ty) = self.surface_transform();
        g.save_state();
        g.add_transform(Affine::scale(s).translated(tx, ty));
        g.reduce_clip_region(IRect::new(0, 0, W, H));
        scope::surface(g, t, W as f32, H as f32);
        Self::child(g, SCOPE, 1.0, |g| scope::scope_under(g, t, SCOPE.w, SCOPE.h, &self.history, self.ui.show_curve));
        g.restore_state();
    }

    fn paint(&self, g: &mut Graphics) {
        let t = self.theme();
        g.fill_all(c(t.chassis_bottom));

        let (s, tx, ty) = self.surface_transform();
        g.save_state();
        g.add_transform(Affine::scale(s).translated(tx, ty));
        g.reduce_clip_region(IRect::new(0, 0, W, H));
        scope::surface(g, t, W as f32, H as f32);

        let quiet = Meters::default();
        let meters = self.meters().unwrap_or(&quiet);
        let warn = c(t.lamp_warn);
        let scale = self.meter_scale();
        Self::child(g, INPUT_METER, 1.0, |g| {
            scope::meter_panel(g, t, INPUT_METER.w, INPUT_METER.h, true, warn, meters, scale);
            self.paint_children(g, Parent::InMeter);
        });
        Self::child(g, IN_GAIN, 1.0, |g| {
            widgets::panel(g, t, IN_GAIN.w, IN_GAIN.h, "IN GAIN", c(t.accent_secondary));
            self.paint_children(g, Parent::InGain);
        });
        Self::child(g, SCOPE, 1.0, |g| {
            scope::scope_under(g, t, SCOPE.w, SCOPE.h, &self.history, self.ui.show_curve);
            self.paint_scope_over(g, meters);
        });
        Self::child(g, KNOB_BAR, 1.0, |g| {
            widgets::panel(g, t, KNOB_BAR.w, KNOB_BAR.h, "", c(t.accent_primary));
            self.paint_children(g, Parent::KnobBar);
        });
        let primary = c(t.accent_primary);
        Self::child(g, OUT_METER, 1.0, |g| scope::meter_panel(g, t, OUT_METER.w, OUT_METER.h, false, primary, meters, scale));
        Self::child(g, OUT_GAIN, 1.0, |g| {
            widgets::panel(g, t, OUT_GAIN.w, OUT_GAIN.h, "OUT GAIN", primary);
            self.paint_children(g, Parent::OutGain);
        });
        // toolbar, in 1.5.3's order of adding
        self.paint_drop(g, 0);
        self.paint_drop(g, 1);
        self.paint_drop(g, 2);
        self.paint_key(g, 2);
        self.paint_key(g, 4);
        self.paint_drop(g, 3);

        if let Some(p) = &self.popup {
            self.paint_popup(g, t, p);
        }
        g.restore_state();

        let (ew, eh) = (self.size.0 as i32, self.size.1 as i32);
        Self::child(g, IRect::new(ew - 18, eh - 18, 18, 18), 1.0, |g| widgets::corner_resizer(g, 18, 18));
    }

    /// `ThemedPopupLookAndFeel`: the menu background and items.
    fn paint_popup(&self, g: &mut Graphics, t: &Theme, p: &Popup) {
        let b = p.bounds;
        Self::child(g, b, 1.0, |g| {
            let r = Rect::new(0.0, 0.0, b.w as f32, b.h as f32);
            g.set_gradient_fill(gradient(c(t.panel_header_top), 0.0, 0.0, c(t.chassis_bottom), 0.0, b.h as f32));
            g.fill_rect(r);
            g.set_colour(c(t.accent_primary).with_alpha(0.65));
            g.draw_rect(r.reduced(0.5), 1.2);
            let mut y = 2;
            for (i, it) in p.items.iter().enumerate() {
                let h = if it.separator { 10 } else { 22 };
                let area = IRect::new(0, y, b.w, h);
                if it.separator {
                    g.set_colour(c(t.panel_border_hi).with_alpha(0.55));
                    g.draw_horizontal_line(area.centre_y(), (area.x + 6) as f32, (area.right() - 6) as f32);
                } else {
                    let row = area.reduced(3, 1).to_float();
                    if p.highlighted == Some(i) {
                        g.set_colour(c(t.accent_primary).with_alpha(0.22));
                        g.fill_rounded_rectangle(row, 3.0);
                        g.set_colour(c(t.accent_primary).with_alpha(0.75));
                        g.draw_rounded_rectangle(row, 3.0, 0.9);
                    }
                    if it.ticked {
                        let cy = row.centre_y();
                        g.set_colour(c(t.accent_primary).with_alpha(0.30));
                        g.fill_ellipse(Rect::new(row.x + 5.0, cy - 4.5, 9.0, 9.0));
                        g.set_colour(c(t.accent_primary));
                        g.fill_ellipse(Rect::new(row.x + 7.2, cy - 2.3, 4.6, 4.6));
                    }
                    g.set_colour(c(if p.highlighted == Some(i) || it.ticked { t.text_bright } else { t.text_primary }));
                    g.set_font(Font::new(12.0, it.ticked));
                    g.draw_fitted_text(&it.text, area.with_trimmed_left(22).with_trimmed_right(8), Just::CentredLeft, 0.7);
                }
                y += h;
            }
        });
    }

    /// A digest of what the global look and each region are painted from.
    fn digests(&self) -> (u64, [u64; 7]) {
        use std::hash::{DefaultHasher, Hash, Hasher};
        let finish = |f: &dyn Fn(&mut DefaultHasher)| {
            let mut h = DefaultHasher::new();
            f(&mut h);
            h.finish()
        };
        let bits = |h: &mut DefaultHasher, v: f32| v.to_bits().hash(h);
        let global = finish(&|h| {
            self.ui.colour_mode.hash(h);
            self.ui.meter.hash(h);
            self.size.hash(h);
            if let Some(p) = &self.popup {
                p.bounds.hash(h);
                p.highlighted.hash(h);
                format!("{:?}", p.kind).hash(h);
            }
        });
        let press = match self.press {
            Press::None => "none".to_string(),
            Press::Knob { k, .. } => format!("knob{k}"),
            Press::Key(k) => format!("key{k}"),
            Press::Drop(d) => format!("drop{d}"),
            Press::Module { m, .. } => format!("module{m}"),
            Press::Curve { h, .. } => format!("curve{h:?}"),
            Press::Resizer { .. } => "resizer".into(),
            Press::Popup(i) => format!("popup{i:?}"),
            Press::Swallowed => "swallowed".into(),
        };
        let ui = finish(&|h| {
            for v in self.v {
                bits(h, v);
            }
            for k in &self.knobs {
                k.position.to_bits().hash(h);
            }
            format!("{:?}", self.hover).hash(h);
            press.hash(h);
            for m in &self.modules {
                (m.x, m.y, m.open).hash(h);
            }
            self.module_order.hash(h);
            (self.ui.show_curve, self.harmonics_custom).hash(h);
        });
        let quiet = Meters::default();
        let m = self.meters().unwrap_or(&quiet);
        let scale = self.meter_scale();
        let input = finish(&|h| {
            ui.hash(h); // the scale selector's hover and press
            [m.level(scale, 0), m.level(scale, 1)].into_iter().for_each(|v| bits(h, v));
        });
        let output = finish(&|h| [m.gr_db(), m.level(scale, 2), m.level(scale, 3)].into_iter().for_each(|v| bits(h, v)));
        let scope = finish(&|h| {
            ui.hash(h);
            self.history.digest().hash(h);
            [self.curve_range.0, self.curve_range.1, m.last_input_db()].into_iter().for_each(|v| bits(h, v));
        });
        let bar = finish(&|h| {
            ui.hash(h);
            bits(h, m.auto_gain_db());
        });
        (global, [input, ui, scope, bar, output, ui, ui])
    }

    /// The regions that changed since the last frame, or `None` when everything must be painted.
    fn changed_regions(&self, scale: f32) -> Option<Vec<usize>> {
        let (global, regions) = self.digests();
        let (last_global, last_regions, last_scale) = self.painted?;
        if last_global != global || last_scale != scale {
            return None;
        }
        Some((0..REGIONS.len()).filter(|&i| regions[i] != last_regions[i]).collect())
    }

    /// A region in editor points, a point wider on each side.
    fn region_rect(&self, i: usize) -> DamageRect {
        let (s, tx, ty) = self.surface_transform();
        let r = REGIONS[i];
        DamageRect::new(r.x as f32 * s + tx, r.y as f32 * s + ty, r.w as f32 * s, r.h as f32 * s).expanded(1.0)
    }

    /// A region in whole device pixels.
    fn region_px(&self, i: usize, scale: f32) -> IRect {
        let r = self.region_rect(i);
        let (x0, y0) = ((r.x * scale).floor() as i32, (r.y * scale).floor() as i32);
        let (x1, y1) = (((r.x + r.w) * scale).ceil() as i32, ((r.y + r.h) * scale).ceil() as i32);
        IRect::new(x0, y0, x1 - x0, y1 - y0)
    }

    /// What changed since the last frame: `None` for everything, otherwise the changed regions in
    /// editor points.
    fn changes(&self, scale: f32) -> Option<Vec<DamageRect>> {
        Some(self.changed_regions(scale)?.into_iter().map(|i| self.region_rect(i)).collect())
    }

    /// What the underlay is painted from.
    fn underlay_key(&self, scale: f32) -> u64 {
        use std::hash::{DefaultHasher, Hash, Hasher};
        let mut h = DefaultHasher::new();
        (self.ui.colour_mode, self.ui.show_curve, self.size, scale.to_bits()).hash(&mut h);
        (self.history.min_db.to_bits(), self.history.max_db.to_bits()).hash(&mut h);
        h.finish()
    }

    fn after_param_or_tick(&mut self) {
        for (spec, s) in KNOBS.iter().zip(self.knobs.iter_mut()) {
            if !s.seeded {
                s.position = f64::from(proportion(spec.param, self.v[spec.param]));
                s.velocity = 0.0;
            }
        }
    }
}

impl Drop for HcEditor {
    fn drop(&mut self) {
        // 1.5.3 kept the editor's width and the modules' places when the editor closed
        let (width, modules) = (self.size.0 as i32, self.ui.modules);
        self.store_ui(|ui| {
            ui.editor_width = width;
            ui.modules = modules;
        });
        if let Some(m) = self.meters() {
            m.watch(None);
        }
    }
}

impl Editor for HcEditor {
    fn size(&self) -> (u32, u32) {
        self.size
    }

    fn size_range(&self) -> SizeRange {
        SizeRange { min: (560, 380), max: (1600, 1085), aspect: Some((W as u32, H as u32)) }
    }

    fn resize(&mut self, width: u32, height: u32) {
        self.size = self.size_range().clamp(width, height);
    }

    fn bind(&mut self, state: Option<EditorState>) {
        self.shared = state.and_then(|s| s.downcast::<Shared>().ok());
        if let Some(s) = self.shared.clone() {
            if let Ok(ui) = s.ui.lock() {
                self.ui = *ui;
            }
            self.apply_ui();
            s.meters.watch(Some(self.meter_scale()));
            // The C++ shells (VST3, AU, AAX) open the view before the state arrives: the host is asked
            // for the saved size on the first tick, when every format's view exists.
            self.resize_wanted = true;
            // the scope starts empty (CompressionScope's constructor discards what is queued)
            if let Ok(mut rx) = s.history.lock() {
                while rx.pop().is_some() {}
            }
            s.history_drops.store(0, std::sync::atomic::Ordering::Relaxed);
        }
    }

    fn param_changed(&mut self, index: usize, plain: f32) {
        if index >= COUNT {
            return;
        }
        if matches!(index, idx::H2 | idx::H3 | idx::H4 | idx::H5) && plain != self.v[index] {
            self.harmonics_custom = false;
        }
        // the value a JUCE slider showed: the parameter's normalised value, back to plain and snapped
        self.v[index] = legal(index, PARAMS[index].from_normalized(proportion(index, plain)));
        self.after_param_or_tick();
    }

    fn tick(&mut self, dt: f32) {
        // the engine runs the meter this editor shows, and none when no editor is open
        if let Some(m) = self.meters() {
            m.watch(Some(self.meter_scale()));
        }
        if std::mem::take(&mut self.resize_wanted) {
            let (w, h) = self.size;
            let range = self.size_range();
            self.host.request_resize(&range, w, h);
        }
        // knob motion, 60 steps a second
        let steps = ((dt * 60.0).round() as usize).clamp(1, 6);
        for (spec, s) in KNOBS.iter().zip(self.knobs.iter_mut()) {
            let target = f64::from(proportion(spec.param, self.v[spec.param]));
            if !s.seeded {
                s.position = target;
                s.velocity = 0.0;
                continue;
            }
            for _ in 0..steps {
                if (s.position - target).abs() < 0.0001 && s.velocity.abs() < 0.0001 {
                    break;
                }
                s.step(target, 1.0 / 60.0);
            }
        }
        // the scope, 30 times a second
        self.scope_clock += dt;
        while self.scope_clock >= 1.0 / 30.0 {
            self.scope_clock -= 1.0 / 30.0;
            if let Some(s) = self.shared.clone() {
                if s.history_drops.swap(0, std::sync::atomic::Ordering::Relaxed) > 0 {
                    self.history.clear();
                }
                if let Ok(mut rx) = s.history.lock() {
                    while let Some(f) = rx.pop() {
                        self.history.push(f);
                    }
                }
            }
            self.history.update_auto_scale(false);
            let (min, max) = (self.history.min_db, self.history.max_db);
            self.set_curve_range(min, max);
        }
    }

    fn damage(&mut self) -> Option<Vec<DamageRect>> {
        let scale = self.painted.map_or(1.0, |p| p.2);
        self.changes(scale)
    }

    fn render(&mut self, scale: f32) -> Frame<'_> {
        let (pw, ph) = (round_to_int(self.size.0 as f32 * scale).max(1) as usize, round_to_int(self.size.1 as f32 * scale).max(1) as usize);
        let mut changes = self.changed_regions(scale);
        if self.canvas.as_ref().is_none_or(|cv| cv.width != pw || cv.height != ph) {
            self.canvas = Some(Canvas::new(pw, ph));
            changes = None;
        }
        // Knob::paint seeds the position on the first paint
        self.after_param_or_tick();
        for s in self.knobs.iter_mut() {
            s.seeded = true;
        }
        let digests = self.digests();
        let mut canvas = self.canvas.take().expect("canvas");
        match &changes {
            None => {
                let mut g = canvas.graphics(scale);
                self.paint(&mut g);
                self.underlay = None;
            }
            Some(regions) => {
                const SCOPE_REGION: usize = 2;
                let others: Vec<IRect> = regions.iter().filter(|&&i| i != SCOPE_REGION).map(|&i| self.region_px(i, scale)).collect();
                if !others.is_empty() {
                    let mut g = canvas.graphics_within(scale, &others);
                    self.paint(&mut g);
                }
                if regions.contains(&SCOPE_REGION) {
                    // the field's underlay from the cache when its look and range are unchanged, then
                    // the moving part on top: the same calls as a full paint, so the same pixels
                    let rect = self.region_px(SCOPE_REGION, scale);
                    let key = self.underlay_key(scale);
                    match &self.underlay {
                        Some((k, r, px)) if *k == key && *r == rect => canvas.copy_in(rect, px),
                        _ => {
                            {
                                let mut g = canvas.graphics_within(scale, &[rect]);
                                self.paint_scope_under(&mut g);
                            }
                            self.underlay = Some((key, rect, canvas.copy_out(rect)));
                        }
                    }
                    let quiet = Meters::default();
                    let meters = self.meters().unwrap_or(&quiet);
                    let mut g = canvas.graphics_within(scale, &[rect]);
                    self.in_scope(&mut g, |g| self.paint_scope_over(g, meters));
                }
            }
        }
        self.canvas = Some(canvas);
        self.painted = Some((digests.0, digests.1, scale));
        let cv = self.canvas.as_mut().expect("canvas");
        let (w, h) = (cv.width, cv.height);
        Frame::new(cv.opaque_pixels(), w, h)
    }

    fn accessibility_nodes(&self) -> Vec<AccessNode> {
        self.access_items()
            .into_iter()
            .map(|item| match item {
                Hit::Knob(k) => AccessNode {
                    label: PARAMS[KNOBS[k].param].name.into(),
                    value: format_value(KNOBS[k].param, self.v[KNOBS[k].param]),
                    role: AccessRole::Slider,
                    on: false,
                    rect: self.editor_rect(KNOBS[k].parent, KNOBS[k].bounds),
                    focused: false,
                    enabled: self.knob_enabled(k),
                },
                Hit::Key(k) => AccessNode {
                    label: self.key_text(k),
                    value: String::new(),
                    role: AccessRole::Switch,
                    on: self.key_on(k),
                    rect: self.editor_rect(KEYS[k].2, KEYS[k].3),
                    focused: false,
                    enabled: self.key_enabled(k),
                },
                Hit::Drop(d) => AccessNode {
                    label: match DROPS[d].0 {
                        DropId::Algorithm => "Model",
                        DropId::Oversample => "Oversample",
                        DropId::Filter => "Oversample Filter",
                        DropId::Colour => "Colour",
                        DropId::Harmonics => "Harmonics",
                        DropId::Sidechain => "Sidechain",
                        DropId::Meter => "Meter scale",
                    }
                    .into(),
                    value: self.drop_text(d),
                    role: AccessRole::Slider,
                    on: false,
                    rect: self.editor_rect(DROPS[d].1, DROPS[d].2),
                    focused: false,
                    enabled: self.drop_enabled(d),
                },
                _ => unreachable!("only controls are listed"),
            })
            .collect()
    }

    fn accessibility_action(&mut self, node: usize, action: AccessAction) -> bool {
        let Some(&item) = self.access_items().get(node) else { return false };
        match (item, action) {
            (Hit::Knob(k), AccessAction::Increment | AccessAction::Decrement) if self.knob_enabled(k) => {
                // 5 % of the travel, as the SDK's controls step for a screen reader
                let p = KNOBS[k].param;
                let step = if action == AccessAction::Increment { 0.05 } else { -0.05 };
                let target = legal(p, PARAMS[p].from_normalized((proportion(p, self.v[p]) + step).clamp(0.0, 1.0)));
                if target == self.v[p] {
                    return false;
                }
                self.set_param(p, target);
                true
            }
            (Hit::Key(k), AccessAction::Press) if self.key_enabled(k) => {
                self.click_key(k);
                true
            }
            (Hit::Drop(d), AccessAction::Increment | AccessAction::Decrement) if self.drop_enabled(d) => {
                self.step_drop(d, action == AccessAction::Increment);
                true
            }
            _ => false,
        }
    }

    fn modifiers_changed(&mut self, modifiers: Modifiers) {
        self.modifiers = modifiers;
    }

    fn mouse_down(&mut self, x: f32, y: f32, clicks: u32) {
        self.pointer = (x, y);
        let (sx, sy) = self.to_surface(x, y);
        if self.popup.is_some() {
            let inside = self.popup.as_ref().is_some_and(|p| p.bounds.contains(sx, sy));
            self.press = match self.popup_item_at(sx, sy) {
                Some(i) => Press::Popup(Some(i)),
                None if inside => Press::Popup(None), // a separator: the menu stays, as JUCE's did
                None => {
                    self.popup = None;
                    Press::Swallowed
                }
            };
            return;
        }
        let hit = self.hit(x, y);
        self.press = match hit {
            Hit::Knob(k) => {
                let p = KNOBS[k].param;
                if !self.knob_enabled(k) {
                    Press::Swallowed
                } else if clicks >= 2 || self.modifiers.alt {
                    // Slider's double-click (or Option-click) return to the default
                    self.host.begin(p);
                    self.perform(p, PARAMS[p].default);
                    self.host.end(p);
                    Press::Swallowed
                } else {
                    self.host.begin(p);
                    let local = self.knob_local(k, sx, sy);
                    self.press = Press::Knob { k, start: local, last: local, value_on_down: self.v[p], last_dragged: self.v[p] };
                    self.knob_drag(k, local.0, local.1);
                    self.press
                }
            }
            Hit::Key(k) if self.key_enabled(k) => Press::Key(k),
            Hit::Drop(d) if self.drop_enabled(d) => Press::Drop(d),
            Hit::Module(m) => {
                self.bring_to_front(m);
                Press::Module { m, start: (round_to_int(sx), round_to_int(sy)), top_left: (self.modules[m].x, self.modules[m].y), dragged: false }
            }
            Hit::Curve(h) => {
                match h {
                    Handle::Combined => {
                        self.host.begin(idx::THRESHOLD);
                        self.host.begin(idx::RATIO);
                    }
                    Handle::Threshold => self.host.begin(idx::THRESHOLD),
                    Handle::Makeup => self.host.begin(idx::MAKEUP),
                }
                Press::Curve { h, start: (sx, sy), threshold: self.v[idx::THRESHOLD], ratio: self.v[idx::RATIO], makeup: self.v[idx::MAKEUP] }
            }
            Hit::Resizer => Press::Resizer { start: (x, y), size: self.size },
            _ => Press::Swallowed,
        };
    }

    fn mouse_move(&mut self, x: f32, y: f32) {
        self.pointer = (x, y);
        let (sx, sy) = self.to_surface(x, y);
        match self.press {
            Press::None => {
                if self.popup.is_some() {
                    let item = self.popup_item_at(sx, sy);
                    if let Some(p) = self.popup.as_mut() {
                        p.highlighted = item;
                    }
                    self.hover = Hit::Nothing;
                } else {
                    self.hover = self.hit(x, y);
                }
            }
            Press::Knob { k, .. } => {
                let (lx, ly) = self.knob_local(k, sx, sy);
                self.knob_drag(k, lx, ly);
            }
            Press::Key(_) | Press::Drop(_) => self.hover = self.hit(x, y),
            Press::Module { m, start, top_left, dragged } => {
                let delta = (round_to_int(sx) - start.0, round_to_int(sy) - start.1);
                let dragged = dragged || delta.0.abs() >= 4 || delta.1.abs() >= 4;
                if dragged {
                    self.modules[m].x = top_left.0 + delta.0;
                    self.modules[m].y = top_left.1 + delta.1;
                    self.clamp_module(m);
                    self.save_modules();
                }
                self.press = Press::Module { m, start, top_left, dragged };
            }
            Press::Curve { h, start, threshold, ratio, makeup } => {
                let cv = self.curve();
                let span = 18.0f32.max(cv.max_db - cv.min_db);
                let dx = (sx - start.0) * span / cv.area.w;
                let dy = -(sy - start.1) * span / cv.area.h;
                if matches!(h, Handle::Combined | Handle::Threshold) {
                    self.perform(idx::THRESHOLD, (threshold + dx).clamp(-60.0, 0.0));
                }
                if h == Handle::Combined {
                    self.perform(idx::RATIO, ratio + dy * 0.5);
                }
                if h == Handle::Makeup {
                    let p = &PARAMS[idx::MAKEUP];
                    let delta = -(sy - start.1) / cv.area.h * (p.max - p.min);
                    self.perform(idx::MAKEUP, (makeup + delta).clamp(p.min, p.max));
                }
            }
            Press::Resizer { start, size } => {
                let w = (size.0 as f32 + x - start.0).max(1.0) as u32;
                let h = round_to_int(w as f32 * H as f32 / W as f32).max(1) as u32;
                let range = self.size_range();
                self.host.request_resize(&range, w, h);
            }
            Press::Popup(_) => {
                let item = self.popup_item_at(sx, sy);
                if let Some(p) = self.popup.as_mut() {
                    p.highlighted = item;
                }
            }
            Press::Swallowed => {}
        }
    }

    fn mouse_up(&mut self, x: f32, y: f32) {
        let (sx, sy) = self.to_surface(x, y);
        let hit = self.hit(x, y);
        match std::mem::replace(&mut self.press, Press::None) {
            Press::Knob { k, .. } => self.host.end(KNOBS[k].param),
            Press::Key(k) if hit == Hit::Key(k) => self.click_key(k),
            Press::Drop(d) if hit == Hit::Drop(d) => self.open_menu(d),
            Press::Module { m, dragged, .. } => {
                if !dragged {
                    self.modules[m].open = !self.modules[m].open;
                    if self.modules[m].open {
                        self.bring_to_front(m);
                    }
                    self.clamp_module(m);
                    self.save_modules();
                }
            }
            Press::Curve { h, .. } => match h {
                Handle::Combined => {
                    self.host.end(idx::THRESHOLD);
                    self.host.end(idx::RATIO);
                }
                Handle::Threshold => self.host.end(idx::THRESHOLD),
                Handle::Makeup => self.host.end(idx::MAKEUP),
            },
            Press::Popup(Some(i)) => {
                if self.popup_item_at(sx, sy) == Some(i)
                    && let Some(p) = self.popup.take()
                {
                    let (kind, choice) = (p.kind, p.items[i].choice);
                    self.choose(kind, choice);
                }
            }
            _ => {}
        }
        self.hover = if self.popup.is_some() { Hit::Nothing } else { self.hit(x, y) };
    }

    fn wheel(&mut self, x: f32, y: f32, dy: f32) {
        if self.popup.is_some() || !matches!(self.press, Press::None) {
            return;
        }
        let Hit::Knob(k) = self.hit(x, y) else { return };
        if !self.knob_enabled(k) {
            return;
        }
        // JUCE's wheel units on macOS: 0.5 / 256 of a pixel delta
        let amount = dy * 10.0 * 0.5 / 256.0;
        let p = KNOBS[k].param;
        let value = self.v[p];
        let target = PARAMS[p].from_normalized((proportion(p, value) + amount * 0.15).clamp(0.0, 1.0));
        let delta = target - value;
        if delta != 0.0 {
            let step = crate::params::INTERVAL[p].max(delta.abs()) * delta.signum();
            self.host.begin(p);
            self.perform(p, value + step);
            self.host.end(p);
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn numbers_read_as_the_display_rules_ask() {
        assert_eq!(general(50.0), "50");
        assert_eq!(general(55.000001192092896), "55");
        assert_eq!(general(14.4), "14.4");
        assert_eq!(ratio_text(100.0), "\u{221e}:1");
        assert_eq!(ratio_text(legal(idx::RATIO, 23.6)), "24:1");
        assert_eq!(format_value(idx::THRESHOLD, legal(idx::THRESHOLD, -17.6)), "-18 dB");
        assert_eq!(format_value(idx::MAKEUP, legal(idx::MAKEUP, 0.0)), "0 dB");
        assert_eq!(format_value(idx::MAKEUP, legal(idx::MAKEUP, 3.0)), "+3 dB");
        // a 1.5.3 session's tenth is shown as it is, not rounded away
        assert_eq!(format_value(idx::THRESHOLD, crate::params::saved(idx::THRESHOLD, -23.7)), "-23.7 dB");
        assert_eq!(ms_text(0.1), "100 \u{b5}s");
        assert_eq!(ms_text(10.0), "10.0 ms");
        assert_eq!(ms_text(100.0), "100 ms");
        assert_eq!(ms_text(1500.0), "1.50 s");
        assert_eq!(format_value(idx::TRANSIENT_ATTACK, 35.0), "+35%");
        assert_eq!(format_value(idx::TRANSIENT_ATTACK, legal(idx::TRANSIENT_ATTACK, 0.0)), "0%");
        assert_eq!(format_value(idx::H2, 100.0), "100%");
        assert_eq!(format_value(idx::MIX, 1.0), "100%");
        assert_eq!(compact_ms(0.5), "500\u{b5}");
        assert_eq!(compact_ms(10.0), "10.0m");
        assert_eq!(compact_ms(100.0), "100m");
        assert_eq!(scope::signed(-0.4), "0");
        assert_eq!(scope::signed(2.6), "+3");
        // the host's text, and what a host user types
        let attack = &PARAMS[idx::ATTACK];
        assert_eq!(attack.format(0.5), "500 us");
        assert_eq!(attack.parse("500 us"), Some(0.5));
        assert_eq!(attack.parse("12"), Some(12.0));
        assert_eq!(PARAMS[idx::MIX].format(0.5), "50%");
        assert_eq!(PARAMS[idx::MIX].parse("50%"), Some(0.5));
        assert_eq!(PARAMS[idx::THRESHOLD].format(-18.0), "-18");
    }

    use openplugin::core::Gesture;
    use std::cell::RefCell;
    use std::rc::Rc;

    /// An editor bound to a fresh engine at 1120 x 760 points (surface units), and the gestures it sends.
    fn editor() -> (HcEditor, Rc<RefCell<Vec<Gesture>>>) {
        let log = Rc::new(RefCell::new(Vec::new()));
        let sink = log.clone();
        let mut e = HcEditor::new(EditorHost::new(move |g| sink.borrow_mut().push(g)));
        let plugin = crate::HarmonicCompressor::default();
        e.bind(openplugin::prelude::Plugin::editor_state(&plugin));
        for (i, p) in PARAMS.iter().enumerate() {
            e.param_changed(i, p.default);
        }
        (e, log)
    }

    fn click(e: &mut HcEditor, x: f32, y: f32) {
        e.mouse_move(x, y);
        e.mouse_down(x, y, 1);
        e.mouse_up(x, y);
    }

    #[test]
    fn a_key_toggles_its_parameter_in_one_gesture() {
        let (mut e, log) = editor();
        click(&mut e, 929.0, 694.0); // AUTO GAIN
        assert_eq!(*log.borrow(), vec![Gesture::Begin(25), Gesture::Perform(25, 1.0), Gesture::End(25)]);
        assert!(e.key_text(5).starts_with("AUTO "), "the key shows the applied gain");
    }

    #[test]
    fn every_toolbar_control_answers_its_click() {
        let (mut e, log) = editor();
        click(&mut e, 1052.0, 22.0); // COLOR
        assert_eq!(e.popup.as_ref().map(|p| p.kind), Some(MenuKind::Colour));
        click(&mut e, 1052.0, (36 + 2 + 11 + 22) as f32); // Soft White
        assert_eq!(e.ui.colour_mode, 1);
        click(&mut e, 20.0, 20.0); // the title is no control
        assert!(e.popup.is_none());
        click(&mut e, 655.0, 22.0); // OS
        assert_eq!(e.popup.as_ref().map(|p| p.kind), Some(MenuKind::Oversample));
        click(&mut e, 1.0, 400.0); // outside the menu: it closes
        click(&mut e, 730.0, 22.0); // filter
        assert_eq!(e.popup.as_ref().map(|p| p.kind), Some(MenuKind::Filter));
        click(&mut e, 1.0, 400.0);
        click(&mut e, 826.0, 22.0); // TRANSIENT
        click(&mut e, 912.0, 22.0); // CURVE
        assert!(!e.ui.show_curve);
        assert!(log.borrow().contains(&Gesture::Perform(idx::TRANSIENT_ENABLED as u32, 1.0)));
    }

    #[test]
    fn closing_the_editor_keeps_a_look_the_host_loaded() {
        let (e, _) = editor();
        let shared = e.shared.clone().expect("bound");
        shared.ui.lock().expect("ui").colour_mode = 2; // a state load while the editor is open
        drop(e);
        assert_eq!(shared.ui.lock().expect("ui").colour_mode, 2);
    }

    #[test]
    fn a_click_on_a_menu_separator_keeps_the_menu() {
        let (mut e, _) = editor();
        e.set_module_open(1, true);
        let b = e.module_bounds(1);
        let (x, y) = ((SCOPE.x + b.x + HEADER + 50) as f32, (SCOPE.y + b.y + HEADER + 8 + 14) as f32);
        click(&mut e, x, y); // the harmonics selector
        let p = e.popup.as_ref().expect("menu").bounds;
        click(&mut e, (p.x + 40) as f32, (p.y + 2 + 6 * 22 + 5) as f32); // the separator
        assert!(e.popup.is_some());
    }

    #[test]
    fn the_meter_scale_is_chosen_on_the_input_meter_and_saved() {
        let (mut e, _) = editor();
        assert_eq!(e.drop_text(6), "PPM dBFS", "PPM in dBFS by default");
        click(&mut e, (INPUT_METER.x + 40) as f32, (INPUT_METER.y + 33) as f32);
        let p = e.popup.as_ref().expect("meter menu");
        assert_eq!(p.kind, MenuKind::Meter);
        let b = p.bounds;
        click(&mut e, (b.x + 40) as f32, (b.y + 2 + 2 * 22 + 10) as f32); // the third item
        assert_eq!(e.drop_text(6), "VU");
        let saved = e.shared.as_ref().map(|s| s.ui.lock().unwrap().meter);
        assert_eq!(saved, Some(crate::METER_VU as i32), "saved with the session");
        assert!(e.accessibility_nodes().iter().any(|n| n.label == "Meter scale" && n.value == "VU"));
    }

    #[test]
    fn the_saved_size_is_asked_of_the_host_once_the_view_exists() {
        let asked = Rc::new(RefCell::new(Vec::new()));
        let log = asked.clone();
        let mut e = HcEditor::new(EditorHost::detached().with_resize(move |w, h| {
            log.borrow_mut().push((w, h));
            true
        }));
        let plugin = crate::HarmonicCompressor::default();
        let shared = openplugin::prelude::Plugin::editor_state(&plugin).expect("state");
        shared.clone().downcast::<Shared>().expect("shared").ui.lock().expect("ui").editor_width = 800;
        e.bind(Some(shared));
        assert!(asked.borrow().is_empty(), "nothing is asked inside bind");
        e.tick(1.0 / 30.0);
        e.tick(1.0 / 30.0);
        assert_eq!(*asked.borrow(), vec![(800, 543)]);
    }

    #[test]
    fn leaving_the_editor_clears_the_hover() {
        let (mut e, _) = editor();
        e.mouse_move(160.0, 710.0);
        assert_eq!(e.hover, Hit::Knob(2));
        e.mouse_move(-1.0, -1.0); // what the view sends when the pointer leaves
        assert_eq!(e.hover, Hit::Nothing);
    }

    #[test]
    fn a_rotary_knob_follows_a_vertical_drag_of_170_units_per_travel() {
        let (mut e, log) = editor();
        e.mouse_down(160.0, 710.0, 1); // THRESHOLD
        e.mouse_move(160.0, 710.0 - 85.0);
        e.mouse_up(160.0, 625.0);
        let g = log.borrow();
        assert_eq!(g.first(), Some(&Gesture::Begin(1)));
        assert_eq!(g.last(), Some(&Gesture::End(1)));
        // -18 dB is 0.7 of the travel; half the travel up reaches the top
        assert_eq!(e.v[idx::THRESHOLD], 0.0);
    }

    #[test]
    fn a_double_click_returns_to_the_default() {
        let (mut e, log) = editor();
        e.param_changed(idx::THRESHOLD, -40.0);
        e.mouse_down(160.0, 710.0, 2);
        e.mouse_up(160.0, 710.0);
        assert_eq!(*log.borrow(), vec![Gesture::Begin(1), Gesture::Perform(1, -18.0), Gesture::End(1)]);
    }

    #[test]
    fn a_module_opens_on_a_click_and_moves_on_a_drag() {
        let (mut e, _) = editor();
        click(&mut e, 283.0, 74.0); // COMPRESSION header
        assert!(e.modules[0].open);
        assert_eq!(e.module_order[3], 0, "an opened module comes to the front");
        // a linear slider jumps to the click: ATTACK's track starts 6 units in
        let x = (116 + 68 + 10 + 6) as f32;
        e.mouse_down(x, (48 + 12 + 28 + 6 + 30) as f32, 1);
        e.mouse_up(x, 124.0);
        assert_eq!(e.v[idx::ATTACK], PARAMS[idx::ATTACK].min);
        e.mouse_down(283.0, 74.0, 1);
        e.mouse_move(283.0 + 2000.0, 74.0 + 2000.0);
        e.mouse_up(2283.0, 2074.0);
        assert!(e.modules[0].open, "a drag does not toggle");
        assert_eq!((e.modules[0].x, e.modules[0].y), (888 - 406 - 6, 618 - 108 - 6), "kept inside the history field");
    }

    #[test]
    fn choosing_a_model_recalls_its_settings() {
        let (mut e, log) = editor();
        click(&mut e, 526.0, 22.0); // MODEL
        assert!(e.popup.is_some());
        click(&mut e, 500.0, (38 + 3 * 22 + 11) as f32); // Bus VCA
        assert!(e.popup.is_none());
        let g = log.borrow();
        assert_eq!(g[..3], [Gesture::Begin(0), Gesture::Perform(0, 3.0), Gesture::End(0)]);
        // the model's own attack, release and knee (1.5.3's mode defaults) follow
        let (attack, release, _) = crate::dsp::mode_defaults(3);
        assert!(g.contains(&Gesture::Perform(idx::ATTACK as u32, legal(idx::ATTACK, attack))));
        assert!(g.contains(&Gesture::Perform(idx::RELEASE as u32, legal(idx::RELEASE, release))));
    }

    #[test]
    fn an_idle_editor_paints_nothing() {
        let (mut e, _) = editor();
        e.render(2.0);
        for _ in 0..5 {
            e.tick(1.0 / 30.0);
            assert_eq!(e.damage().map(|d| d.len()), Some(0), "digests {:?}", e.digests());
            e.render(2.0);
        }
    }

    #[test]
    fn a_screen_reader_finds_and_works_the_controls() {
        let (mut e, log) = editor();
        let nodes = e.accessibility_nodes();
        let threshold = nodes.iter().position(|n| n.label == "Threshold").expect("threshold node");
        assert_eq!(nodes[threshold].value, "-18 dB");
        assert!(e.accessibility_action(threshold, AccessAction::Increment));
        assert_eq!(log.borrow()[0], Gesture::Begin(1));
        let delta = nodes.iter().position(|n| n.label == "DELTA").expect("delta node");
        assert!(e.accessibility_action(delta, AccessAction::Press));
        assert!(e.on(idx::DELTA));
        let model = nodes.iter().position(|n| n.label == "Model").expect("model node");
        assert!(e.accessibility_action(model, AccessAction::Increment));
        assert_eq!(e.mode(), 1);
        assert!(!nodes.iter().any(|n| n.label == "Attack"), "closed modules are not listed");
    }

    /// Repainting only the changed regions (with the history field's cached underlay) gives the same
    /// pixels as painting everything, to CoreGraphics' gradient dither: it depends on the area being
    /// repainted (as in 1.5.3's own partial repaints), one level per gradient, two where the header deck's
    /// translucent gradient lies over the chassis gradient.
    #[test]
    fn partial_repaints_equal_a_whole_repaint() {
        use openplugin::prelude::Plugin;
        let mut plugin = crate::HarmonicCompressor::default();
        plugin.prepare(48000.0, 1600, 2);
        let mut e = HcEditor::new(EditorHost::detached());
        e.bind(plugin.editor_state());
        e.render(2.0);
        let mut n = 0u32;
        for frame in 0..90 {
            let mut l: Vec<f32> = (0..1600).map(|_| {
                n += 1;
                0.4 * (n as f32 * 0.05).sin() * if (n / 24000) % 2 == 0 { 1.0 } else { 0.1 }
            }).collect();
            let mut r = l.clone();
            plugin.process(&mut [l.as_mut_slice(), r.as_mut_slice()], 1600);
            e.tick(1.0 / 30.0);
            if frame == 40 {
                e.mouse_move(160.0, 710.0); // hover a knob: the knob bar repaints too
            }
            let partial = e.render(2.0).pixels.to_vec();
            if frame % 15 == 14 {
                e.invalidate();
                let whole = e.render(2.0).pixels.to_vec();
                let diff: Vec<(usize, usize, u32)> = partial
                    .iter()
                    .zip(&whole)
                    .enumerate()
                    .filter(|(_, (a, b))| a != b)
                    .map(|(i, (a, b))| {
                        let d = (0..3).map(|c| ((a >> (8 * c)) & 255).abs_diff((b >> (8 * c)) & 255)).max().unwrap_or(0);
                        (i % 2240, i / 2240, d)
                    })
                    .collect();
                if diff.iter().any(|d| d.2 > 2) {
                    let (x0, x1) = (diff.iter().map(|d| d.0).min().unwrap(), diff.iter().map(|d| d.0).max().unwrap());
                    let (y0, y1) = (diff.iter().map(|d| d.1).min().unwrap(), diff.iter().map(|d| d.1).max().unwrap());
                    let worst = diff.iter().map(|d| d.2).max().unwrap();
                    panic!("frame {frame}: {} pixels differ (worst {worst}) in x {x0}..{x1}, y {y0}..{y1}", diff.len());
                }
            }
        }
    }

    /// plugin-qa rule 2, the GUI leg: every parameter has a control (knob, key, or a selector that
    /// writes it), as in 1.5.3.
    #[test]
    fn every_parameter_has_a_control() {
        let mut covered = [false; COUNT];
        for k in &KNOBS {
            covered[k.param] = true;
        }
        for (key, ..) in KEYS {
            match key {
                KeyId::AutoAttack => covered[idx::AUTO_ATTACK] = true,
                KeyId::AutoRelease => covered[idx::AUTO_RELEASE] = true,
                KeyId::TransientHeader | KeyId::TransientModule => covered[idx::TRANSIENT_ENABLED] = true,
                KeyId::AutoGain => covered[idx::AUTO_GAIN] = true,
                KeyId::Delta => covered[idx::DELTA] = true,
                KeyId::Curve => {} // a view option, not a parameter
            }
        }
        for (d, ..) in DROPS {
            match d {
                DropId::Algorithm => covered[idx::MODE] = true,
                DropId::Oversample => covered[idx::OVERSAMPLE_MODE] = true,
                DropId::Filter => covered[idx::OVERSAMPLE_FILTER] = true,
                DropId::Sidechain => {
                    covered[idx::SC_ENABLED] = true;
                    covered[idx::SC_LISTEN] = true;
                }
                DropId::Harmonics | DropId::Colour | DropId::Meter => {} // presets over the 2nd..5th knobs; the look; the meter scale
            }
        }
        let missing: Vec<&str> = (0..COUNT).filter(|&i| !covered[i]).map(|i| PARAMS[i].id).collect();
        assert!(missing.is_empty(), "parameters with no control: {missing:?}");
    }

    /// plugin-qa display rules 1 to 4 on every control, over its whole travel: at most three
    /// digits, no 0.x value, whole steps on the dynamics controls; the host's text the same.
    #[test]
    fn every_control_meets_the_display_rules() {
        let dynamics = [idx::THRESHOLD, idx::RATIO, idx::KNEE, idx::MAKEUP, idx::INPUT_GAIN, idx::OUTPUT_GAIN];
        for k in &KNOBS {
            let p = &PARAMS[k.param];
            let mut ends = Vec::new();
            for i in 0..=400 {
                let v = legal(k.param, p.from_normalized(i as f32 / 400.0));
                for text in [format_value(k.param, v), p.format(v)] {
                    let digits = text.chars().filter(char::is_ascii_digit).count();
                    assert!(digits <= 3, "{}: {text:?} has {digits} digits", p.id);
                    let number = text.trim_start_matches(['-', '+']);
                    assert!(!number.starts_with("0."), "{}: {text:?} is a 0.x value", p.id);
                    if dynamics.contains(&k.param) {
                        assert!(!text.contains('.'), "{}: {text:?} is not a whole step", p.id);
                    }
                }
                if i % 200 == 0 {
                    ends.push(format_value(k.param, v));
                }
            }
            println!("{:<18} {}  (default {})", p.id, ends.join(" | "), format_value(k.param, legal(k.param, p.default)));
        }
    }

    /// The text audit: no text of the editor is set under 8 pt (1.5.3 had labels at 7.2 to 7.9).
    #[test]
    fn no_text_is_smaller_than_8_pt() {
        for (file, src) in [("mod.rs", include_str!("mod.rs")), ("scope.rs", include_str!("scope.rs")), ("widgets.rs", include_str!("widgets.rs"))] {
            // a call's own arguments, up to its closing bracket; not this test's quoted pattern
            for (n, line) in src.lines().enumerate().filter(|(_, l)| l.contains("Font::new(") && !l.contains("\"Font::new(")) {
                let args = line.split("Font::new(").nth(1).and_then(|a| a.split(')').next()).unwrap_or("");
                let sizes = args.split(|c: char| !(c.is_ascii_digit() || c == '.')).filter_map(|t| t.parse::<f32>().ok()).filter(|v| *v >= 1.0);
                for size in sizes {
                    assert!(size >= 8.0, "{file}:{}: text at {size} pt", n + 1);
                }
            }
        }
    }

    #[test]
    fn the_editor_renders_its_canvas() {
        let mut e = HcEditor::new(EditorHost::detached());
        let f = e.render(1.0);
        assert_eq!((f.width, f.height), (1120, 760));
        assert!(f.pixels.iter().any(|&p| p != f.pixels[0]));
    }
}
