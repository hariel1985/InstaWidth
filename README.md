# InstaWidth

Free, open-source M/S stereo width plugin built with JUCE. Available as VST3, AU and LV2.

![VST3](https://img.shields.io/badge/format-VST3-blue) ![AU](https://img.shields.io/badge/format-AU-blue) ![LV2](https://img.shields.io/badge/format-LV2-blue) ![C++](https://img.shields.io/badge/language-C%2B%2B17-orange) ![JUCE](https://img.shields.io/badge/framework-JUCE-green) ![License](https://img.shields.io/badge/license-GPL--3.0-lightgrey) ![Build](https://github.com/hariel1985/InstaWidth/actions/workflows/build.yml/badge.svg)

## What is it?

InstaWidth is a Mid/Side stereo-width tool with **three frequency bands**, a **Monomaker** for low-end mono compatibility, a **side-channel Tilt EQ**, a **side de-esser**, a **live X/Y goniometer** and a **correlation meter** with mono-compatibility warning.

A single global switch flips the whole band-splitting + tilt + monomaker chain between **Minimum Phase** (low-latency IIR Linkwitz-Riley) and **Linear Phase** (zero phase-distortion FIR with selectable resolution). The dynamic side de-esser detector is always IIR (a detector cannot be linear-phase in any meaningful sense), but it only ever modulates the side gain — never colours the phase.

## Why M/S width done this way?

A naive stereo widener that just amplifies the difference signal sounds wider but creates **phase problems**:

- bass loses focus and disappears on mono playback,
- mid-band imaging smears (de-essers, vocals on the side go wider too),
- treble harshness gets exaggerated unintentionally.

InstaWidth solves this by giving you **per-band width control**, a **Monomaker** that collapses the side below a configurable frequency, a **side Tilt** to gently balance side spectrum, and a **side de-esser** to tame sibilance without losing width elsewhere. The Linear Phase mode preserves transients on drums and full mixes by eliminating crossover phase smear.

## Download

**[Latest Release](https://github.com/hariel1985/InstaWidth/releases)**

### Windows
| File | Description |
|------|-------------|
| `InstaWidth-VST3-Win64.zip` | VST3 plugin — copy to `C:\Program Files\Common Files\VST3\` |

### macOS (Universal Binary: Apple Silicon + Intel)
| File | Description |
|------|-------------|
| `InstaWidth-VST3-macOS.zip` | VST3 plugin — copy to `~/Library/Audio/Plug-Ins/VST3/` |
| `InstaWidth-AU-macOS.zip` | Audio Unit — copy to `~/Library/Audio/Plug-Ins/Components/` |

### Linux (x64, built on Ubuntu 22.04)
| File | Description |
|------|-------------|
| `InstaWidth-VST3-Linux-x64.zip` | VST3 plugin — copy to `~/.vst3/` |
| `InstaWidth-LV2-Linux-x64.zip` | LV2 plugin — copy to `~/.lv2/` |

> **macOS note:** Builds are Universal Binary. Not code-signed — remove the quarantine flag after copying:
> ```bash
> xattr -cr ~/Library/Audio/Plug-Ins/VST3/InstaWidth.vst3
> xattr -cr ~/Library/Audio/Plug-Ins/Components/InstaWidth.component
> ```

## Features

### Three-band M/S width
- Fixed 3 bands: **Low / Mid / High**
- Two adjustable crossover frequencies (default 150 Hz and 2 kHz)
- Per-band **Width**: 0% (mono) … 100% (untouched) … 200% (double side)
- Crossover slope: **Linkwitz-Riley 24 dB/oct** in Minimum Phase mode, brickwall-equivalent FIR in Linear Phase mode

### Monomaker (phase-safety low-end)
- High-pass on the **side** channel below a configurable cutoff
- Range **20–500 Hz**, default **120 Hz**
- On/off toggle — when off, all bands process freely
- Ensures kicks, basses and sub content stay perfectly mono

### Side Tilt EQ
- A single **tilt** control: negative tilts spectrum down (warmer side), positive tilts up (airier side)
- Range ±6 dB at 20 Hz / 20 kHz, pivot at 1 kHz
- Affects the side channel only — leaves the mid (centre) untouched

### Side De-esser
- Bandpass-detector based dynamic gain reduction on the side channel
- Adjustable **center frequency** (1–12 kHz), **threshold** (-40…0 dB), **range** (0…24 dB)
- Detector is always IIR (intentional — minimum-phase detection)
- The gain reduction itself is a real-time multiply on side; phase-transparent

### Processing Mode
A single global switch:

- **Minimum Phase** — IIR all the way: zero added latency, classic feel. Good for tracking, busses, live monitoring.
- **Linear Phase** — All band splitting, tilt and Monomaker are baked into a single symmetric FIR applied to the side channel. Zero phase distortion. Ideal for mastering and parallel busses.

### Configurable FIR resolution (Linear Phase mode)

| Taps | Latency (44.1 kHz) | Best for |
|------|---------------------|----------|
| 512 | ~6 ms | Low-latency monitoring |
| 1024 | ~12 ms | Tracking |
| **2048** | **~23 ms** | **Default — mixing** |
| 4096 | ~46 ms | Detailed work |
| 8192 | ~93 ms | Mastering |
| 16384 | ~186 ms | Maximum precision |

Latency is reported to the DAW and compensated automatically.

### Live X/Y Goniometer
- Lissajous-style stereo image scope rotated 45° (vertical = mono / mid, horizontal = side)
- Phosphor-style persistent decay
- Shows post-processing signal so the effect of your settings is immediately visible

### Correlation Meter
- Pearson correlation between L and R, integrated over a short window
- Green (>0.5) / Yellow (0…0.5) / Red (<0) colour scale
- **Negative-correlation warning**: a flashing notice appears when correlation goes below 0 — meaning your stereo image will partially cancel in mono

### GUI
- Dark modern UI matching the InstaLPEQ visual family
- 3D metal knobs with multi-layer glow (orange = level/freq/width, blue = Q-like / mode)
- Carbon-fibre background texture
- Rajdhani custom font (embedded)
- Fully resizable, proportional scaling
- All state saved/restored with the DAW session

## How it works

### Minimum Phase mode

```
L,R → M/S encode →
  Side path: Tilt (LF + HF shelves) →
             De-esser (BP detector → side gain mod) →
             LR4 3-band split → per-band width gain → sum →
             Monomaker HP →
  Mid path:  passthrough
M/S decode → L,R out
```

### Linear Phase mode

All of the static side-channel processing (tilt + multiband width + monomaker) is folded into a single magnitude response:

1. For each FFT bin: compute tilt magnitude × monomaker HP magnitude × multiband per-bin width gain (with crossover-shape weighting)
2. Inverse FFT the zero-phase magnitude into a symmetric impulse response
3. Apply a Blackman-Harris window
4. Normalise the FIR so a flat side response is unity
5. Convolve on the side channel via JUCE's partitioned `dsp::Convolution`
6. The dynamic side de-esser runs on top of the convolved side signal — its detector remains IIR (by design), the gain reduction is a sample-by-sample multiply that does not affect phase

The FIR is regenerated on a background thread whenever any parameter changes, so parameter automation is click-free.

## Building

### Requirements
- CMake 3.22+
- JUCE framework (cloned to `../JUCE` relative to project)

#### Windows
- Visual Studio 2022 Build Tools (C++ workload)

#### macOS
- Xcode 14+

#### Linux (Ubuntu 22.04+)
```bash
sudo apt-get install build-essential cmake git libasound2-dev \
  libfreetype6-dev libx11-dev libxrandr-dev libxcursor-dev \
  libxinerama-dev libwebkit2gtk-4.1-dev libcurl4-openssl-dev
```

### Build Steps

```bash
git clone https://github.com/juce-framework/JUCE.git ../JUCE
cmake -B build -G "Visual Studio 17 2022" -A x64    # Windows
cmake -B build -G Xcode                              # macOS
cmake -B build -DCMAKE_BUILD_TYPE=Release             # Linux
cmake --build build --config Release
```

Output:
- VST3: `build/InstaWidth_artefacts/Release/VST3/InstaWidth.vst3`
- AU: `build/InstaWidth_artefacts/Release/AU/InstaWidth.component` (macOS)
- LV2: `build/InstaWidth_artefacts/Release/LV2/InstaWidth.lv2`

## Tech Stack

- **Language:** C++17
- **Framework:** JUCE 8
- **Build:** CMake + MSVC / Xcode / GCC
- **Audio DSP:** juce::dsp (FFT, Convolution, IIR LR-cascade, ProcessorChain)
- **Font:** Rajdhani (SIL Open Font License)

## License

GPL-3.0-or-later. See [LICENSE](LICENSE).
