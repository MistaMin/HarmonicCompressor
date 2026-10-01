# Harmonic Compressor

A multi-mode (Opto / VCA / FET / Red / Hybrid) harmonic-coloring compressor
plugin, built with JUCE. Ships as VST3, AU, and a Standalone app on macOS.

## Building

```
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

A packaged macOS installer can be produced with `Scripts/build_installer.sh`.

## Licensing

The source code in this repository (everything outside the `JUCE/`
submodule) is released under the **MIT License** — see [LICENSE](LICENSE).

This project depends on the [JUCE](https://juce.com) framework, which is
**dual-licensed** by Raw Material Software Limited and is **not** covered by
this repository's MIT license. If you clone this repository and compile the
plugin yourself, the resulting binary is subject to whichever JUCE license
you build it under:

- **AGPLv3** — if you build JUCE under its open-source license, any binary
  you distribute must also comply with the AGPLv3 (including making your
  own source available).
- **JUCE Commercial license** (Starter / Indie / Pro / Educational) — if you
  hold one of JUCE's commercial licenses, your binary follows that license's
  terms instead. Note that the free **Starter** tier requires the JUCE
  splash screen to be shown on launch; this repository does not disable it
  (no `JUCE_DISPLAY_SPLASH_SCREEN` override is set), so builds from this
  source remain Starter-license compliant out of the box. Removing the
  splash screen requires upgrading to an Indie or Pro JUCE license.

In short: the Harmonic Compressor code itself is free to use, modify, and
redistribute under MIT, but JUCE's own license terms travel with any binary
you compile from it.
