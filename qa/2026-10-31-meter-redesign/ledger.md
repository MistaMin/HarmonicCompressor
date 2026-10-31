# GUI and metering redesign

Reference direction: WaterChannel compressor screenshot (pale blue panels, fine graph grids, compact controls); public compressor product page supplied by user for scrolling signal/GR presentation. Original drawing code; no reference artwork embedded.

| Item | Implementation | Evidence / status |
|---|---|---|
| Remove VU faces | UI/MeterPanel.h replaced | Screenshot inspected in standalone |
| Tall input left | inputMeter, stereo peak bars | Screenshot inspected |
| Tall output and GR right | meterPanel, stereo peaks + downward GR | Screenshot inspected |
| Input/output history with GR | CompressionScope and processor HistoryFrame FIFO | Built; offline telemetry harness result stored in workspace |
| Preserve model selector and three handles | Editor and TransferCurveDisplay retained | Visible in screenshot; interaction not certified |
| Preserve all parameter controls | Existing attachments retained | Controls visible; audio behavior unchanged except telemetry collection |

History uses audio-block input/output peak envelopes and maximum applied wet-path GR, aggregated into >=10ms bins over six seconds. It is not a sample waveform. Output is post makeup/harmonics/mix; GR describes the wet compression stage. FIFO is bounded, allocation-free on write and drops telemetry on overflow; the UI clears history after a drop. Display-time telemetry does not prove circuit fidelity. No DAW, minimum-size or automation interaction certification in this pass.
