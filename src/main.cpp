
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

// ============================================================
// Patch Parameters
// These globals shape every aspect of the sound. They are
// intentionally non-const so they can be mutated at runtime
// to let the patch evolve over time.
// ============================================================

// --- Timing & Tempo ---
float tempo_bpm             = 72.0f;    // Starting BPM shared by both clocks
float tempo_min             = 65.0f;    // Minimum BPM during random tempo evolution
float tempo_range           = 18.0f;    // Random BPM range added to tempo_min on each evolution event

// --- Melody Oscillators ---
float osc2_detune_ratio     = 2.12f;    // Frequency multiplier applied to osc2 (slightly above an octave, creates beating, for a tung drum like sound)
float osc1_gain             = 0.75f;    // Gain of the primary melody oscillator before the envelope
float melody_mix_level      = 0.125f;   // Final attenuation of the combined melody signal into the stereo mix
float melody_overtone_vol_max = 0.125f; // Maximum volume cap for the osc2 overtone when re-randomized

// --- Melody Envelope ---
float env_attack            = 0.15f;    // Attack time (s) for the percussive melody envelope
float env_release_init      = 0.2f;     // Initial release time (s); overridden on each trigger
float env_release_min       = 0.75f;    // Minimum release time (s) when randomly chosen on trigger
float env_release_range     = 1.5f;     // Random range (s) added to env_release_min on each trigger

// --- Arp Pad Envelope ---
float arp_env_attack        = 4.0f;     // Attack time (s) for the slow arp pad swell
float arp_env_release       = 18.0f;    // Release time (s) for the slow arp pad swell

// --- Arp Oscillator ---
int   arp_oct_min           = 2;        // Minimum octave for random melody note selection
int   arp_oct_max           = 3;        // Maximum octave for random melody note selection
int   arp_octave            = 4;        // Octave used for fast-clock arp note selection
float arp_mix_level         = 0.2f;     // Mix level of the filtered arp signal into the output

// --- Arp Filter ---
float arp_filter_freq_base  = 300.0f;   // Base cutoff frequency (Hz) for the arp lowpass filter
float arp_filter_freq_range = 100.0f;   // Random Hz added to arp_filter_freq_base during evolution
float arp_filter_res_max    = 0.5f;     // Maximum resonance of the arp filter during evolution

// --- Noise Source ---
float noise_volume          = 0.25f;    // Volume of the pink noise before bandpass filtering
float noise_filter_min      = 800.0f;   // Minimum cutoff (Hz) for the noise bandpass; also the base for the LFO sweep
float noise_filter_lfo_depth = 150.0f;  // LFO modulation depth (Hz) on the noise filter's upper cutoff bound

// --- Main Lowpass Filter Sweep ---
float filter_cutoff_min     = 500.0f;   // Lower bound (Hz) of the filterLFO cutoff sweep
float filter_cutoff_max     = 2000.0f;  // Upper bound (Hz) of the filterLFO cutoff sweep

// --- Output Highpass (rumble / DC-block) ---
float output_highpass_freq  = 50.0f;    // Cutoff (Hz) of the final highpass filter on the output

// --- Delay ---
float delay_time_min        = 11950.0f; // Minimum delay read position in samples during LFO wobble
float delay_time_max        = 12100.0f; // Maximum delay read position in samples during LFO wobble
float delay_feedback_min    = 0.6f;     // Minimum feedback gain during LFO modulation
float delay_feedback_max    = 0.9f;     // Maximum feedback gain during LFO modulation
float delay_dry_mix         = 0.75f;    // Level of the dry signal in the final delay mix
float delay_wet_mix         = 0.25f;    // Level of the delayed signal in the final delay mix

// --- LFO Rates ---
float filter_lfo_freq       = 0.025f;   // Initial rate (Hz) of the main filter sweep LFO
float filter_lfo_freq_min   = 0.0125f;  // Minimum rate (Hz) when the filterLFO is re-randomized
float filter_lfo_freq_range = 0.0250f;  // Random range added to filter_lfo_freq_min on re-randomize
float filter_lfo_lfo_freq   = 0.00125f; // Rate (Hz) of the meta-LFO that slowly shifts filterLFO's upper depth
float delay_lfo_freq        = 1.25f;    // Rate (Hz) of the delay time wobble LFO
float delay_fb_lfo_freq     = 0.025f;   // Rate (Hz) of the delay feedback modulation LFO

// --- Probability Thresholds (runtime-evolved values) ---
float tung_prob             = 0.8f;     // Current probability that a clock tick does NOT fire a melody note (higher = sparser)
float tung_prob_min         = 0.85f;    // Minimum tung_prob after a colour evolution event
float tung_prob_range       = 0.14f;    // Range added to tung_prob_min when re-randomizing
float arp_trigger_prob      = 0.75f;    // Current threshold: arp envelope fires if rng exceeds this value
float arp_trigger_prob_min  = 0.75f;    // Minimum arp_trigger_prob after evolution
float arp_trigger_prob_range = 0.24f;   // Range added to arp_trigger_prob_min when re-randomizing
float arp_clock_prob        = 0.5f;     // Current arp clock trigger probability (reserved for future gating logic)
float arp_clock_prob_min    = 0.33f;    // Minimum arp_clock_prob after evolution
float arp_clock_prob_range  = 0.66f;    // Range added to arp_clock_prob_min when re-randomizing
float fast_clock_note_prob  = 0.5f;     // Probability that the arp note updates on each fast clock tick
float colour_change_prob    = 0.9f;     // Probability threshold for triggering a colour/tempo evolution event

// --- Evolution Timing (in clock ticks) ---
int colour_change_ticks     = 128;      // Ticks between colour/tempo evolution checks
int filter_lfo_update_ticks = 32;       // Ticks between filterLFO rate re-randomizations
int arp_trigger_ticks       = 16;       // Ticks between arp envelope trigger checks

// --- Output Stage ---
float softclip_knee         = 1.2f;     // Softclip knee — higher allows more headroom before saturation
float output_scale          = 32000.0f; // Float-to-int16 scale factor (leaves a little headroom below 32767)

// --- Scene ---
int initial_colour          = 7;        // Colour/scale index loaded at startup (0-15)

// ============================================================
// DSP Objects & Runtime State
// ============================================================

PinkNoise pink_noise(69);

Oscillator osc;
Oscillator osc2;
Oscillator arp;

Envelope env;
Envelope arp_env;
TempoClock clock(tempo_bpm, 2);
TempoClock fast_clock(tempo_bpm, 4);

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
float melody_overtone_vol = 0.125f;

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
        uint8_t oct = rng.nextRange(arp_oct_min, arp_oct_max);
        float n = colour.getNote(oct, 4 - step);
        osc2.setFrequency(n * osc2_detune_ratio);
        osc.setFrequency(n);
        env.setRelease(env_release_min + rng.nextFloat() * env_release_range);
        env.reset();
        env.trigger(); // Percussive: auto-releases after attack
      }

      // change up colour
      if ((clock.getTick() % colour_change_ticks) == 0 && rng.nextFloat() > colour_change_prob) {
        colour.setColour(rng.nextRange(0, 15));
        // and tempo
        float new_tempo = tempo_min + (rng.nextFloat() * tempo_range);
        clock.setBPM(new_tempo);
        fast_clock.setBPM(new_tempo);
        melody_overtone_vol = rng.nextFloat() * melody_overtone_vol_max;
        arp_filter.setFrequency(arp_filter_freq_base + rng.nextFloat() * arp_filter_freq_range);
        arp_filter.setResonance(rng.nextFloat() * arp_filter_res_max);
        arp_trigger_prob = arp_trigger_prob_min + rng.nextFloat() * arp_trigger_prob_range;
        arp_clock_prob = arp_clock_prob_min + rng.nextFloat() * arp_clock_prob_range;
        tung_prob = tung_prob_min + rng.nextFloat() * tung_prob_range;
      }

      if ((clock.getTick() % filter_lfo_update_ticks) == 0) {
        filterLFO.setFrequency(filter_lfo_freq_min + rng.nextFloat() * filter_lfo_freq_range);
      }

      if ((clock.getTick() % arp_trigger_ticks == 0) && rng.nextFloat() > arp_trigger_prob) {
        arp_env.trigger();
      }
    }

    if (fast_clock.process()) {
      if (rng.nextFloat() > fast_clock_note_prob) {
        arp.setFrequency(colour.getNote(arp_octave, rng.nextRange(0, 4)));
      }
    }

    // this LFO is used a few places, process it now.
    float lfotime = filterLFO.process();


    // NOISE Oscillation
    float noise_sample = pink_noise.process() * noise_volume;
    float noise_lfo_max = noise_filter_min + (filterLFOLFO.process() * noise_filter_lfo_depth);
    noise_filter.setFrequency(mapf(lfotime, -1.0f, 1.0f, noise_filter_min, noise_lfo_max));
    noise_filter.process(noise_sample);
    noise_sample = noise_filter.bandpass();

    
    // Calculate the tung drum melody levels
    float tung_sample = (((osc.process() * osc1_gain) + (osc2.process() * melody_overtone_vol)) * env.process()) * melody_mix_level;

    // add arp
    arp_filter.process(arp.process());
    float arp_sample = (arp_filter.lowpass() * arp_env.process() * arp_mix_level);


    // apply last low pass filter
    float sample = noise_sample + tung_sample + arp_sample;
    float cutoff = mapf(lfotime, -1.0f, 1.0f, filter_cutoff_min, filter_cutoff_max);
    filter1.setFrequency(cutoff);
    filter1.process(sample);
    sample = filter1.lowpass();

    // apply delay
    float dlfo = delayLFO.process();
    float delay_fb = mapf(delayFeedbackLFO.process(), -1.0f, 1.0f, delay_feedback_min, delay_feedback_max);
    float delayed = delay1.read(mapf(dlfo, -1.0f, 1.0f, delay_time_min, delay_time_max));
    delay1.write(sample + delayed * delay_fb);

    float out = (sample * delay_dry_mix) + (delayed * delay_wet_mix);

    // apply a low pass filter to remove sub bass frequencies.
    output_highpass.process(out);
    out = output_highpass.highpass();

    int16_t s = (int16_t)(softclip(out, softclip_knee) * output_scale);
    buffer[i] = s;
    buffer[i + 1] = s;
  }
}

void setup() {
  osc.setWaveform(Waveform::SINE);
  osc.setAmplitude(1.0f);
  osc2.setWaveform(Waveform::SINE);

  arp.setWaveform(Waveform::SAW);
  arp_filter.setFrequency(arp_filter_freq_base);

  env.setAttack(env_attack);
  env.setRelease(env_release_init);
  arp_env.setAttack(arp_env_attack);
  arp_env.setRelease(arp_env_release);

  filter1.setFrequency(filter_cutoff_min);

  filterLFO.setWaveform(Waveform::SINE);
  filterLFO.setFrequency(filter_lfo_freq);
  filterLFO.setDivider(64);   // compute at ~689 Hz, imperceptible to audio

  filterLFOLFO.setWaveform(Waveform::SINE);
  filterLFOLFO.setFrequency(filter_lfo_lfo_freq);
  filterLFOLFO.setDivider(256);

  delayLFO.setWaveform(Waveform::SINE);
  delayLFO.setFrequency(delay_lfo_freq);
  delayLFO.setDivider(64);    // ~1378 updates/cycle, very smooth

  delayFeedbackLFO.setWaveform(Waveform::SINE);
  delayFeedbackLFO.setFrequency(delay_fb_lfo_freq);
  delayFeedbackLFO.setDivider(64);

  output_highpass.setFrequency(output_highpass_freq);

  colour.setColour(initial_colour);

  // seed the RNG with some noise.
  for (int i = 0; i < 10; i++) {
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
