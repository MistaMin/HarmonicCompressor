# Third-party notices

Harmonic Compressor source is MIT licensed. Compiled-binary terms are in
LICENSE-BINARIES.txt. Third-party components retain their own terms.

Since 1.6.0 the binaries are built from the Rust crate in `plugin/`:

| Component | Use | License notice |
|---|---|---|
| OpenPluginSDK (`openplugin` and its crates, GoodLookinDri) | Plugin framework, format shells, editor hosting, meters | MIT or Apache-2.0; Licenses/third-party/OpenPluginSDK-LICENSE-MIT.txt, OpenPluginSDK-LICENSE-APACHE.txt |
| VST 3 SDK (Steinberg) | VST3 format | MIT; Licenses/third-party/VST3-SDK-LICENSE.txt |
| AudioUnitSDK (Apple) | Audio Unit format | Apache-2.0; Licenses/third-party/AudioUnitSDK-LICENSE.txt |
| CLAP | Plugin interface | MIT; Licenses/third-party/CLAP-LICENSE.txt |
| LV2 | Plugin interface (implemented in OpenPluginSDK; no LV2 source is distributed) | ISC, lv2plug.in |
| AAX SDK (Avid) | AAX Native format | Avid AAX Developer Agreement / SDK License; Licenses/third-party/AAX-SDK-LICENSE.txt |
| Rust crates linked through OpenPluginSDK: ahash, allocator-api2, cfg-if, fontdue, hashbrown, itoa, log, memchr, num-complex, num-integer, num-traits, once_cell, primal-check, rustfft, serde, serde_core, serde_json, strength_reduce, transpose, ttf-parser, zerocopy, zmij | Support code | MIT or Apache-2.0 (fontdue also Zlib; memchr Unlicense or MIT; zerocopy also BSD-2-Clause; zmij MIT); texts in Licenses/third-party/Rust-crates-LICENSES.txt |

The list of crates is what `cargo tree -e normal` shows for `plugin/`; `plugin/Cargo.lock` pins
their versions. The OpenPluginSDK revision of each release build is recorded in its QA folder.

The source of 1.5.3 and earlier (`Source/`, `CMakeLists.txt`) is kept in the repository and uses:

| Component | Use | License notice |
|---|---|---|
| JUCE | Plugin framework and format wrappers | Licenses/third-party/JUCE-LICENSE.md |
| GoodLookinUI | Interface toolkit | Licenses/third-party/GoodLookinUI-LICENSE.txt |
| clap-juce-extensions | CLAP wrapper | Licenses/third-party/clap-juce-extensions-LICENSE.md |

The AAX SDK implementation is not distributed in this repository. AAX and Pro Tools
are trademarks of Avid Technology, Inc. PACE signing does not grant a license to
redistribute the SDK. VST is a registered trademark of Steinberg Media Technologies GmbH.
Audio Units is a trademark of Apple Inc.
