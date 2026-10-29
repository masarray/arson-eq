# ARD — ArSonEQ Architecture & Design Rules

Version 0.1 · 2026-10-29 · Status: approved direction, implementation evolving

## Design decisions

**A1 — Native engine, not Web Audio:** Reimplement an independent C++17 DSP core with deterministic biquad coefficients. MaSonKuPik is a functional/UX reference, not a source tree to copy indiscriminately.

**A2 — JUCE 9.0.4 for VST3 and native editor:** Use JUCE only for plug-in host integration, parameters, state and rendering. DSP owns no JUCE types. Pin external dependency version. The repo and the JUCE AGPL distribution path are compatible; commercial distribution requires a separate licence decision.

**A3 — 24 fixed band slots, no dynamic audio allocations:** Each slot has type, frequency, gain, Q, cut slope, channel mode and enabled state. Keep parameter IDs stable even after a slot is removed in UI. Fixed max 4 stages for P0 cut filters.

**A4 — Hard isolation of audio and UI:** Audio callback does no malloc/free, locks, network IO, console IO, GPU interaction, or file access. Parameter snapshots are read as atomics, and coefficients update only on changed values. UI can cache graph responses and throttle analyzer repainting independently.

**A5 — Stereo M/S routing:** For each band transform L/R to M=(L+R)/2, S=(L-R)/2 only if that band's placement needs M/S; apply independent filter state to the selected route; reconstruct L=M+S, R=M-S. Never apply per-band M/S as a global widening effect. Mono input uses mono processing and disables meaningless Left/Right/Side routes as necessary.

**A6 — Separate transfer functions from UI graphs:** Expose analytical magnitude response from the exact DSP coefficient generator. UI graph must use actual sample rate and its current processing mode, not a fabricated 48kHz Web Audio offline context.

**A7 — Explicit progression of phase modes:** P0 direct causal minimum-phase biquad, zero algorithmic delay. P2 separate measured natural-phase alternative and latency-accounted linear-phase FIR. Do not claim a direct analog-copy of FabFilter algorithms.

## Architecture

```text
DAW host
  ├── VST3 wrapper (JUCE): buses, latency, automation, state, host lifetime
  ├── ParameterAdapter: stable IDs, normalized ranges, bounded snapshots
  ├── AudioProcessor (real-time): input → 24 fixed-band EQ → output
  │      ├── biquad coefficient generator (RBJ topology + Butterworth stages)
  │      ├── per-band L/R/M/S state, sanitized numerical limits
  │      └── smoothing / topology transition policy
  └── Editor (message thread): graph, nodes, inspector, cached response
         └── optional RT-safe metering/FFT telemetry queue (future)
```

Portable DSP must be independently tested with a simple C++ test harness. It must not include Web Audio, JUCE, UI, JSON, or licensing code.

## Signal flow / control

1. Host writes normalized parameter values into APVTS atomics. On each block, processor reads a coherent-enough bounded snapshot per slot; each changed slot updates its target coefficients. A later revision may use immutable whole-bank snapshots for simultaneous multi-parameter changes.
2. Each sample flows through enabled slots in stable numerical order, with independent state for L/R or M/S. All stereo transforms are per slot; no summing of nonidentical latency paths.
3. Gain in dB converts to amplitude; cut filters are Butterworth cascades using the MaSonKuPik Q-stage array: 12 [0.70710678], 24 [0.5411961,1.30656296], 36 [0.51763809,0.70710678,1.93185165], 48 [0.50979558,0.60134489,0.89997622,2.56291545].
4. For bell/shelf/notch calculate normalized biquad from RBJ Audio EQ Cookbook. Frequency/Q are bounded; every intermediate is finite; target coefficients computed at configuration changes, **never per sample**.
5. Retune uses interpolation or a fully verified dual-pipeline crossfade to avoid zippering and instability. Shape/slope change needs separate discontinuity tests. A simple block-step rebuild is **not** a release-quality solution.
6. Bypass is unity and preserves host state; transitions require crossfade/ramps, and full bypass must neither alter latency nor create clicks.
7. On sample-rate changes, update all coefficients and reset/transition filter state predictably; on silence avoid denormal stalls.

## Parameter contract

For slot N, IDs are immutable `bNN_enabled`, `bNN_type`, `bNN_freq`, `bNN_gain`, `bNN_q`, `bNN_slope`, `bNN_route`. Type order: Bell, LowShelf, HighShelf, LowCut, HighCut, Notch. Route: Stereo, Left, Right, Mid, Side. Slope: 12,24,36,48. Safe defaults: disabled, bell, 1000 Hz, 0 dB, Q=1, slope=12, Stereo. Numbering and enum order are persistence APIs; do not reorder.

Band ranges: 20 Hz–20 kHz (clamp to 45% SR), -24 to +24 dB, Q 0.1–24; visual options may expose more restricted zoom. Actual cutoff and response are sample-rate limited.

Schema migration: a versioned host state tree will preserve current slot identity when changing GUI, FFT or dynamic features. Any later incompatible addition must be additive or migratable, never rename existing host parameter IDs.

## Lifecycle and threading

- Construction: allocate fixed states once. Prepare: set sample rate, initialize states. Process: no allocation and no throwing.
- UI: repaints only on observed change; optional analyzer target 20–30 FPS. No unbounded `repaint()` loop or heavyweight effects; when editor closes, analysis stops.
- For P1 FFT, use preallocated SPSC sample/tap queue and worker/UI drain; drop visual frames rather than block the audio callback. Ensure lock-free queue semantics and ownership with tests.
- Offline render and host-initiated parameter automation must use the same DSP equations; no dependence on frame rate or wall clock.

## Precision and verification

For reference response test: compare analytical `H(e^{jw})` with measured sine sweep at 44.1/48/96/192 kHz and check normalized Biquad stability; verify 24 stages × two channels with random input finite. Test M/S symmetry, mono, disabled-band unity, impulse tail, repeatability, gain at band center, coefficient edge cases, and rapid-automation stress. Track max absolute error by mode.

Before shipping: run Steinberg validator, JUCE pluginval, REAPER, and a second DAW; run Windows Defender scan on artifacts. Include licenses in binary distribution.

## Design risk register

| Risk | Mitigation |
|---|---|
| Browser DSP copy is not VST-ready | Port feature contract and verified math only |
| Automation zipper/crackle | Per-sample coefficient smoothing and topology-specific crossfade QA |
| GUI hover/FFT slowing real-time audio | Cached CPU draw, no GPU dependency for sound, throttled analyzer |
| JUCE AGPL/commercial licensing | Publish source under AGPL, review commercial edition separately |
| Nonzero-phase behavior misrepresented | Name only implemented modes, measure before enabling |
| Parallel thread drift | PRD/ARD and CHANGELOG are canonical; small PRs with tests, no speculative rewrites |

## Stage gates

M0: repo structure, standalone DSP, unit tests, CMake; M1: VST3 adapter, host params, graph interaction and save state; M2: audited transitions, spectrum analyzer, host validation; M3: dynamic EQ; M4: phase variants/surround/other formats. Consult [ROADMAP.md](ROADMAP.md) for binary acceptance. Avoid costly CI on unneeded toolchains until the host wrapper exists.


## M1.5 / early M2 engineering reconciliation (2026-10-29)

After auditing the initial native implementation, naive coefficient interpolation was replaced for **discrete topology changes**: enabled/bypass, filter shape, route and number of stages now use two fixed preallocated processing paths, a 10ms smoothstep crossfade and bounded last-request-wins pending transitions. This avoids changing the same path's topology/state instantly or restarting an unfinished transition. Topology crossfades cost extra CPU **only while a change is in progress**, and never introduce latency. Frequency/Q/gain updates retain stable-coefficient interpolation over 12ms on the active path. All work is performed with fixed storage and no heap operations on the callback.

**Limitations:** exact pop/click-free behavior still requires listening and more stringent amplitude-difference stress tests with external hosts; high-rate topology automation can defer the most recent request by the remaining crossfade duration. For 24 automated bands all changing simultaneously, two-path execution increases CPU temporarily. This must be benchmarked at small buffers; it is not yet a measured CPU guarantee.

The analyzer uses a fixed 16,384-frame SPSC ring of {pre-mono, post-mono} samples, with release/acquire indexed ownership. The audio thread only pushes when an editor is open; on overflow telemetry frames are dropped rather than delaying audio. The editor consumes on a 30Hz timer, computes Hann-windowed 4,096-point pre/post FFTs on the message thread, and draws both behind the EQ curve. FFT size limits low-frequency resolution (~11.7Hz per bin at 48kHz); future analyzer iterations can use lower-frequency multiresolution windows if CPU budget permits. The audio path does **not** invoke the FFT.

The graph caches a 2x2 complex transfer matrix for the full cascade, including L/R/M/S and crossfeed. UI plots diagonal L<-L and R<-R responses and a subdued crossfeed line, recomputing only when EQ parameters, sample rate or editor geometry changes. This is a more accurate representation than the initial stereo-only approximate graph. It does not claim to equal a measured music-spectrum FFT or FabFilter's proprietary phase algorithms.

The fixed APVTS parameter IDs remain unchanged. The project now tests single-thread finite/RT stress, measured sine gain against the analytic curve, switch continuity, no heap allocations during process, ring buffer overflow and concurrent producer-consumer integrity.

CI efficiency: one small Linux headless job and one Windows VST3 job that also runs the headless tests; avoid a duplicate Windows headless matrix. CI begins only after a coherent multi-file milestone commit, not after each individual edit.


## M2 GUI Architecture — native DSP + bundled JUCE WebView2 shell

**Decision**: User explicitly requested porting MaSonKuPik EQ presentation, with an optimized web GUI. On Windows the editor uses JUCE 9.0.4 WebBrowserComponent with the WebView2 backend. Assets are three embedded local-only files: `WebUI/index.html`, `style.css`, `app.js`. There are **no npm runtime dependencies**, React, Tailwind, live web requests, localhost server, remote fonts, Web Audio DSP, or online CDNs. Windows native JUCE editor remains as a fallback if WebView2 options are unsupported; the DSP and VST3 parameters are unchanged.

**Bridge and ownership**:
- C++ reads the authoritative 168 APVTS host parameters (24 slots x 7) on the editor message thread at 25Hz. Browser can only submit allow-listed `enabled/type/route/freq/gain/q/slope` commands via one `eq-command` listener. Numeric values and indices are bounded. Gestures (`begin` / `edit` / `end`) use `beginChangeGesture`, `setValueNotifyingHost`, `endChangeGesture` to preserve host automation.
- UI response curves are computed from the **same native DSP transfer matrix**, including stereo L/R and M/S; transmitted only on changed bands, sample rate, or selected band (256 log-spaced points). No offline Web Audio simulation or JS filter coefficients.
- Pre/post audio telemetry enters existing fixed-size SPSC queue; the message thread performs FFT and sends 224 log bins via a bounded 25Hz event. The WebView does **not** call native audio processing, use Web Audio, or access the audio callback.
- Canvas 2D retains MaSonKuPik interaction logic (double-click to add/remove, pointer drag, wheel Q/slope, notes and inspector), but uses a new self-contained DOM/Canvas implementation to avoid shipping its entire mastering app and dependency graph. Rendering is event-driven: only repaint after changed state or spectrum and maintain a 2x maximum device pixel ratio.
- Embedded resource provider only serves exact static resource paths; page navigation is limited to local resource root; CSP denies all external origins. Avoid security expansion by never loading third-party pages in a native-integrated WebView.
- WebView2 is a Chromium runtime and may use more baseline memory than JUCE's native editor. A tiny JS bundle is not the same as a zero-cost WebView2 process. Measure resident memory, first-open time, CPU and multi-instance behavior on Windows before calling the UI "ultra-lightweight".

**CI**: Javascript syntax + no-dependency mock-DOM smoke on Linux; Windows build checks pinned WebView2 SDK headers/loader and embeds the assets into the VST3 artifact. Use a cached pinned NuGet SDK and retain native DSP test suites. Real DAW/WebView2 runtime inspection remains a gate.

**Compatibility**: Never change existing host parameter IDs, plug-in ID, manufacturer ID, plugin binary format, state schema or audio process methods just to support GUI redesign. Native editor must remain available as a recovery path. If runtime initialization fails after CMake-supported capability probe, report failure and improve fallback rather than claiming success.


## M2.1 Visual hierarchy correction — reference images and ArSonMix design system (2026-10-29)
User supplied side-by-side screenshots: Pro-Q 3 (902×572), ArSonMix mixer (1652×962), old ArSonEQ (1112×712). Audit shows wasted right inspector column, 1110×680 editor default, chart truncated by minimum CSS geometry, redundant header/footer text, and overlarge always-on panels.

**Canonical UX**: The graph is the main work area (full width); only a narrow 43px top chrome and 29px bottom status bar remain. Selected bands summon a 466px floating inspector adjacent to the selected node; the box prefers below if space permits, otherwise above, clamped inside graph bounds. No inspector exists when selection is empty. All 24 active bands remain available through the graph or optional BANDS popover. A fixed 900×535 JUCE initial editor size fits ordinary floating host windows. User may resize from 760×455 to 1360×850, and the floating inspector stays within graph bounds.

**Visual tokens**: Derive from ArSonMix's documented `design-references/brushed-metal-v1.0/reference.json`: metallic/aluminum controls, blue-black glass in the FX workspace, turquoise status and contextual channel colors. Retain high contrast, compact controls with 1px beveled borders and no high-cost hover filters, blur or animation. Use Pro-Q's high-level placement concept only, not FabFilter artwork/trademarks/assets.

**Rendering**: Always draw full graph, subtle log-frequency grid, warm 0dB baseline, restrained 2px curve, thin R line only when different from L, per-band colored draggable nodes and optional FFT layer. No React runtime and no constant idle redraw. Canvas changes remain event/telemetry driven and are capped at device DPR=2. No changes to the audio callback, native DSP or stable host parameters. WebView2 is still a browser process; memory/idle-paint tests remain required.

**Acceptance**: JavaScript smoke checks selected inspector visibility, floating position, no sidebar, menus and native gestures; Windows JUCE/WebView2 VST3 release build required; actual DAW screenshot/re-open/multi-instance test and CPU/RAM profiling remain release gates.


### M2.1 finishing integration — rotary automation and spectrum cost gate
The floating inspector includes three tactile rotary controls for frequency, gain and Q; each supports vertical pointer drag, mapped logarithmically for Hz/Q and linearly for dB. Each pointer begins one APVTS gesture, sends bounded parameter edits during movement, and ends the gesture on pointer release/cancel/lost capture. The native bridge's fixed gesture array grows from 2 to 3 fields. Numeric inputs remain for precise editing. This only extends the *GUI bridge*; the engine, 168 parameter IDs, and persisted state schema are unchanged.

The ANALYZER switch sends a native `spectrum` command. When off, `AnalyzerQueue::setEnabled(false)` atomically shuts off audio telemetry production, and the message-thread FFT/send loop is skipped; when on, telemetry resumes. Closing the editor still disables the queue. This prevents unnecessary audio-to-UI data and expensive FFT work when the user wants a still EQ curve. This is a source-level optimization and must be validated with actual Windows WebView2 CPU measurements.

Smoke coverage checks knob Q gesture, native UI event paths, spectrum ON/OFF command, full-width graph, no permanent sidebar, popup placement, and popup/menu lifecycle. CI workflow paths include UI assets (build) and all WebUI test changes (headless), without rebuilding JUCE for a test-file-only edit.


## M2.2 — Dual-target native Windows delivery

The exact same `juce_add_plugin(ArSonEQ ... FORMATS VST3 Standalone)` shared source set produces (1) a real VST3 plugin bundle for third-party hosts, and (2) a true native Windows `ArSonEQ.exe` using JUCE's official Standalone plugin wrapper and audio-device lifecycle. This avoids creating a second competing processor/GUI code path, avoids a wrapper webserver or Electron process, and prevents hardcoding Windows device APIs. Windows audio I/O belongs to JUCE Standalone: device selection and permissions follow the runtime/OS. Browser assets are binary resources embedded into each target; no localhost server or external assets.

The CI compiles both targets under ONE configured JUCE build graph, runs the same three native DSP suites, verifies nontrivial EXE and VST3 module outputs, and uploads **one combined test bundle** with a commit SHA manifest. Only native DSP C++ source and plugin client wrapper differ at link time. Test audio-device initialization, system-exclusive modes, auto/manual input selection, actual ASIO/WASAPI drivers, and persistence on Windows real hardware before advertising the Standalone app as production ready. The VST3 and Standalone executables should not be mixed or installed into each other's folders.


## M2.3 GUI audit: VSTHost screenshot differential and non-occluding editing architecture

**User-supplied evidence:** A VSTHost screenshot shows an active ArSonEQ band at 78.4Hz/+12.25dB and a 466px editing card obscuring much of the actual response and lower plot. A comparison VSTHost screenshot shows Pro-Q 3 displaying its dominant frequency response in an uncluttered upper graph with small, clearly grouped musical controls near the bottom. Our previous M2.1 approach (an absolutely positioned inspector based on selected node X/Y) fundamentally allowed graph occlusion; CSS repositioning alone was not a credible solution.

**Fix architecture:** The `inspector` node is a *sibling following* `graph-wrap` inside a vertical-flex `workspace`. When selected, the bottom context deck has exactly 143px of dedicated layout height plus a 7px edge gap; when selection is absent the deck disappears and plot expands. **No band control exists inside graph-wrap and no band controls are positioned absolutely over the plot.** This makes graph occlusion mathematically impossible in the DOM structure, including at extrema of frequency, gain, host resize and all 24 slots. No animation/reflow loops, no hovering tooltip stealing pointer events.

**Visual pass:** Carbon/charcoal graph rather than navy dashboard; premium dark ArSonMix workspace with restrained metallic control surfaces; response line in warm gold; translucent green area for positive boost and subtle warm attenuation fill; per-band colored handles; selected-band isolated response; independent right-side FFT dB rail, thin frequency grid, more restrained typography. Original gold/green scheme is a design pattern, not use of FabFilter's proprietary art. Default 12dB display range automatically expands to 15/18/21/24dB in response to actual parameter magnitudes; graph displays the authoritative C++ calculated transfer matrix rather than a synthetic JavaScript frequency response.

**Performance:** Right rail costs 40 CSS pixels, leaving a dedicated graph viewport width for logarithmic X mapping. Canvas render is event-/FFT-telemetry driven; no permanent animation loop, React, network requests, blur effects or global hover filters. FFT still runs only in editor thread and is fully gated OFF when disabled. The inspector is compact and uses existing host APVTS gesture semantics and parameter IDs. Both Windows Standalone and VST3 compile from exactly the same HTML/CSS/JS embedded bytes.

**QA / evidence:** Headless browser synthetic host fixture (CI only) provides deterministic initial bands and a synthetic Gaussian-like visual response to expose layout issues in a 900×535 screenshot. This *visual mock response is never compiled or embedded into the plugin*: only `index.html`, `style.css` and `app.js` are embedded. The fixture does not validate native DSP accuracy, Windows WebView2, ArSonMix/VSTHost focus handling, or real audio latency. The Linux quick job renders a screenshot with existing Chromium and publishes it as a separate tiny artifact; no extra JUCE build. Automated JS smoke asserts dock sibling DOM, bounded layout structure, native host gestures, analyzer off gating, 12dB+ high-gain state, and popover behavior.

**Release gates:** Inspect headless pixel rendering for any clipping/collisions and run both Windows binaries in actual VSTHost and ArSonMix at 100%/150% DPI. Profile multiple instances and input device support separately. FabFilter Pro-Q 3 feature/visual parity is an aspiration, not currently certified or claimed.


### M2.3.1 visual QA findings — axis collision and pointer-frame invariant

The 900x535 Chromium screenshot was reviewed, not merely generated. The geometry correctly shows an isolated bottom edit deck and four unobstructed EQ nodes, but the top-right +12/+9 dB labels collided because both were clamped to y >= 28px. Fix axis text placement to `clamp(y+3,13,h-17)`, which preserves separation at both positive and negative range extremes; move the 20kHz frequency label inward to prevent collision with the rightmost -12dB label. Enlarge static metallic knobs from 30px to 34px with a correspondingly bounded 148px dock.

**Interaction invariant:** The graph height changes when opening/closing the dock. Previously `pointerdown` synchronously selected a node and opened the dock before a drag, remapping the Canvas coordinate system between pointerdown and pointermove. Now a first pointerdown captures the node and begins a host gesture **without selecting/opening the dock**. The dock appears only on pointer release after ending the host gesture, so the first drag uses one stable graph coordinate system throughout. A recent first-hit cache is used narrowly for double-click deletion when the second click lands a few pixels away after layout reflow. The fake-host test validates that the dock stays closed until release, then opens with correct host gesture lifecycle. Editing an already-selected node retains a fixed dock height. This is especially important for VSTHost and other small floating editors.

These are GUI-only fixes; C++ DSP, latency, APVTS IDs, standalone/VST3 sources remain unchanged.


## M2.4 architecture decision — Pro-Q 3 gap audit, 2026-10-30

**Authoritative baseline:** `docs/PROQ3_GAP_AUDIT_AND_M24_PLAN_20261030.md` compares user VSTHost/Standalone screenshots and the original Pro-Q 3 50-page manual against the real native C++/JUCE source. It supersedes earlier speculative UI descriptions; do not start any more style-only changes without addressing analyzer correctness.

**P0 failure in source:** the current processor computes `(L+R)*0.5` before queuing a spectrum PCM stream; anti-phase L/R cancels to zero even with audible energy. The current 4096 FFT maps 224 logarithmic UI frequencies to their nearest FFT bin; duplicate bins share temporal smoothing state and get updated multiple times per frame. These are the concrete engineering causes of weak/stair-stepped analyzer behavior. A WebUI CSS gradient is not a solution.

**Approved direction:** preserve 100% native C++ DSP, a single lock-free PCM producer, and UI/editor FFT. Extend the SPSC payload to preserve L/R before and after EQ and compute power spectra independently. Merge in the power domain. Calibrate window/magnitude, implement per-display-frequency smoothing once per frame, and apply 4.5dB/oct visualization-only tilt by default. Offer 1024–8192 FFT, speed, range, Pre/Post/Freeze and sample-peak L/R meter with actual audio telemetry. No audio-thread FFT, lock, alloc or WebView.

**UI density correction:** at 900×535, default selected-band deck <=70px so plot remains >=390px tall. Advanced controls are user-expanded, not permanently visible. The current ~148px deck cannot reach Pro-Q 3 graph dominance even after styling. The selected band and the gain curve must remain unobscured. Use ArSonMix visual tokens with original assets and shared CSS; do not copy FabFilter's proprietary art.

**Compatibility:** 24 slots and the existing 168 APVTS parameter IDs remain unchanged through M2.4. Analyzer settings use versioned non-host-parameter settings until their automation requirements are decided; no retroactive parameter-ID remapping. Keep VST3 and Standalone generated in one Windows Release job. Proof gates cover 44.1/48/96k, 32/64/128 buffers, 50Hz–10k calibrated response, anti-phase stereo source, DPI 100/150%, VSTHost, ArSonMix and multi-instance resource use. Manual references: https://www.fabfilter.com/downloads/pdf/help/ffproq3-manual.pdf printed pp. 9–11 and 25–26.


### M2.4 implementation (2026-10-30, candidate before host QA)
The SPSC `MeterFrame` is upgraded from premixed mono input/output to four channel snapshots `preL,preR,postL,postR`; mono uses dual-mono. Sample energy is combined **after** native FFT in the power domain, preventing anti-phase cancellation in Mid/Side material. `Source/DSP/SpectrumDisplay.h` defines testable pure C++ log-frequency interpolation, power/dB normalization, tilt and time-constant-based smoothing; this never touches the audible DSP coefficients.

The editor owns four preallocated 8192-sample circular time-domain buffers, a reusable JUCE FFT scratch buffer, and four magnitude tables. GUI-side FFT dispatch is selectable 1024/2048/4096/8192, using a Hann window and coherent-amplitude normalization (sum(Hann)/2), followed by power-domain stereo combination into 384 log-spaced visual frequencies. EMA attack/release uses elapsed UI time, not a fixed smoothing-per-bin mutation. The FFT only executes on the JUCE message thread while GUI and analyzer are enabled. Resolution, speed, tilt (0/3/4.5/6 dB per octave), 60/90/120 dB display range, Pre/Post and Freeze are editor settings, **not new VST3 host parameters**. The right-side L/R meter shows the highest observed output samples from editor telemetry, with display release; not true peak and not guaranteed sample-complete if the SPSC telemetry ring overflows.

Default active-band dock is 70px with explicit MORE expansion to 151px for knob/route/slope/note detail; graphical controls are always outside `graph-wrap`. The graph gets approximately 398px height at 900×535, exceeding the >=390px design contract. Renderer is original Canvas2D, no React/Web Audio, no GPU blur or permanent animation loop. There are still only the original 168 host band params, all preserved.

**Validation discipline:** native analyzer test asserts fully anti-phase L/R power, -6dB power conversion, 4.5dB/oct tilt and concurrent FIFO. UI smoke tests FFT option event, freeze and 70px dock. These are unit/smoke checks, **not** a complete Hann-window signal calibration / VSTHost cross-DAW / pluginval certification. Actual sine sweep and stereo music comparison remains a release gate.


**M2.4 native fallback QA / FFT optimization:** Windows first compilation identified native JUCE fallback's old `MeterFrame::before/after` field names. Its analyzer now captures independent L/R histories and combines their **FFT powers**, so anti-phase and right-only signals are visible even when WebView2 is unavailable. Four-channel WebView analyzer computes Hann coefficients and window coherent gain once at editor construction or FFT resolution changes, not with expensive trig for each FFT/channel at 25 Hz. Both changes preserve real-time safe core audio. The first CI failure is an integration regression, not evidence of validated Windows audio; recheck on a fresh dual-format CI run.


## M2.5 — professional direct manipulation and immutable graph viewport (2026-10-30)

The user-reported screenshot regression in M2.4 is a real layout bug: the 70px inspector is a flex sibling of the plot, and selection, deselection or MORE expansion changes the canvas height. This shifts log-frequency and dB coordinates mid gesture. M2.5 moves `#inspector` **inside** `#graphWrap` and composites it with absolute positioning. It never consumes flex height. An algorithm computes an initial card position from the selected handle, preferring below the plot line; if the handle is too near the bottom, it places the card above the handle. Card position is updated on selection, resize, and MORE toggle, **not on every drag frame**. Canvas width/height and FFT/meter geometry are unchanged by selection. Right-click context menu uses the same graph coordinate system.

Native + WebView command contract:
- Existing single-band `begin`/`edit`/`end` unchanged.
- M2.5 adds `gestureMany {begin:boolean, items:[{id,fields:["freq","gain"]}]}` with max 24 entries, and `bulkEdit {entries:[{id,field,value}],gesture:boolean}` with max 168 entries; the native bridge validates each id and field through existing editor methods. One WebView IPC frame can update all 24 bands. No audio-thread work.
- Ctrl/Meta click toggles band membership; Ctrl/Meta+A selects all enabled; selected set remains independent of native `selected_` primary band. Group drag applies **frequency ratio** from shared log-x displacement and **gain dB offset** to each eligible bell/shelf; cuts and notches preserve gain. Host gestures are opened for each band and closed on pointer release/cancel, including on destructor failure cleanup.
- Shift drag scales pointer movement to ~17% of full-range for precision; frequency clipping and gain clipping remain within 20–20000 Hz / ±24dB, no parameter-ID migration.
- Undo/Redo uses bounded 64-entry per-editor snapshots of all 24×7 band controls. The UI records one edit transaction per drag or knob gesture and sends one bounded `bulkEdit` transaction when undoing/redoing. Ctrl+Z/Y and toolbar buttons target **UI-local** history; external DAW undo/redo, automated host control, session-persisted undo stacks remain out-of-scope. The actual VST3 parameters are authoritative and changed with `setValueNotifyingHost` via JUCE. History is not a new persisted state parameter.
- Context menu: right-click handle for edit/bypass/delete, right-click empty plot for add band, select all from menu. All actions are connected to actual band parameters, not inactive mock controls.

Validation: JavaScript fake-host smoke now verifies floating nesting and compositor CSS contract, drag gestures, grouped `bulkEdit`, undo/redo, right-click and analyzer actions; Chromium visual snapshot of selected band on a fixed canvas. Real VSTHost/ArSonMix 100%/150% DPI, host automation and drag-click feedback remain mandatory before merge. Keep PR draft.

**Limits:** Group selection currently supports aggregate drag/delete/bypass, not true Pro-Q 3 per-band solo, range editing, spectrum grab or fully integrated DAW undo. Do not claim those implemented. Advanced card may cover background spectrum but selected band nodes are protected by initial placement; it is not a reflowing bottom dock.
