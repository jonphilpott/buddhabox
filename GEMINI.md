# BuddhaBox — GEMINI Context

## Project Overview
BuddhaBox is a generative ambient audio framework built for the **STM32F401CC "Black Pill"** microcontroller and the **UDA1334A I2S DAC**. It provides a modular, sample-by-sample DSP architecture for creating evolving soundscapes, drones, and meditative audio.

The project is designed with an educational focus, featuring highly documented code and a "modular synth" approach where DSP components (oscillators, filters, envelopes) are wired together in code.

### Core Tech Stack
- **Language:** C++ (with focus on performance-critical DSP)
- **Framework:** Arduino (STM32 core)
- **Build System:** PlatformIO
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

## Architecture & Conventions

### DSP Module Pattern
- **Sample-by-Sample:** Every module must expose a `float process()` (or `float process(float input)`) method that processes exactly one sample.
- **Header-Only:** DSP modules are primarily header-only (`.h`) to allow the compiler to inline performance-critical audio code.
- **Floating Point:** Internal math uses `float` (range -1.0 to 1.0).
- **Fixed Output:** Conversion to `int16_t` happens only at the final stage in `audio_engine.cpp`.
- **No Dynamic Allocation:** All DSP objects and buffers (like `DelayLine`) must be allocated at compile time (usually as globals or static members).

### Project Structure
- `src/`: Core DSP modules and hardware drivers.
  - `main.cpp`: The entry point and the "patch" definition.
  - `audio_engine.h/cpp`: Hardware-level I2S/DMA management.
  - `dsp_common.h`: Shared utilities (clamp, lerp, map, softclip).
- `docs/`: Project documentation.
  - `index.html`: Comprehensive web-based API reference and hardware guide.
- `platformio.ini`: Project configuration and build flags.

### Coding Standards
- **Documentation:** Every class and public method **must** have a Doxygen-style docstring (`/** ... */`).
- **Comments:** Explain the "why" and "how" behind DSP logic, targeting a "junior developer" level of clarity.
- **Performance:** Avoid `double`, `sinf()` (use parabolic approximations), and any branching/I/O inside the `audioCallback`.

## Key Files
- `CLAUDE.md`: Detailed project guidelines and coding requirements (Precedence: High).
- `docs/index.html`: The definitive source of truth for hardware wiring and API usage.
- `src/main.cpp`: Example of how to compose modules into a "patch".
