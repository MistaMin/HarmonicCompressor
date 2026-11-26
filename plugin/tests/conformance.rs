//! TS-1: the conformance kit on this plugin. Debug builds run Options::quick() (the Stop hook);
//! release builds run every rate and block size (the commit tester).

mod common;

use harmonic_compressor::HarmonicCompressor;
use openplugin::testing::{Options, conformance};

openplugin::testing::install_allocator!();

#[test]
fn conforms() {
    let base = if cfg!(debug_assertions) { Options::quick() } else { Options::default() };
    conformance(HarmonicCompressor::default, common::options(base)).assert_pass();
}
