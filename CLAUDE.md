# BuddhaBox — Project Guidelines

## Project Overview
Generative ambient audio device built on STM32F401CC Black Pill + UDA1334A I2S DAC.
A modular DSP framework for creating evolving soundscapes, drones, and meditative audio.

## Build System
- PlatformIO with Arduino framework (`ststm32@15.4.1`)
- Build: `pio run`
- Upload: `pio run -t upload`

## Architecture
- All DSP modules are header-only for inlining (performance-critical audio code)
- Every module processes **one sample at a time** via a `float process()` method
- Modules are composable: output of one feeds into input of another
- Audio engine uses I2S DMA double-buffering — user fills buffers in `audioCallback()`

## Code Style & Documentation Requirements

### Docstrings
- Every class, function, and non-trivial method MUST have a detailed docstring
- Include: purpose, arguments (with types/ranges), return values, and example usage
- Use `/** ... */` style for docstrings

### Comments
- Write comments as if teaching a junior developer new to DSP and C++
- Explain the "why" and "how", not just "what"
- Use inline comments to explain rationale behind specific logic choices
- Educational callouts: when using advanced or non-obvious patterns, explain how they work
- Break complex functions into numbered steps using comments as a roadmap

### Code Patterns
- Header-only DSP modules (except audio_engine which has hardware-specific .cpp)
- Phase accumulator pattern for oscillators (0.0-1.0 range)
- All audio signals normalized to -1.0 to 1.0 float range
- 16-bit integer conversion only at the final output stage (audio_engine)
- Use `clampf()`, `lerpf()`, `mapf()`, `softclip()` from dsp_common.h for utility operations

## Hardware Pinout
- I2S: PB12 (LRCLK), PB13 (BCLK), PB15 (DIN)
- ADC: PA0+ for analog sensors
- MCU: STM32F401CC, 84MHz, 64KB RAM, 256KB Flash

## Key Constants
- Sample rate: 44100 Hz
- Audio block size: 256 samples
- Audio format: 16-bit stereo interleaved

## DSP Modules
- `dsp_common.h` — constants, clampf, lerpf, mapf, softclip
- `oscillator.h` — Oscillator (sine/saw/square/triangle)
- `noise.h` — WhiteNoise, PinkNoise
- `random.h` — LFSR random number generator
- `lfo.h` — LFO (same waveforms, sub-audio rates)
- `envelope.h` — Envelope (attack-release with gate/trigger)
- `filter.h` — SVFilter (state variable: LP/HP/BP)
- `delay.h` — DelayLine (circular buffer, int16 storage, interpolated reads)
- `karplus.h` — KarplusStrong (plucked string synthesis)
- `clock.h` — TempoClock (sample-accurate tempo clock)
- `midi_utils.h` — midiToFreq, note constants
- `analog_input.h` — AnalogInput (smoothed ADC reader)
- `audio_engine.h/.cpp` — I2S DMA output

## Project Documentation
- HTML documentation lives at `docs/index.html`
- **MUST be updated** whenever modules, APIs, architecture, hardware setup, or examples change
- Documentation includes: architecture overview, file layout, module reference, usage examples, quick start guide, hardware wiring, and composition guide

### Documentation Requirements
- **Full API reference**: every class must show instantiation with constructor arguments; every method must document each input argument (name, type, range) and return type with description
- **Examples per class**: each class must include at least one runnable code example
- **Composition guide**: documentation must include a section on how to compose new classes from existing modules, with examples
- **Diagrams**: use styled HTML elements (divs/spans with monospace font and proper spacing) for diagrams — do NOT use raw ASCII art in `<pre>` tags, as character alignment breaks across fonts and browsers
- Keep examples compilable and consistent with the actual API
