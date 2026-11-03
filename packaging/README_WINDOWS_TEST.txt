ArSonEQ M2.2 - WINDOWS x64 ENGINEERING TEST

THIS PACKAGE CONTAINS TWO DIFFERENT BUILDS OF THE SAME ENGINE:

1. VST3/ArSonEQ.vst3
   Copy the entire ArSonEQ.vst3 directory to:
   C:\Program Files\Common Files\VST3\
   Restart/rescan your DAW or ArSonMix. Do not copy only the inner DLL.

2. Standalone/ArSonEQ.exe
   Launch this executable directly on Windows 10/11 x64.
   It is an EQ application, NOT a VST host.
   Select audio input and output from the JUCE Standalone audio device settings.
   Route a real physical input or compatible loopback feed to the app; audio
   from other Windows applications is not intercepted automatically.

Both builds have the same DSP and compact M2.1 WebView2 GUI.
Both require the Microsoft Edge WebView2 Runtime for web UI support.
Set ARSONEQ_USE_NATIVE_GUI=1 before launching the app or DAW if web UI fails.

INSTALLATION: Neither format needs Node/npm or an internet connection.
TESTING: This is NOT a production-certified release.
Check actual WASAPI/ASIO devices, latency, sound quality, GUI scaling,
multiple instances (VST3), automation and save/recall. Report screenshots.
The BUILD.txt manifest identifies the exact source commit used.

LICENSING: GNU AGPLv3 / JUCE 9.0.4; see repository LICENSE.

M2.5 engineering test: floating band card (no graph reflow), Ctrl+click group selection, Shift precision drag, Ctrl+Z/Y undo/redo, right-click context menu. Test 100% and 150% Windows DPI; verify same VST3 and Standalone controls. This is not a production-signed installer.

M3 real preset and dynamic test:
1) Center dropdown -> choose Factory "Vocal Presence": nodes must change; choose "Init / Flat" to reset.
2) Set 1kHz Bell at 0dB static gain, dynamic range -6dB / threshold -40dBFS under MORE; inject 1kHz tone. Output must be reduced vs range=0; set threshold 0dBFS and attenuation should disappear.
3) Open MORE > choose BPF, Tilt Shelf, Flat Tilt: confirm native response curves and measured audio.
4) Set unique bands; choose SAVE destination USER 01, SAVE. Close VST3 and Standalone, reopen and select USER 01: state must return.
5) Confirm no flicker/reflow; group move, preset recall, 100%/150% DPI; profile CPU with 24 active dynamic bands.
Note: factory names/presets are original examples; auto threshold, phase variants, EQ Match and Spectrum Grab are not present.

M3.1 comparison tests:
- Open Native VST3 and Standalone. Click FILTER TYPE and inspect popup: dark background/legible light text (no Windows white menu). Click preset title: dark picker without down-arrow glyph.
- Select dynamic Bell: static Gain 0dB, Dynamic Range -8dB, Threshold -40dBFS, signal 1kHz at -12dBFS. Look for actual movable gold response curve, purple difference shading, separate dot dynamic indicator, and live negative gain value/percentage.
- Silence must release activity; changing band from 1kHz to 5kHz must also retune the detector. With Analyzer OFF, dynamic curve still responds.
- Open MORE: card <=484x142, nodes remain draggable without graph resizing, popup is compact.
- Save a User preset and reload after restarting both EXE and host. Test 100%/150% scaling and CPU at 24 dynamic bands. Log release/audio behavior for issue #4.
