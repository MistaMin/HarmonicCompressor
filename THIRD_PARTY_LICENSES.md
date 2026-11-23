# Third-party notices

Harmonic Compressor source is MIT licensed. Compiled-binary terms are in
LICENSE-BINARIES.txt. Third-party components retain their own terms.

| Component | Use | License notice |
|---|---|---|
| JUCE | Plugin framework and format wrappers | Licenses/third-party/JUCE-LICENSE.md |
| GoodLookinUI | Interface toolkit | Licenses/third-party/GoodLookinUI-LICENSE.txt |
| clap-juce-extensions | CLAP wrapper | Licenses/third-party/clap-juce-extensions-LICENSE.md |
| CLAP | Plugin interface | Licenses/third-party/CLAP-LICENSE.txt |
| AAX SDK (Avid) | AAX Native format | Avid AAX Developer Agreement / SDK License; Licenses/third-party/AAX-SDK-LICENSE.txt |

The AAX SDK implementation is not distributed in this repository. AAX and Pro Tools
are trademarks of Avid Technology, Inc. PACE signing does not grant a license to
redistribute the SDK.

The JUCE, GoodLookinUI and clap-juce-extensions revisions of each release build are recorded in its QA folder (`qa/<date>-release-<version>/build-env.txt`).
The CLAP extension revision is recorded in CMakeLists.txt.
