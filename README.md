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

### If you're just installing the binaries (the .pkg installer / Standalone app)

The compiled plugin and app are distributed as **freeware**: free to use,
personally or commercially, at no cost, with no registration required.
A plain-language copy of this is also installed alongside the app — see
`LICENSE-BINARIES.txt` in this repository, which is copied into
`/Applications` by the installer so it's available on disk after install.
The binaries carry no warranty of any kind.

### If you're building from source

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

The official pre-built binaries distributed from this repository's GitHub
Releases are built under a JUCE Starter license and released as freeware
under the terms above (MIT code + JUCE Starter splash screen + no warranty).

In short: the Harmonic Compressor code itself is free to use, modify, and
redistribute under MIT, but JUCE's own license terms travel with any binary
you compile from it — and the official pre-built binaries are free to use
as-is.
