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
