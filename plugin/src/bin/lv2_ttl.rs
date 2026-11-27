//! Writes the LV2 description files into a bundle folder (the SDK's build script runs it):
//! `lv2_ttl <folder> <library stem>` gives `manifest.ttl` and `<stem>.ttl`.
use harmonic_compressor::HarmonicCompressor;

fn main() {
    let args: Vec<String> = std::env::args().collect();
    let (Some(dir), Some(stem)) = (args.get(1), args.get(2)) else {
        eprintln!("usage: lv2_ttl <folder> <library stem>");
        std::process::exit(2);
    };
    #[cfg(target_os = "macos")]
    let editor = Some(harmonic_compressor::editor::make_editor as _);
    #[cfg(not(target_os = "macos"))]
    let editor = None;
    if let Err(e) = openplugin::targets::lv2::write_bundle_for_editor(&HarmonicCompressor::default(), std::path::Path::new(dir), stem, editor) {
        eprintln!("{dir}: {e}");
        std::process::exit(1);
    }
}
