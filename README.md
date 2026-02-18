# ☸️ BuddhaBox

**BuddhaBox** is an open-source, generative ambient audio framework built for the **STM32F401CC "Black Pill"** microcontroller and the **UDA1334A I2S DAC**.

Designed with an educational focus, it provides a "modular synth" approach to DSP, allowing you to wire together oscillators, filters, and effects directly in C++ to create evolving soundscapes, meditative drones, and generative music.

---

## ✨ Features

- **Sample-by-Sample DSP:** Every module processes one sample at a time for high precision and modular flexibility.
- **Floating Point Engine:** Internal math uses `float` (-1.0 to 1.0) for high fidelity and ease of development.
- **Hardware Accelerated:** Leverages the ARM Cortex-M4 Floating Point Unit (FPU) for efficient real-time audio.
- **Header-Only Modules:** Highly optimized, inlined DSP code for maximum performance.
- **Educational Design:** Clear, well-documented code designed to teach the fundamentals of digital signal processing.

## 🛠 Hardware Requirements

### Core Components
- **MCU:** [STM32F401CC "Black Pill"](https://www.st.com/en/microcontrollers-microprocessors/stm32f401cc.html) (84MHz ARM Cortex-M4)
- **DAC:** [UDA1334A I2S Stereo DAC Breakout](https://www.adafruit.com/product/3678)

### Pinout Configuration
| Component | Function | Pin |
| :--- | :--- | :--- |
| **UDA1334A** | I2S Word Select (LRCLK) | `PB12` |
| **UDA1334A** | I2S Bit Clock (BCLK) | `PB13` |
| **UDA1334A** | I2S Data In (DIN) | `PB15` |
| **UDA1334A** | Power | `3.3V` / `GND` |
| **Sensors** | Analog Input (ADC) | `PA0` (and above) |

---

## 🚀 Getting Started

### 1. Prerequisites
Install [PlatformIO](https://platformio.org/) (VS Code extension or CLI).

### 2. Clone and Build
```bash
git clone https://github.com/yourusername/buddhabox.git
cd buddhabox
pio run
```

### 3. Flash to Hardware
To upload via the built-in USB-C (DFU mode):
1. Hold the **BOOT0** button.
2. Press and release **NRST** (Reset).
3. Release **BOOT0**.
4. Run:
   ```bash
   pio run -t upload
   ```

---

## 🏗 Architecture

BuddhaBox uses a **pull-based audio engine** with DMA double-buffering. You define your "patch" in the `audioCallback()` inside `main.cpp`.

### Modular Example
```cpp
// A simple sine wave oscillator through a low-pass filter
float out = oscillator.process();
out = filter.process(out);
return out;
```

### Core Modules
- `oscillator.h`: Sine, Saw, Square, Triangle waveforms.
- `filter.h`: State Variable Filter (LP, HP, BP).
- `delay.h`: Interpolated circular buffer for echoes and Karplus-Strong.
- `envelope.h`: Attack-Release envelopes.
- `lfo.h`: Sub-audio rate modulation.

---

## 📚 Documentation

The project includes a comprehensive web-based guide and API reference. To view it, open `docs/index.html` in your favorite browser.

**Documentation includes:**
- Full API Reference for every module.
- Hardware wiring diagrams.
- Composition guide for building new modules.
- Examples of generative "patches."

---

## 📝 License
This project is provided as-is for educational and creative purposes. (Check the repository for specific license details).
