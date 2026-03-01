/**
 * @file plank.cpp
 * @brief Plank — second BuddhaBox application patch.
 *
 * Build:  pio run -e plank
 * Flash:  pio run -e plank -t upload
 *
 * Wire up DSP modules here following the same pattern as main.cpp:
 *   1. Declare patch parameters as globals.
 *   2. Instantiate DSP objects as globals (no heap allocation).
 *   3. Implement audioCallback() — called by DMA at 44.1 kHz.
 *      Process exactly one stereo sample pair per loop iteration.
 *      No I/O, no blocking calls, no branches on slow state here.
 *   4. Implement setup() — initialise modules, seed RNG, call
 *      AudioEngine::begin() last.
 *   5. Implement loop() — poll sensors, update non-blocking drivers.
 */

#include <Arduino.h>
#include <Wire.h>
#include "audio_engine.h"
#include "dsp_common.h"
#include "mpr121_input.h"
#include "oscillator.h"
#include "colour.h"
#include "moog_filter.h"
#include "lfo.h"
#include "filter.h"
#include "delay.h"
#include "triggered_osc.h"

MPR121Input touch;




// ============================================================
// Patch Parameters
// ============================================================


// ============================================================
// DSP Objects & Runtime State
// ============================================================


Colour colour;
SVFilter filter;
TriggeredOsc oscs[12];
LFO lfo;
DelayLine<20000> delay1;

// ============================================================
// Audio Callback
// Called by DMA interrupt once per audio block (256 samples).
// DO NOT call analogRead(), Serial, delay(), or millis() here.
// ============================================================
int n = 0;
void audioCallback(int16_t *buffer, uint16_t length) {
  for (uint16_t i = 0; i < length; i += 2) {
    float sample = 0;
    float num_active = 0;
    for (int j = 0; j < 12 ; j++) {
      TriggeredOsc *t = &oscs[j];

      t->setFrequency(colour.getNote(3, j));
      
      if (touch.triggered(j)) {
        t->trigger();
      }

      if (touch.released(j)) {
        t->released();
      }

      if (t->isActive()) {
        sample += t->process();
      }
    }

    
    float fco = colour.getNote(4, 0) + (lfo.process() * 150.0f);

    
    filter.setFrequency(fco);

    sample = sample * 0.5f;
    
    filter.process(sample);
    sample = filter.lowpass();

    float delayed = delay1.read(11000.0f);
    delay1.write(sample + (delayed * 0.5f));

    sample = (sample + delayed) * 0.33f;

    
    sample = softclip(sample, 1.0f);
    int16_t s = (int16_t)(sample * 32000.0f);
    buffer[i]     = s;  // left channel
    buffer[i + 1] = s;  // right channel
  }
}

void i2cScan() {
  Wire.setSDA(PB9);
  Wire.setSCL(PB8);
  Wire.begin();
  Serial.println("Scanning I2C bus...");
  uint8_t found = 0;
  for (uint8_t addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      Serial.print("  Found device at 0x");
      Serial.println(addr, HEX);
      found++;
    }
  }
  if (found == 0) {
    Serial.println("  No I2C devices found.");
  }
  Serial.println("Scan complete.");
}

void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.println("ready");

  if (!touch.begin()) {
    Serial.println("MPR121 not found!");
  }
  delay(200);  // allow MPR121 filter pipeline to fill before dumping
  touch.diagnosticDump();

  colour.setRoot(NOTE_C2 + 2);
  lfo.setFrequency(0.5f);
  filter.setResonance(0.4f);
  AudioEngine::begin();
}

void loop() {
  delay(10);
  touch.update();

  for (int i = 0 ; i < 12 ; i++) {
    if (touch.isHeld(i)) {
      Serial.println(i);
    }
  }
  
}
