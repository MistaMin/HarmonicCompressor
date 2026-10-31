# Additional compressor mode integration

User requested adding the optical leveler and RMS/VCA references. Added modes at indices 8 and 9; old indices unchanged. Both explicitly provisional in host parameter choices.

| Item | Status | Evidence |
|---|---|---|
| Independent optical engine with three recovery states | Implemented; fidelity unverified | CompressorCore.h, additional-reference-hashes.json |
| Independent RMS feed-forward engine with coupled timing | Implemented; fidelity unverified | CompressorCore.h, additional-reference-hashes.json |
| Host choices, menu labels, colours, pool/defaults | Implemented; runtime menu unverified | PluginProcessor.cpp, PluginEditor.cpp |
| Circuit-defined timing/knee controls disabled | Implemented; runtime GUI unverified | applyModeLook |
| Burst, recovery, reset at 44.1/48/96k | Pass | model-smoke.cpp, model-smoke.txt |
| Full circuit stages and SPICE comparison | Deferred, incomplete | dsp/CIRCUIT_INTEGRATION.md |

Smoke check is a numerical harness, not a DAW test or sound acceptance. Test sine frequency changes with sample rate, so results do not establish cross-rate fidelity. Existing transfer graph remains a generic static illustration, not a measured curve for these models. GUI polish requested earlier remains separate unfinished work.
