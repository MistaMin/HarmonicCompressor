# Unreleased

- Added Auto Gain: matches the output to the input's K-weighted short-term loudness (ITU-R BS.1770, 3 s window), gated at -70 LUFS, +/-24 dB range. The AUTO GAIN key shows the applied gain.
- Added Delta: plays the output minus the delay-compensated dry signal (the untouched input while Auto Gain is on).
- Two host parameters appended (`autoGain`, `delta`); existing parameter identifiers and saved sessions are unchanged, and older sessions load with both off.
- The four main knobs are slightly narrower to make room for the new keys.
- Fixed parallel Mix with oversampling on: the dry signal now passes through the same oversampling filters as the processed signal. Previously an interpolated delay stood in for them, which dulled the dry signal (-2 dB at 10 kHz, up to -8 dB at 18 kHz) and, in Min Phase modes, caused phase cancellation (Mix 50 %: up to -10.7 dB at 18 kHz). Only 2x Lin Phase was unaffected. Oversampled modes now use more CPU (8x Lin Phase: 0.49 to 0.67 ms per 256-sample block at 48 kHz).
- Fixed a possible crash when a host removed the plugin right after a model or oversampling change. Model and oversampling changes from automation no longer allocate memory on the calling thread.

# Harmonic Compressor 1.5.1

- Fixed linear sliders jumping to the maximum when clicked. The clickable travel now follows the painted groove, and values can be dragged back down from maximum.
- Fixed a race where an algorithm-mode switch queued just before a session restore could overwrite the restored parameters.
- Added a regression test against the actual editor and parameter attachments: all 13 linear sliders, four themes and editor reopening.
- Corrected the manual to document all 25 host parameters, actual ranges and defaults, the post-mix transient stage, oversampling choices and movable modules.
- Added the Pro Tools control-surface page table, so AAX control surfaces expose the plugin's parameters.
- Preserved parameter identifiers and saved-state layout.
- Installer upgrades now replace old bundles cleanly, removing obsolete resources that could invalidate a bundle's code signature.

macOS: universal Apple silicon/Intel build (arm64 and x86_64), macOS 11 or later. Plugin Manager package: VST3, AU, CLAP, LV2 and PACE-signed AAX. This plugin ships no Standalone app. Windows and Linux builds are listed only after their native builds and tests pass on their own machines.

Approved binary downloads are published on the GitHub release page and through the OpenGrid Plugin Manager.
