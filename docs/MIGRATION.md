# MaSonKuPik → ArSonEQ: evidence-based migration audit

Date: 2026-10-29. Source: private `masarray/masonkupik` main, reviewed through connected GitHub; destination: public `masarray/arson-eq`.

| Source file | Evidence / usable logic | Port policy |
|---|---|---|
| `src/audio/presets.ts` | `EqBand`: id, type, frequency, gain, q, slope, enabled. Six filter types; Butterworth Q arrays 12–48 dB/oct. | Re-express model as stable native parameters; numeric Q values may be retained. |
| `src/audio/engine.ts` | `MasterChain` creates Web Audio `BiquadFilterNode` groups; also compressor, color, limiter, width and meters. | **Do not port full engine**. Implement native biquads, reuse validated EQ behavior only. |
| `src/components/plugins/EQEditor.tsx` | Double-click add/remove, drag freq/gain, wheel Q/cut slope, frequency response from `OfflineAudioContext`. | Recreate interactions in native JUCE; replace offline Web Audio response with exact C++ transfer function. |
| `src/components/plugins/_shell/spectrumCanvas.ts` | Log-frequency spectrum display, peak hold, smoothing, ~26 FPS FFT reads. | Port visual principles, not Web Audio `AnalyserNode` or React component. |
| `src/state/app.ts` | Zustand interactions, change grouping and undo operations. | Use APVTS for host automation/state, native undo manager for UI actions. |
| `tools/audio-quality-audit.ts` | Offline response, signal/polarity and format checks on browser engine. | Recreate deterministic C++ tests; host validation is separately required. |

**Audit conclusion:** MaSonKuPik is a browser mastering application with EQ, not an existing C++ VST or a separable low-level C++ EQ library. An indiscriminate code copy makes ArSonEQ heavier and less interoperable. Since source is private and destination public, do not expose its unrelated assets/presets/secrets without reviewing provenance. Owner-requested porting of relevant behavior does not imply third-party asset permissions.

### Verified reference behavior vs extensions

Source currently supports Bell, Low Shelf, High Shelf, Low Cut, High Cut, Notch; 12/24/36/48 cut slopes; live spectrum; curve response; draggable bands. It does not establish VST3 host compliance, phase modes, per-band dynamic EQ or sidechain: these are **new work**, not port-complete features.

### Provenance and ownership

MaSonKuPik source repository had no project-level LICENSE file visible in the audited tree. Destination already has AGPL-3.0. Before copying any substantial source expression into this public repo, document ownership, dependencies and permissions. The initial C++ implementation is a fresh portable engine, with openly documented standard RBJ filter equations.

Reference links:
- https://github.com/masarray/masonkupik
- https://www.w3.org/TR/webaudio/
- https://www.fabfilter.com/news/1543478400/fabfilter-releases-fabfilter-pro-q-3-equalizer-plug-in-with-dynamic-eq
- https://github.com/juce-framework/JUCE
- https://www.steinberg.net/developers/vstsdk/
