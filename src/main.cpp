
#include "analog_input.h"
#include "audio_engine.h"
#include "clock.h"
#include "delay.h"
#include "envelope.h"
#include "filter.h"
#include "lfo.h"
#include "midi_utils.h"
#include "noise.h"
#include "oscillator.h"
#include "random.h"

PinkNoise pink_noise(69);
Oscillator bass_osc;
Oscillator osc;
Envelope env;
TempoClock clock(72.0f, 2); // 140 BPM, eighth notes

// japanese In scale
const uint8_t arpeggio[] = {
    NOTE_C4 + 2,  // D4  (MIDI 62)
    NOTE_C4 + 4,  // Eb4
    NOTE_C4 + 6,  // G4
    NOTE_C4 + 9,  // A4  (MIDI 69)
    NOTE_C4 + 11, // Bb4
    NOTE_C5 + 2   // D5  (MIDI 74)
};
const int arpLen = 6;

LFSR rng;

SVFilter filter1;
SVFilter filter2;
SVFilter noise_filter;

DelayLine<8192> delay1;

LFO filterLFO;
LFO delayLFO;
LFO delayFeedbackLFO;

void audioCallback(int16_t *buffer, uint16_t length) {
  for (uint16_t i = 0; i < length; i += 2) {

    // On each clock tick, advance to the next arpeggio note
    if (clock.process()) {
      if (rng.nextFloat() > 0.5) {
        int step = clock.getTick() % arpLen;
        osc.setFrequency(midiToFreq(arpeggio[step]));
        env.reset();
        env.trigger(); // Percussive: auto-releases after attack
      }

      bass_osc.setFrequency(midiToFreq(arpeggio[clock.getTick() % arpLen]) * 2);
    }

    float lfotime = filterLFO.process();

    float noise = pink_noise.process() * 0.1f;
    noise_filter.setFrequency(mapf(lfotime, -1.0f, 1.0f, 800.0f, 1000.0f));
    noise_filter.process(noise);
    noise = noise_filter.bandpass();

    // saw oscillator shaped by the envelope
    float sample = (osc.process() * env.process()) * 0.1f;

    filter2.setFrequency(mapf(lfotime, -1.0f, 1.0f, 400.0f, 500.0f));
    filter2.process(bass_osc.process());

    // add bass
    sample = sample + filter2.lowpass() * 0.08f;
    // add noise
    sample = sample + noise;

    float cutoff = mapf(lfotime, -1.0f, 1.0f, 500.0f, 2000.0f);
    filter1.setFrequency(cutoff);
    filter1.process(sample);
    sample = filter1.lowpass();

    float dlfo = delayLFO.process();
    float delay_fb = mapf(delayFeedbackLFO.process(), -1.0f, 1.0f, 0.2f, 0.8f);
    float delayed = delay1.read(mapf(dlfo, -1.0f, 1.0f, 8000.0f, 8100.0f));
    delay1.write(sample + delayed * delay_fb);

    float out = (sample * 0.6) + (delayed * 0.25);

    int16_t s = (int16_t)(softclip(out, 1.0) * 32000.0f);
    buffer[i] = s;
    buffer[i + 1] = s;
  }
}

void setup() {
  bass_osc.setWaveform(Waveform::TRIANGLE);
  bass_osc.setAmplitude(1.0f);
  bass_osc.setFrequency(midiToFreq(NOTE_C6 + 2));
  osc.setWaveform(Waveform::TRIANGLE);
  osc.setAmplitude(1.0f);
  env.setAttack(0.05f);
  env.setRelease(0.2f);
  filter1.setFrequency(500.0f);
  filter2.setFrequency(100.0f);
  filterLFO.setWaveform(Waveform::SINE);
  filterLFO.setFrequency(0.025f);
  filterLFO.setDivider(64);   // 0.025 Hz — compute at ~689 Hz, imperceptible
  delayLFO.setWaveform(Waveform::SINE);
  delayLFO.setFrequency(0.5f);
  delayLFO.setDivider(64);    // 0.5 Hz — still ~1378 updates/cycle, very smooth
  delayFeedbackLFO.setWaveform(Waveform::SINE);
  delayFeedbackLFO.setFrequency(0.025f);
  delayFeedbackLFO.setDivider(64); // 0.025 Hz — same as filterLFO
  AudioEngine::begin();
}

void loop() { delay(10); }
