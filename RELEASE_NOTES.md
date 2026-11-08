# Harmonic Compressor 1.5.1

- Fixed linear sliders jumping to the maximum when clicked. The clickable travel now follows the painted groove, and values can be dragged back down from maximum.
- Added a regression test against the actual editor and parameter attachments: all 13 linear sliders, four themes and editor reopening.
- Corrected the manual to document all 25 host parameters, actual ranges and defaults, the post-mix transient stage, oversampling choices and movable modules.
- Added the Pro Tools control-surface page table, so AAX control surfaces expose the plugin's parameters.
- Preserved parameter identifiers and saved-state layout.
- Installer upgrades now replace old bundles cleanly, removing obsolete resources that could invalidate a bundle's code signature.

macOS: universal Apple silicon/Intel build (arm64 and x86_64), macOS 11 or later. Plugin Manager package: VST3, AU, CLAP, LV2 and PACE-signed AAX. This plugin ships no Standalone app. Windows and Linux builds are listed only after their native builds and tests pass on their own machines.

Approved binary downloads are published on the GitHub release page and through the OpenGrid Plugin Manager.
