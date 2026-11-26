//! What is true of this plugin for the conformance kit, shared by tests/conformance.rs and tests/soak.rs.

use openplugin::testing::Options;

pub fn options(base: Options) -> Options {
    Options {
        // the oversampling factor and filter set the latency, as in 1.5.3
        latency_depends_on_params: true,
        // The designed gain range, as in 1.5.3, which has no output clip: input gain, makeup and
        // output gain +24 dB each, the transient designer +18 dB and VCA 160's +6 dB, about +96 dB
        // over a full-scale input. Instability is caught by finite_output and the tail check.
        max_output: 2.0e5,
        ..base
    }
}
