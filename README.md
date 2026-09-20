<div align="center">

# DIAMOND ROOM

**Reflect · Shape · Space**

Four parallel reverbs. Harmonic drive. Blended tube colour.

[![Build VST3](https://github.com/zenoxart/DiamondRoom-VST/actions/workflows/build.yml/badge.svg?branch=main)](https://github.com/zenoxart/DiamondRoom-VST/actions/workflows/build.yml)
![Format](https://img.shields.io/badge/format-VST3-aed9ef?style=flat-square)
![Platform](https://img.shields.io/badge/platform-Windows_x64-91b6d1?style=flat-square)
![Built with JUCE](https://img.shields.io/badge/built_with-JUCE_8-637e99?style=flat-square)

### [↓ Download VST3 for Windows](https://github.com/zenoxart/DiamondRoom-VST/releases/download/latest-build/DiamondRoom-Windows-x64.zip)

[Build details & checksum](https://github.com/zenoxart/DiamondRoom-VST/releases/tag/latest-build) · [Installation](#installation) · [Controls](#controls) · [Build from source](#development)

</div>

![Diamond Room — crystal rack interface with Drive, four reverb sections, Tube and master Mix](docs/images/diamond-room.png)

Diamond Room brings the `DiamondRoom.fst` FL Studio Patcher concept into a
self-contained JUCE/C++ plugin. Blend four reverb characters, push the entire
signal through Drive, and add tube compression to the reverb sum.

All audio processing is native. Waves, Valhalla and FL Studio are not required.
The reverbs are custom implementations inspired by the original chain, not the
original commercial plugins.

## The sound

| Shape | Space | Character |
| :--- | :--- | :--- |
| **Drive** adds even and odd harmonics, with darker highs as you push it. | **Four parallel reverbs** give you separate tone, time, character and level controls. | **Tube** blends the CleanVoice valve stage into the combined reverb signal. |
| Oversampled saturation with automatic level compensation. | Enable each section independently and blend with the master Mix. | Eight factory presets, undo/redo and a resizable crystal interface. |

## Installation

1. **[Download the Windows x64 VST3 ZIP](https://github.com/zenoxart/DiamondRoom-VST/releases/download/latest-build/DiamondRoom-Windows-x64.zip)** and extract it.
2. Close your DAW, then copy the **entire `Diamond Room.vst3` folder** into:
   ```text
   C:\Program Files\Common Files\VST3\
   ```
3. Open your DAW and rescan VST3 plugins. In FL Studio, open **Options → Manage plugins → Find installed plugins**.
4. Add **Diamond Room** to a mixer effect slot.

Keep the `Contents` folder inside the VST3 bundle intact. Replacing an older
version requires closing the DAW first.

**Download channel:** unsigned Windows x64 development build. The link above
updates after each successful `main` build. The release page includes the exact
commit and SHA-256 checksum. macOS and Linux binaries are not currently provided.

## Controls

| Section | Controls | Character |
| :--- | :--- | :--- |
| **Drive** | Drive · 0–10 | Saturation and progressively darker highs across the entire signal, including the dry path. |
| **H-Reverb** | Tone · Time · H-Mix | Hall-style tail with tonal shaping and early reflections. |
| **MannyM Reverb** | Distortion · Amount · Mix | Chamber character with output saturation and density control. |
| **Valhalla Reverb** | HighCut · Decay · Mix | Modulated hall with adjustable brightness and tail length. |
| **True Verb** | Distance · Roomsize · Mix | Room reflections, depth and air absorption. |
| **Tube** | Tube · 0–10 | Blend of the CleanVoice valve compressor on the reverb sum; 0 bypasses it, 10 applies it fully. |
| **Master** | Mix · 0–100% | Blend between the driven dry signal and the processed reverb sum. |

The glowing LEDs switch individual reverbs on and off. Section mix faders set
each reverb's contribution to the sum.

```mermaid
flowchart LR
    IN[Input] --> DRIVE[Drive]
    DRIVE --> DRY[Dry path]
    DRIVE --> H[H-Reverb]
    DRIVE --> M[MannyM Reverb]
    DRIVE --> V[Valhalla Reverb]
    DRIVE --> T[True Verb]
    H --> SUM[Section mixes + sum]
    M --> SUM
    V --> SUM
    T --> SUM
    SUM --> TUBE[Tube blend]
    DRY --> MIX[Master Mix]
    TUBE --> MIX
    MIX --> OUT[Output]
```

## Presets & workflow

Start with **Diamond Room**, **Vocal Plate**, **Tight Room**, **Cathedral**,
**Dark Chamber**, **Driven Wash**, **Crushed Verb** or **Subtle Air**.

- Click the preset name to load, save or delete a user preset. The arrows step through presets.
- Use undo and redo for parameter edits and preset changes. Each drag is one undo step.
- Choose a window size with the gear menu or resize from the corner. The size is saved with the session.
- User presets live in `%APPDATA%\DiamondRoom\Presets` as `.drpreset` files. An asterisk marks an edited preset.

## Automated builds

Every branch push and pull request triggers the [Windows build workflow](https://github.com/zenoxart/DiamondRoom-VST/actions/workflows/build.yml).
It checks out the pinned JUCE version, compiles the VST3, runs the offline checks,
and packages the complete plugin bundle. Each successful build has a downloadable
artifact retained for 30 days.

Successful builds on `main` also update the **[public VST3 download](https://github.com/zenoxart/DiamondRoom-VST/releases/download/latest-build/DiamondRoom-Windows-x64.zip)**.
Pull requests and other branches never replace that download. The workflow can
also be started manually from the Actions tab.

## Development

<details>
<summary><strong>Build from source</strong></summary>

Requires CMake 3.22+, Visual Studio 2022 with C++ tools, and JUCE 8.0.4.

```powershell
git clone https://github.com/zenoxart/DiamondRoom-VST.git
cd DiamondRoom-VST
git clone --depth 1 --branch 8.0.4 https://github.com/juce-framework/JUCE.git JUCE
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DDIAMONDROOM_COPY_PLUGIN_AFTER_BUILD=OFF
cmake --build build --config Release
```

The VST3 and standalone app are generated under `build/DiamondRoom_artefacts/Release/`.
Set `DIAMONDROOM_COPY_PLUGIN_AFTER_BUILD=ON` to copy the VST3 into the system plugin
folder after building. This is the default for local builds; CI disables it.

All artwork is embedded, including the complete crystal background. No external
image files or image-processing tools are needed to run or build the plugin.

</details>

<details>
<summary><strong>Run checks & refresh the screenshot</strong></summary>

```powershell
cmake -S . -B build -DDIAMONDROOM_BUILD_SCREENSHOT=ON
cmake --build build --config Release --target DiamondRoomShot
$tool = './build/DiamondRoomShot_artefacts/Release/DiamondRoomShot.exe'
& $tool --audio
& $tool --ui
& $tool --drive
& $tool --tube
& $tool --presets
& $tool --lifecycle
& $tool docs/images/diamond-room.png 1600
```

| Check | Coverage |
| :--- | :--- |
| `--audio` | Tail decay, headroom and finite output samples. |
| `--ui` | Fader drawing and mouse alignment at multiple sizes. |
| `--drive` | Harmonics, level compensation and high-frequency darkening. |
| `--tube` | Bypass, compression and blend behaviour. |
| `--presets` | Preset save/load, undo/redo and saved window size. |
| `--lifecycle` | 30 processor removals, 60 editor closures and five GUI shutdown cycles. |

These are offline checks. DAW-specific behaviour still needs host testing.
The preset check creates and removes a temporary preset in the user preset folder.

</details>

<details>
<summary><strong>Project structure</strong></summary>

```text
Source/       Processor, editor, parameters and presets
  dsp/        Drive, four reverbs and the CleanVoice valve stage
  gui/        Knobs, faders, panels and drawing helpers
Assets/       Embedded background and control artwork
Tools/        Offscreen renderer and offline checks
docs/images/ README screenshot
.github/      Windows build and download publishing workflow
```

</details>

---

Project code: [MIT](LICENSE). JUCE and other dependencies retain their own licences.
