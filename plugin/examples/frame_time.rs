//! Time per editor frame as the view's 30 Hz timer drives it (tick, damage, render), idle and with
//! audio running (meters, history and the curve's live dot move every frame).
//!   cargo run --release --example frame_time -- [scale]
#[cfg(target_os = "macos")]
fn main() {
    use harmonic_compressor::HarmonicCompressor;
    use harmonic_compressor::editor::HcEditor;
    use openplugin::core::{Editor, EditorHost};
    use openplugin::prelude::*;
    // CPU time of this thread: wall time is meaningless on a loaded machine
    fn cpu() -> f64 {
        let mut t = libc_timespec { tv_sec: 0, tv_nsec: 0 };
        unsafe { clock_gettime(16, &mut t) }; // CLOCK_THREAD_CPUTIME_ID on macOS
        t.tv_sec as f64 + t.tv_nsec as f64 * 1e-9
    }
    #[repr(C)]
    struct libc_timespec {
        tv_sec: i64,
        tv_nsec: i64,
    }
    unsafe extern "C" {
        fn clock_gettime(clock: u32, t: *mut libc_timespec) -> i32;
    }
    let scale: f32 = std::env::args().nth(1).and_then(|s| s.parse().ok()).unwrap_or(2.0);
    let mut plugin = HarmonicCompressor::default();
    plugin.prepare(48000.0, 1600, 2);
    let mut e = HcEditor::new(EditorHost::detached());
    e.bind(plugin.editor_state());
    e.render(scale);
    let n = 30;
    let start = cpu();
    for _ in 0..n {
        e.invalidate();
        e.render(scale);
    }
    println!("whole repaint {:.2} ms CPU at {scale}x", (cpu() - start) * 1000.0 / n as f64);
    for audio in [false, true] {
        // audio: 4 s of warm-up first, so the history's auto-scale has settled as in steady playback
        let (warm, n) = (if audio { 120 } else { 0 }, 90);
        let mut phase = 0.0f32;
        let mut start = cpu();
        for frame in 0..warm + n {
            if frame == warm {
                start = cpu();
            }
            if audio {
                let mut l: Vec<f32> = (0..1600).map(|i| 0.3 * ((phase + i as f32) * 0.03).sin()).collect();
                phase += 1600.0;
                let mut r = l.clone();
                plugin.process(&mut [l.as_mut_slice(), r.as_mut_slice()], 1600);
            }
            e.tick(1.0 / 30.0);
            let d = e.damage();
            if std::env::var("DBG").is_ok() { eprintln!("{:?}", d.as_ref().map(|d| d.len())); }
            if d.is_none_or(|d| !d.is_empty()) {
                e.render(scale);
            }
        }
        let ms = (cpu() - start) * 1000.0 / n as f64;
        println!("{} {ms:.2} ms CPU per frame at {scale}x", if audio { "audio" } else { "idle " });
    }
}
#[cfg(not(target_os = "macos"))]
fn main() {}
