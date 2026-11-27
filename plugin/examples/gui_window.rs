//! Opens the editor in a window of its own (the SDK's Cocoa view, as hosts get it) with decaying tone
//! bursts running through the engine, and quits after `seconds` (default: stays open).
//!   cargo run --release --example gui_window -- [seconds]
#[cfg(target_os = "macos")]
fn main() {
    use harmonic_compressor::HarmonicCompressor;
    use harmonic_compressor::editor::HcEditor;
    use openplugin::core::{Editor, EditorHost};
    use openplugin::prelude::*;
    use std::cell::RefCell;
    use std::rc::Rc;

    let seconds: Option<f64> = std::env::args().nth(1).and_then(|s| s.parse().ok());
    let plugin = Rc::new(RefCell::new(HarmonicCompressor::default()));
    plugin.borrow_mut().prepare(48000.0, 1600, 2);
    // gestures go straight to the engine, as a host would send them back
    let target = plugin.clone();
    let mut editor = HcEditor::new(EditorHost::new(move |g| {
        if let openplugin::core::Gesture::Perform(i, v) = g {
            target.borrow_mut().set_param_index(i as usize, v);
        }
    }));
    editor.bind(plugin.borrow().editor_state());
    for (i, p) in harmonic_compressor::params::PARAMS.iter().enumerate() {
        editor.param_changed(i, p.default);
    }
    let start = std::time::Instant::now();
    let mut n = 0u64;
    let poll = Box::new(move |e: &mut dyn Editor| {
        let sr = 48000.0f64;
        let mut l = vec![0.0f32; 1600];
        for x in l.iter_mut() {
            let t = (n as f64 % (0.85 * sr)) / sr;
            *x = ((0.035 + 0.085 * (-t / 0.15).exp()) * (2.0 * std::f64::consts::PI * 220.0 * n as f64 / sr).sin()) as f32;
            n += 1;
        }
        let mut r = l.clone();
        let mut p = plugin.borrow_mut();
        p.process(&mut [l.as_mut_slice(), r.as_mut_slice()], 1600);
        for (i, info) in harmonic_compressor::params::PARAMS.iter().enumerate() {
            e.param_changed(i, p.param(info.id).unwrap_or(info.default));
        }
        if seconds.is_some_and(|s| start.elapsed().as_secs_f64() > s) {
            std::process::exit(0);
        }
    });
    std::process::exit(openplugin::dri::cocoa::run_window("Harmonic Compressor", Box::new(editor), Some(poll)));
}
#[cfg(not(target_os = "macos"))]
fn main() {}
