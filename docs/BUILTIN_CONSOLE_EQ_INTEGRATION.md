# ArSonMix built-in channel EQ — isolated DSP candidate

**Branch scope (2026-11-06):** This is an additive, native-only console profile in the existing **ArSonEQ** source project. It does **not** change ArSonMix `main`, the approved `/preview/` GUI, mixer routing, monitoring, channel graph, VST3 factory packages, or audio callbacks. The existing 24-band ArSonEQ VST3/Standalone remain unchanged.

## Why reuse the current engine

The ArSonEQ PR #1 DSP already supplies RT-owned IIR biquads, Butterworth cut filters (12/24/36/48), gain/Q support, and fade/smoothing logic. The compact **ConsoleChannelEQ** wrapper uses that identical `Engine`, not a second EQ implementation, vendor library, WebView, FFT, or VST host. Source code lives in `Source/DSP/ConsoleChannelEQ.h` and is AGPL-3.0-only like the existing project.

## Operator contract (9 stable slots)

| ID | Type | Flat/default center | Controls |
| --- | --- | --- | --- |
| 0 | HPF / LowCut | 80 Hz | Enable, Hz, 12/24/48 dB/oct |
| 1 | Low shelf | 160 Hz | Enable, Hz, gain, Q |
| 2–6 | Bell 1–5 | 250 / 500 / 1200 / 3200 / 7500 Hz | Enable, Hz, gain, Q |
| 7 | High shelf | 10 kHz | Enable, Hz, gain, Q |
| 8 | LPF / HighCut | 18 kHz | Enable, Hz, 12/24/48 dB/oct |

All bands **disabled** initially (flat, transparent). Frequency 20–min(20k, 0.45×sample rate); shelf/bell gain ±18 dB; Q 0.1–24. Cuts ignore gain and use Butterworth Q tables. Dynamics, oversampling and analyzer are OFF in this profile. No new host parameters, plugin identity or saved-show schema introduced at this stage.

## Realtime and ownership

- One instance per stable accepted `ChannelId` (mono or stereo). Each instance owns its filter history; do not share across channels or reconnect epochs.
- The same native audio callback and rendering graph must own `processMono` / `processStereo`. Processing is **in-place** with no new audio callback, block buffer, requeue, dynamic allocation, FFT, locks, or background sample transfer. A never-enabled EQ is a direct pass-through.
- Construct, prepare, and publish on the control side. Apply validated controls **only on the audio owner at a safe block boundary** (or prepare an immutable replacement off-thread). `setBand` is not thread-safe across UI/control/audio threads. Do not call `bandResponseDb` from the UI concurrently with processing.
- Slot enable/disable and topology changes reuse ArSonEQ's existing approximately 10ms crossfade. Numeric control changes reuse its smoothing. This is **zero additional sample buffering**, not a promise of zero analog/ASIO round-trip delay, zero IIR group delay, or click-free changes under all conditions.
- The audio graph must determine actual insert order (normally input trim → EQ → FX rack → fader → sends/MAIN) and keep gain staging safe; **this branch intentionally does not change that graph**.
- Controls must reflect accepted native state after `ACK + snapshot`, never browser simulation. Use existing epoch/revision checks and fail closed for unsupported ChannelIds. Do not make UI selector interactions start/arm a device or route new physical outputs.
- Rack master bypass remains independent from channel EQ. Do not override per-insert saved enable state.

## Why this is not yet a product claim

The wrapper and tests are a separately reviewable dependency candidate, **not an active ArSonMix EQ**. There is no new on-screen EQ button, live mixer PCM integration, installer change or platform roundtrip benchmark in this PR. Once the current mixer single-owner integration has stabilized, a separate planned native/GUI vertical slice can pin the audited ArSonEQ DSP source, expose a built-in channel EQ UI on **the existing approved** `/preview/` skin, wire accepted ChannelId parameters and readbacks, and run exact Windows builds + real ASIO hardware FAT.

## Test contract

The independent CMake test verifies the 9-slot schema, default transparency, fail-closed parameter validation, bell/cut frequency response, sample-0 impulse delivery, stereo independence, mono alias safety, bypass tail and no process-path heap allocations at 32/64/128/512 frames. Existing DSP/RT/analyzer tests must keep passing. Physical CPU load, plugin host VST3 footprint, dropouts and hardware timing remain **PENDING** and must not be inferred from CTest.
