# BuddhaBox — GEMINI Context

## Project Overview
BuddhaBox is a generative ambient audio framework built for the **STM32F401CC "Black Pill"** microcontroller and the **UDA1334A I2S DAC**. It provides a modular, sample-by-sample DSP architecture for creating evolving soundscapes, drones, and meditative audio.

The project is designed with an educational focus, featuring highly documented code and a "modular synth" approach where DSP components (oscillators, filters, envelopes) are wired together in code.

### Core Tech Stack
- **Language:** C++ (with focus on performance-critical DSP)
- **Framework:** Arduino (STM32 core)
- **Build System:** PlatformIO (Platform: `ststm32@15.4.1`)
- **Hardware Platform:** STM32F401CC (84MHz ARM Cortex-M4)
- **Audio Interface:** I2S with DMA double-buffering
- **DAC:** UDA1334A (Stereo, 16-bit, 44.1kHz)

## Building and Running

### Commands
- **Build Project:** `pio run`
- **Upload Firmware:** `pio run -t upload` (Uses DFU by default)
- **Serial Monitor:** `pio device monitor`
- **Clean Build:** `pio run -t clean`

### Deployment Notes
To upload via USB-C (DFU mode):
1. Hold **BOOT0** button.
2. Press and release **NRST** (reset).
3. Release **BOOT0**.
4. Run `pio run -t upload`.

## Hardware Specifications
- **I2S Pins:** `PB12` (LRCLK), `PB13` (BCLK), `PB15` (DIN)
- **ADC Pins:** `PA0` and above for analog sensors/pots
- **Audio Block Size:** 256 samples (stereo interleaved)
- **Sample Rate:** 44100 Hz

## Architecture & Conventions

### DSP Module Pattern
- **Sample-by-Sample:** Every module must expose a `float process()` (or `float process(float input)`) method that processes exactly one sample.
- **Header-Only:** DSP modules are primarily header-only (`.h`) to allow the compiler to inline performance-critical audio code.
- **Floating Point:** Internal math uses `float`. **All audio signals must be normalized to -1.0 to 1.0 range.**
- **Phase Accumulators:** Oscillators should use a `0.0` to `1.0` range for phase.
- **Fixed Output:** Conversion to `int16_t` happens only at the final stage in `audio_engine.cpp`.
- **No Dynamic Allocation:** All DSP objects and buffers (like `DelayLine`) must be allocated at compile time (usually as globals or static members).

### Project Structure
- `src/`: Core DSP modules and hardware drivers.
  - `dsp_common.h`: Shared utilities (`clampf`, `lerpf`, `mapf`, `softclip`).
  - `oscillator.h`, `lfo.h`, `filter.h`, `envelope.h`, `delay.h`: Core synthesis modules.
  - `noise.h`, `random.h`: Generators.
  - `karplus.h`: Plucked string synthesis.
  - `clock.h`, `midi_utils.h`: Timing and pitch utilities.
  - `analog_input.h`, `audio_engine.h/cpp`: Hardware interfacing.
- `docs/`: Project documentation.
  - `index.html`: Comprehensive web-based API reference and hardware guide.

### Coding & Documentation Standards

- **Performance:** Avoid `double`, `sinf()` (use parabolic approximations), and any branching/I/O inside the `audioCallback`.

#### Docstrings
- Every class, function, and non-trivial method MUST have a detailed docstring
- Include: purpose, arguments (with types/ranges), return values, and example usage
- Use `/** ... */` style for docstrings

#### Comments
- Write comments as if teaching a junior developer new to DSP and C++
- Explain the "why" and "how", not just "what"
- Use inline comments to explain rationale behind specific logic choices
- Educational callouts: when using advanced or non-obvious patterns, explain how they work
- Break complex functions into numbered steps using comments as a roadmap

#### Doumentation Requirements

- **Full API reference**: every class must show instantiation with constructor arguments; every method must document each input argument (name, type, range) and return type with description
- **Examples per class**: each class must include at least one runnable code example
- **Composition guide**: documentation must include a section on how to compose new classes from existing modules, with examples
- **Diagrams**: use styled HTML elements (divs/spans with monospace font and proper spacing) for diagrams — do NOT use raw ASCII art in `<pre>` tags, as character alignment breaks across fonts and browsers
- Keep examples compilable and consistent with the actual API


## Key Files
- **docs/index.html:** The definitive source of truth for hardware wiring and API usage.
- **src/main.cpp:** Example of how to compose modules into a "patch".

## Other Instructions
1. Always ask before making any code changes.
