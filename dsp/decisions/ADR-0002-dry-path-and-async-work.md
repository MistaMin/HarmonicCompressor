# ADR-0002: Matched resampler chain for the dry path; owned AsyncUpdater for message-thread work

Date: 2026-11-14. Status: accepted. Evidence: `qa/2026-11-14-processor-fixes/` (`before.log` from
master @ d5b833c, `after.log`, `bench.log`).

## 1. Dry path

**Problem.** The dry signal for parallel Mix was delayed by the oversampler's nominal latency, using a
ring buffer with linear interpolation. That latency is fractional (2x Min 3.137, 4x Min 4.433, 4x Lin 59.5,
8x Min 4.951, 8x Lin 64.25 samples), and linear interpolation is a low-pass. The Min Phase IIR half-band
is also not a delay at all: its phase response cannot be matched by any delay. Measured at 48 kHz with
neutral compression (`before.log`):

- dry alone (Mix 0 %) vs the wet path: -2.0 dB at 10 kHz and -7.9 dB at 18 kHz (4x Min), -8.3 dB at 18 kHz (4x Lin);
- Mix 50 % vs 100 %: -10.7 dB at 18 kHz (2x Min), -9.4 dB (4x Min), phase cancellation between dry and wet;
- only 2x Lin Phase (integer latency, 49 samples) was clean.

**Decision.** Run the dry signal, together with the untouched-input reference used by Delta, through a
second set of the same `juce::dsp::Oversampling` filters (up, then down, nothing in between). Dry and wet
then share the exact magnitude and phase response. The delay ring is removed.

**Result.** Mix 50 %/0 % equal to 100 % within 0.000 dB from 100 Hz to 18 kHz in all seven modes
(`after.log`). Delta on neutral settings nulls to -280 dB in every mode (was -7 to -28 dB).

**Cost** (`bench.log`, 48 kHz, 256-sample blocks, same machine; "after" also includes Auto Gain's
measurement, about 0.004 ms):

| Mode | before ms/block | after ms/block | after % of deadline |
|---|---|---|---|
| 1x | 0.0548 | 0.0591 | 1.11 |
| 2x Min | 0.1140 | 0.1404 | 2.63 |
| 2x Lin | 0.1380 | 0.2023 | 3.79 |
| 4x Min | 0.2224 | 0.2724 | 5.11 |
| 4x Lin | 0.2491 | 0.3572 | 6.70 |
| 8x Min | 0.4457 | 0.5430 | 10.18 |
| 8x Lin | 0.4867 | 0.6734 | 12.63 |

4x Lin Phase now exceeds the 6 % budget of `dsp/reports/STATUS.md` C7 (measured at the default Min Phase,
which stays within it). Available reduction, not taken: filter the Delta reference channels only while
Auto Gain is on (resetting that chain when it stops), about half the added cost when Auto Gain is off.

**Alternatives rejected.** A better fractional delay (Lagrange, Thiran all-pass) improves the magnitude
but still cannot reproduce the Min Phase filter's phase, so Mix would still comb.

## 2. Message-thread work

**Problem.** `parameterChanged()` queued `MessageManager::callAsync ([this] ...)` for latency updates and
mode switches. If the processor was destroyed before the message loop delivered the lambda, it ran on
freed memory. `before.log`: 50 processors destroyed with work queued, then the loop ran: exit code 139
(segmentation fault). `callAsync` also allocates a message on the calling thread, which can be the audio
thread when a host automates Model or Oversample.

**Decision.** The processor is a private `juce::AsyncUpdater`. `parameterChanged()` sets
`latencyUpdatePending` or stores the requested mode and the restore generation it was requested under in
one 64-bit atomic, then calls `triggerAsyncUpdate()` (no allocation). `handleAsyncUpdate()` performs the
work on the message thread. The destructor cancels a pending update.

**Behaviour change.** Several mode changes before delivery coalesce to the last. The end state is the same
as applying each in turn (an intermediate mode would only re-save the snapshot it had just loaded); tested.
The restore race fix of 1.5.1 is kept: a switch requested before a state restore is dropped.
