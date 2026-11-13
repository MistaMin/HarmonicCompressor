# AAX test.page_table.automation_list — investigation report (2026-11-08)

## Result
**Fixed.** The AAX Validator 2024.6 test `test.page_table.automation_list` now **PASSES** on the rebuilt bundle.
The full default suite (describe_validation, parameters, parameter_traversal.random.fast, page_table.load,
page_table.automation_list, load_unload) passes too. Log: `final-rebuilt/`.

## Root cause
The page table `Resources/HarmonicCompressor.xml` had only `<PageTableLayouts>`. It had no `<Editor>` block with a
`<DiscCtrls>` list. The validator's `AAXHShell_GetNumControlNamesForId` fails when that block is missing.
The cell contents and parameter IDs were never the problem. That is why every cell and ID variation tried earlier still failed.

Evidence (each run is in its own folder here; the XML used is in `exp*.xml`):

| Run | Page XML | automation_list |
|---|---|---|
| exp0-baseline-tmp | as shipped (layouts only) | FAIL |
| expA | + empty `<ControlNamesVariations>` | FAIL |
| expB2 | + ControlNamesVariations + `<Editor>`/PluginList, **no DiscCtrls** | FAIL |
| expB | + ControlNamesVariations + Editor with DiscCtrls (ArcVu format) | PASS |
| expB3 | Editor + DiscCtrls with only `MasterBypassID` | PASS |
| expB4 | Editor + DiscCtrls without `MasterBypassID` | PASS |
| expC-nocnv | Editor + DiscCtrls, no ControlNamesVariations | PASS |
| expC-full | Editor + DiscCtrls + empty ControlNamesVariations (shipped) | PASS |

The passing sibling ArcVu (`/Volumes/Developer/OpenGrid/ArcVu/Resources/ArcVu.xml`) and RadarVU have this
`<Editor><DiscCtrls>` block. The validator's own warning checks the parameter IDs: it lists all 25 APVTS IDs plus
`MasterBypassID` as registered automatable parameters. So the managed string IDs in the XML match what the wrapper
registers (hypotheses 1 and 2 ruled out).

## Change (only file changed: `Resources/HarmonicCompressor.xml`)
Appended after `</PageTableLayouts>`, in ArcVu's format:
- an empty `<ControlNamesVariations>` element
- an `<Editor vers='1.3.7.1'>` block containing a `<PluginList>` (jcbb mono, jccc stereo) and a `<DiscCtrls>` list. The list has the discrete parameters (algorithmMode, scEnabled, scListen, autoRelease, autoAttack, oversampleMode, oversampleFilter, transientEnabled) plus `MasterBypassID`.

Parameter IDs, saved-state layout and C++ source are unchanged. No other source or CMake edits. Nothing was committed, signed or installed.

## Rebuild
`cmake --build build-release --target HarmonicCompressor_AAX` (log `build-aax.log`). The XML is not a build
dependency, so ninja reported "no work to do". The POST_BUILD copy only runs on relink, so I deleted the built binary
in build output to force a relink. Bundle XML now equals the source XML (cmp).
**Build-system note:** editing the XML alone does not update the bundle. Consider making the XML a dependency of the copy step.

## Environment issue found (not the plugin)
Since some time after 21:11, the validator's AAXHostShell gives "Plug-in loading failed" (E_ABORTED) for **any**
unsigned build bundle under `/Volumes/Developer`. This happens with our bundle and with `RadarVU/build-qa`
(`control-radarvu-buildqa`). The same bundle copied with `ditto` to `/tmp/hc-aax-copy` loads and passes. Installed
plugins in `/Library` load fine (`control-arcvu`, `control-installed-hc`). `dlopen` of the binary from a shell works.
I did not find the cause. All experiments and the final run therefore used the `/tmp` copy of the rebuilt build output (`final-inplace` shows the in-place abort).

## Not verified
- The installed/signed plugin in `/Library` was not updated or re-tested with the new page table. It still has no page table, so `page_table.load` fails there (`control-installed-hc`).
- Pro Tools control-surface behaviour was not tested.
- The cause of the `/Volumes/Developer` load failure.
- No other plugin format suites were run. LV2 FAIL and the OpenGrid9 route are untouched.
