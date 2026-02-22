
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
#include "colour.h"

PinkNoise pink_noise(69);

Oscillator osc;
Oscillator osc2;
Oscillator arp;

Envelope env;
Envelope arp_env;
TempoClock clock(72.0f, 2); // 140 BPM, eighth notes
TempoClock fast_clock(72.0f, 4);

LFSR rng;

SVFilter filter1;
SVFilter noise_filter;
SVFilter output_highpass;
SVFilter arp_filter;

DelayLine<12110> delay1;

LFO filterLFO;
LFO delayLFO;
LFO delayFeedbackLFO;
LFO filterLFOLFO;


Colour colour;

uint8_t bass_tracker = 0;
uint8_t bass_note = 0;

float arp_sample = 0;
float melody_overtone_vol = 0.125;

float arp_trigger_prob = 0.75;
float arp_clock_prob   = 0.5;

float tung_prob = 0.8;

// noise source
AnalogInput lightSensor(PA0, 0.03f);

void audioCallback(int16_t *buffer, uint16_t length) {
  for (uint16_t i = 0; i < length; i += 2) {

    // On each clock tick, advance to the next arpeggio note
    if (clock.process()) {

      // maybe flip a bit in the LFSR to keep things different.
      rng.add_entropy(lightSensor.readRaw());
      
      if (rng.nextFloat() > tung_prob) {
        int step = clock.getTick() % 5;
        uint8_t oct = rng.nextRange(2,3);
        float n = colour.getNote(oct, 4-step);
        osc2.setFrequency(n * 2.12);
        osc.setFrequency(n);
        env.setRelease(0.75 + rng.nextFloat() * 1.5f);
        env.reset();
        env.trigger(); // Percussive: auto-releases after attack
      }


      // change up colour
      if ((clock.getTick() % 128) == 0 && rng.nextFloat() > 0.9) {
        colour.setColour(rng.nextRange(0, 15));
        // and tempo
        float new_tempo = 65 + (rng.nextFloat() * 18);
        clock.setBPM(new_tempo);
        fast_clock.setBPM(new_tempo);
        melody_overtone_vol = rng.nextFloat() * 0.125;
        arp_filter.setFrequency(400 + rng.nextFloat() * 200.0f);
        arp_filter.setResonance(rng.nextFloat() * 0.8);
        arp_trigger_prob = 0.75 + rng.nextFloat() * 0.24;
        arp_clock_prob = 0.33 + rng.nextFloat() * 0.66;
        tung_prob = 0.85 + rng.nextFloat() * 0.14;
      }

      if ((clock.getTick() % 32) == 0) {
        filterLFO.setFrequency(0.0125f + rng.nextFloat() * 0.0250);
      }


      if ((clock.getTick() % 16 == 0) && rng.nextFloat() > arp_trigger_prob) {
        arp_env.trigger();
      }

    }
    
    if (fast_clock.process()) {
      if (rng.nextFloat() > 0.5) {
        arp.setFrequency(colour.getNote(4, rng.nextRange(0, 4)));
      }
    }

    float lfotime = filterLFO.process();

    float noise = pink_noise.process() * 0.25f;
    float noise_lfo_max = 800 + (filterLFOLFO.process() * 150);
    noise_filter.setFrequency(mapf(lfotime, -1.0f, 1.0f, 800.0f, noise_lfo_max));
    noise_filter.process(noise);
    noise = noise_filter.bandpass();

    // melody oscillator
    float sample = (((osc.process() * 0.75) + (osc2.process() * melody_overtone_vol))  * env.process()) * 0.125;


    // add noise
    sample = sample + noise;

    // add arp
    arp_filter.process(arp.process());
    sample = sample + (arp_filter.lowpass() * arp_env.process() * 0.2);
    

    float cutoff = mapf(lfotime, -1.0f, 1.0f, 500.0f, 2000.0f);
    filter1.setFrequency(cutoff);
    filter1.process(sample);
    sample = filter1.lowpass();

    float dlfo = delayLFO.process();
    float delay_fb = mapf(delayFeedbackLFO.process(), -1.0f, 1.0f, 0.6f, 0.9f);
    float delayed = delay1.read(mapf(dlfo, -1.0f, 1.0f, 11950.0f, 12100.0f));
    delay1.write(sample + delayed * delay_fb);

    float out = (sample * 0.75) + (delayed * 0.25);

    output_highpass.process(out);
    out = output_highpass.highpass();
    

    int16_t s = (int16_t)(softclip(out, 1.2f) * 32000.0f);
    buffer[i] = s;
    buffer[i + 1] = s;
  }
}

void setup() {
  osc.setWaveform(Waveform::SINE);
  osc.setAmplitude(1.0f);
  osc2.setWaveform(Waveform::SINE);
  
  arp.setWaveform(Waveform::SAW);
  arp_filter.setFrequency(400.0f);

  env.setAttack(0.15f);
  env.setRelease(0.2f);
  arp_env.setAttack(4.0f);
  arp_env.setRelease(18.0f);

  filter1.setFrequency(500.0f);

  filterLFO.setWaveform(Waveform::SINE);
  filterLFO.setFrequency(0.025f);
  filterLFO.setDivider(64);   // 0.025 Hz — compute at ~689 Hz, imperceptible

  filterLFOLFO.setWaveform(Waveform::SINE);
  filterLFOLFO.setFrequency(0.00125f);
  filterLFOLFO.setDivider(256);
  
  delayLFO.setWaveform(Waveform::SINE);
  delayLFO.setFrequency(1.25f);
  delayLFO.setDivider(64);    // 0.5 Hz — still ~1378 updates/cycle, very smooth

  delayFeedbackLFO.setWaveform(Waveform::SINE);
  delayFeedbackLFO.setFrequency(0.025f);
  delayFeedbackLFO.setDivider(64); // 0.025 Hz — same as filterLFO

  output_highpass.setFrequency(50.0f);
  
  colour.setColour(7);


  // seed the RNG with some noise.
  for (int i = 0 ; i < 10 ; i++) {
    lightSensor.update();
    rng.add_entropy(lightSensor.readRaw());
    rng.next();
    delay(10);
  }

  AudioEngine::begin();
}

void loop() {
  delay(10);
  lightSensor.update();
}
