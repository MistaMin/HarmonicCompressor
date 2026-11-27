//! Renders the editor in each state of qa/2026-11-26-rust-port/gui/scenarios.txt to <out>/<name>.ppm,
//! for compare.py to set against 1.5.3's renders.
//!   cargo run --release --example gui_render -- <scenarios.txt> <out dir>

#[cfg(target_os = "macos")]
fn main() {
    use harmonic_compressor::editor::HcEditor;
    use harmonic_compressor::params::PARAMS;
    use harmonic_compressor::{HarmonicCompressor, Shared};
    use openplugin::core::{Editor, EditorHost};
    use openplugin::prelude::*;
    use std::io::Write;

    let args: Vec<String> = std::env::args().collect();
    let (list, out) = (&args[1], std::path::Path::new(&args[2]));
    std::fs::create_dir_all(out).expect("output folder");
    for line in std::fs::read_to_string(list).expect("scenarios").lines() {
        let line = line.trim();
        if line.is_empty() || line.starts_with('#') {
            continue;
        }
        let mut words = line.split_whitespace();
        let name = words.next().expect("name");
        let mut plugin = HarmonicCompressor::default();
        let state = plugin.editor_state().expect("editor state");
        let shared = state.clone().downcast::<Shared>().expect("shared state");
        let (mut scale, mut open, mut audio) = (2.0f32, Vec::new(), 0.0f32);
        for w in words {
            let (key, value) = w.split_once('=').expect("key=value");
            if key == "audio" {
                audio = value.parse().expect("seconds");
            } else if key == "scale" {
                scale = value.parse().expect("scale");
            } else if key == "open" {
                open = value.split(',').map(str::to_string).collect();
            } else if let Some(prop) = key.strip_prefix("ui.") {
                shared.ui.lock().expect("ui").set(prop, value);
            } else if !plugin.set_param(key, value.parse().expect("value")) {
                eprintln!("{name}: unknown setting {w}");
            }
        }
        let mut editor = HcEditor::new(EditorHost::detached());
        editor.bind(Some(state));
        for (i, p) in PARAMS.iter().enumerate() {
            editor.param_changed(i, plugin.param(p.id).expect("value"));
        }
        for (m, key) in ["comp", "harm", "sc", "trans"].iter().enumerate() {
            if open.iter().any(|o| o == key) {
                editor.set_module_open(m, true);
            }
        }
        // decaying 220 Hz bursts every 0.85 s, as 1.5.3's manual pictures were made, with the editor
        // ticking every block as a host's 30 Hz timer would
        if audio > 0.0 {
            plugin.prepare(48000.0, 512, 2);
            let (sr, period) = (48000.0f64, 0.85 * 48000.0);
            let mut n = 0u64;
            for _ in 0..(audio as f64 * sr / 512.0) as usize {
                let mut left = vec![0.0f32; 512];
                for x in left.iter_mut() {
                    let t = (n as f64 % period) / sr;
                    let env = 0.035 + 0.085 * (-t / 0.15).exp();
                    *x = (env * (2.0 * std::f64::consts::PI * 220.0 * n as f64 / sr).sin()) as f32;
                    n += 1;
                }
                let mut right = left.clone();
                plugin.process(&mut [left.as_mut_slice(), right.as_mut_slice()], 512);
                editor.tick(512.0 / 48000.0);
            }
        }
        let frame = editor.render(scale);
        let mut ppm = format!("P6\n{} {}\n255\n", frame.width, frame.height).into_bytes();
        for p in frame.pixels {
            ppm.extend_from_slice(&[(p >> 16) as u8, (p >> 8) as u8, *p as u8]);
        }
        std::fs::File::create(out.join(format!("{name}.ppm"))).and_then(|mut f| f.write_all(&ppm)).expect("write");
        println!("{name} {}x{}", frame.width, frame.height);
    }
}

#[cfg(not(target_os = "macos"))]
fn main() {}
