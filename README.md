# Diamond Room

A JUCE/C++ VST3 rebuild of the `DiamondRoom.fst` FL Studio Patcher preset: four
parallel reverbs fed from a saturation stage, summed through a valve
compressor, on a single 4U rack panel cut from crystal.

Everything is native DSP - the plugin has no dependency on Waves, Valhalla or
FL Studio, and does not host or require the original plugins.

## Signal flow

Rebuilt from the Patcher routing:

```
in ─ Driver ─┬───────────────────────────────────── dry ──┐
             │                                            │
             ├─ H-Reverb ──── H-Mix ────┐                  │
             ├─ MannyM Reverb ─ Mix ────┤                  │
             ├─ Valhalla Verb ─ Mix ────┼─ Sum ─┬──────────┤
             └─ True Verb ───── Mix ────┘       │          │
                                                └─ Tube ───┤
                                                           │
                                                  Mix (dry/wet) ─ out
```

The Patcher fed only the reverbs from the Driver, leaving the dial inaudible at
anything but a very wet Mix. Here it sits ahead of the split, so Drive colours
the whole signal.

`Tube` is the valve stage from CleanVoice, ported unchanged, with the dial as
its Mix: at 0 the sum passes through untouched, at 10 the stage is fully in
circuit. In CleanVoice it is a switch; here it can be blended.

## Controls

| Control | Range | What it does |
| --- | --- | --- |
| Drive | 0–10 | Aggressive valve overdrive over the whole signal: even and odd harmonics, progressively darker, level held constant, 4x oversampled |
| H-Reverb Tone | ±10 | Tilt around 900 Hz on the H-Reverb tail |
| H-Reverb Time | 0–10 | RT60, 0.25 s to 10 s (preset value 3.24 s ≈ 6.9) |
| MannyM Distortion | 0–10 | Saturation on the chamber output |
| MannyM Amount | 0–10 | Chamber density and level |
| Valhalla HighCut | 0–10 | 1 kHz to 20 kHz (preset value 16.4 kHz ≈ 9.3) |
| Valhalla Decay | 0–10 | 0.3 s to 9 s (preset value 2.23 s ≈ 5.9) |
| True Verb Distance | 0–10 | 0.5 m to 30 m; sets early/tail balance and air absorption |
| True Verb Roomsize | 0–10 | 200 m³ to 30000 m³; sets reflection spacing, tank length and decay (0.5 s to 1.9 s) |
| Tube | 0–10 | Mix of the CleanVoice valve compressor on the reverb sum |
| Section LEDs | on/off | Arm each reverb |
| H-Mix … TrueVerb Mix | 0–100 % | Level of each reverb into the sum |
| Mix | 0–100 % | Master dry/wet |

Along the top rail: undo and redo, preset stepping either side of the preset
name, and a settings gear. Clicking the preset name opens the full list, plus
**Save as...**, **Delete** for user presets, and **Show preset folder**.

## Presets

Eight factory presets ship with the plugin, from **Subtle Air** through the
**Diamond Room** default to **Crushed Verb**. User presets are written to
`%APPDATA%/DiamondRoom/Presets` as `.drpreset` XML and appear under **User** in
the same menu. A preset whose controls have since been moved is shown with a
trailing asterisk.

The window size is remembered in the plugin state, so a session reopens at the
size it was closed at. The gear menu offers five sizes, and the window can also
be dragged from its corner; the aspect ratio is fixed.

Undo and redo cover parameter moves, reverb on/off switches and preset loads.
One gesture is one step: dragging a fader from end to end is a single undo, not
a hundred.

Factory defaults are taken from the presets saved in the `.fst`: H-Reverb "Focused Lead
Vocal", MannyM "Dave Aron - Rock Vocal Hall 1", Valhalla "VaViRb_Vox_9" (Concert
Hall / 1970s), TrueVerb "Vocal spread".

## Building

Needs CMake 3.22+ and a C++17 compiler. JUCE 8.0.4 is expected in `JUCE/`:

```bash
git clone --depth 1 --branch 8.0.4 https://github.com/juce-framework/JUCE.git JUCE
```

Then:

```bash
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

VST3 and a standalone app are produced under
`build/DiamondRoom_artefacts/Release/`. The VST3 is copied to the system plugin
folder automatically (`COPY_PLUGIN_AFTER_BUILD`).

If a host still has the plugin loaded, that copy fails with a long MSB3073 error
even though the build itself succeeded - Windows will not overwrite a DLL that is
mapped into a running process. Close the host and build again.

## Development helper

`Tools/RenderEditor.cpp` builds as an optional console app that renders the
editor to a PNG and runs an offline DSP check (tail decay, headroom, no
non-finite samples) without needing a host:

```bash
cmake -B build -DDIAMONDROOM_BUILD_SCREENSHOT=ON
cmake --build build --config Release --target DiamondRoomShot
./build/DiamondRoomShot_artefacts/Release/DiamondRoomShot.exe --audio
./build/DiamondRoomShot_artefacts/Release/DiamondRoomShot.exe --ui
./build/DiamondRoomShot_artefacts/Release/DiamondRoomShot.exe --drive
./build/DiamondRoomShot_artefacts/Release/DiamondRoomShot.exe --tube
./build/DiamondRoomShot_artefacts/Release/DiamondRoomShot.exe --presets
./build/DiamondRoomShot_artefacts/Release/DiamondRoomShot.exe panel.png 2001
```

`--ui` checks that every fader's drawn cap and its draggable region agree at
several window sizes. That one is worth keeping: a `Slider` subclass that
overrides `resized()` without calling the base leaves JUCE's draggable region
one pixel wide, and the fader becomes impossible to set.

`--drive` runs an FFT over the Drive stage and reports even and odd harmonic
content, level change and high-band tilt at several settings. Both tone and
level have to be measured on noise rather than on a sine: a sine sits below the
tone filtering, so it shows neither the level the filtering costs on real
material nor the darkening itself, since adding harmonics to a sine raises its
spectral centroid however dark the stage is.

At Drive 0 / 10 it currently reports:

| | 0 | 10 |
| --- | --- | --- |
| even harmonics | -144 dBc | -12.5 dBc |
| odd harmonics | -144 dBc | -8.8 dBc |
| broadband level | 0 dB | 0 dB |
| high band tilt | +10.4 dB | -10.0 dB |

`--tube` checks Mix 0 is a bit-exact bypass and Mix 10 both compresses and
stays roughly level-neutral at the -14 dBFS its make-up is designed around.

`--presets` round-trips the preset store, checks undo and redo restore parameter
values, and checks the remembered window size survives a state save and reload.

## Layout

```
Source/
  PluginProcessor.*     chain wiring, dry-delay compensation, master mix
  PluginEditor.*        rack panel layout, frame, preset and settings menus
  Parameters.*          parameter layout and cached atomics
  PresetManager.*       factory tables and the user preset store
  dsp/
    DspUtils.h          filters, delay lines, all-passes, FDN mixing
    OneKnobDriver.*     Drive
    HReverb.*           FDN hall with early reflections and tail compression
    MannyMReverb.*      chamber with output distortion and phaser
    ValhallaVerb.*      Concert Hall, 1970s colour
    TrueVerb.*          geometric room simulator
    CleanVoiceTube.*    CleanVoice's valve compressor, behind a Mix control
  gui/
    Theme.*             procedural crystal, diamond-cut knob and gem artwork
    MetalKnob.*         MetalSlider.*      PanelSection.*
    RackButton.*        top rail switches and the preset name plate
```
