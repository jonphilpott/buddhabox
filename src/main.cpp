
#include "audio_engine.h"
#include "oscillator.h"
#include "noise.h"
#include "random.h"
#include "lfo.h"
#include "filter.h"
#include "delay.h"
#include "clock.h"
#include "midi_utils.h"
#include "analog_input.h"
#include "envelope.h"

PinkNoise pink_noise(69);
Oscillator bass_osc;
Oscillator osc;
Envelope env;
TempoClock clock(88.0f, 2);  // 140 BPM, eighth notes

// D major arpeggio: D4, F#4, A4, D5
const uint8_t arpeggio[] = {
  NOTE_C4 + 2,   // D4  (MIDI 62)
  NOTE_C4 + 4,    // E4
  NOTE_C4 + 6,   // F#4 (MIDI 66)
  NOTE_C4 + 9,   // A4  (MIDI 69)
  NOTE_C4 + 11,   // B4
  NOTE_C5 + 2    // D5  (MIDI 74)
};
const int arpLen = 6;

LFSR rng;

SVFilter filter1;
SVFilter filter2;
SVFilter noise_filter;

DelayLine<8192> delay1;

LFO filterLFO;
LFO delayLFO;

void audioCallback(int16_t* buffer, uint16_t length) {
  for (uint16_t i = 0; i < length; i += 2) {

    // On each clock tick, advance to the next arpeggio note
    if (clock.process()) {
      if (rng.nextFloat() > 0.8) {
        int step = clock.getTick() % arpLen;
        osc.setFrequency(midiToFreq(arpeggio[step]));
        env.reset();
        env.trigger();  // Percussive: auto-releases after attack
      }
    }

    float lfotime = filterLFO.process();

    float noise = pink_noise.process() * 0.3f;
    noise_filter.setFrequency(mapf(lfotime, -1.0f, 1.0f, 100.0f, 200.0f));
    noise_filter.process(noise);
    noise = noise_filter.lowpass();
    
    // saw oscillator shaped by the envelope
    float sample = osc.process() * env.process() * 0.3f;

    filter2.setFrequency(mapf(lfotime, -1.0f, 1.0f, 200.0f, 250.0f));
    filter2.process(bass_osc.process());
    
    // add bass
    sample = sample + filter2.lowpass() * 0.2f;
    sample = sample + noise;
    
    float cutoff = mapf(lfotime, -1.0f, 1.0f, 600.0f, 2000.0f);
    filter1.setFrequency(cutoff);        
    filter1.process(sample);
    sample = filter1.lowpass();

    float dlfo = delayLFO.process();

    float delayed = delay1.read(mapf(dlfo, -1.0f, 1.0f, 8000.0f, 8100.0f));
    delay1.write(sample + delayed * 0.8f);

    float out = sample * 0.6 + delayed * 0.4f;
    out = out * 0.5;

    int16_t s = (int16_t)(softclip(out, 1.0f) * 32000.0f);
    buffer[i]     = s;
    buffer[i + 1] = s;
  }
}

void setup() {
  bass_osc.setWaveform(Waveform::SAW);
  bass_osc.setAmplitude(1.0f);
  bass_osc.setFrequency(midiToFreq(NOTE_C2 + 2));
  osc.setWaveform(Waveform::SAW);
  osc.setAmplitude(1.0f);
  env.setAttack(0.02f);
  env.setRelease(0.8f);
  filter1.setFrequency(500.0f);
  filter2.setFrequency(100.0f);
  filter2.setResonance(0.6f);
  filterLFO.setWaveform(Waveform::SINE);
  filterLFO.setFrequency(0.125f);
  delayLFO.setWaveform(Waveform::SINE);
  delayLFO.setFrequency(0.5f);
  AudioEngine::begin();
}

void loop() { delay(10); }
