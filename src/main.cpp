
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
float tempo_bpm             = 72.0f;    // Starting BPM shared by both
                                        // clocks
float tempo_min             = 65.0f;    // Minimum BPM during random
                                        // tempo evolution
float tempo_range           = 18.0f;    // Random BPM range added to
                                        // tempo_min on each evolution
                                        // event

// --- Melody Oscillators ---
float osc2_detune_ratio     = 2.05f;    // Frequency multiplier
                                        // applied to osc2 (slightly
                                        // above an octave, creates
                                        // beating, for a tung drum
                                        // like sound)
float osc1_gain             = 0.75f;    // Gain of the primary melody
                                        // oscillator before the
                                        // envelope
float melody_mix_level      = 0.25f;    // Final attenuation of the
                                        // combined melody signal into
                                        // the stereo mix
float melody_overtone_vol_max = 0.0125f; // Maximum volume cap for the
                                         // osc2 overtone when
                                         // re-randomized
int   tung_oct_min           = 2;        // Minimum octave for random
                                         // melody note selection
int   tung_oct_max           = 3;        // Maximum octave for random
                                         // melody note selection


// --- Melody Envelope ---
float env_attack            = 0.15f;    // Attack time (s) for the
                                        // percussive melody envelope
float env_release_init      = 0.2f;     // Initial release time (s);
                                        // overridden on each trigger
float env_release_min       = 0.75f;    // Minimum release time (s)
                                        // when randomly chosen on
                                        // trigger
float env_release_range     = 1.5f;     // Random range (s) added to
                                        // env_release_min on each
                                        // trigger

// --- Arp Pad Envelope ---
float arp_env_attack        = 4.0f;     // Attack time (s) for the
                                        // slow arp pad swell
float arp_env_release       = 18.0f;    // Release time (s) for the
                                        // slow arp pad swell

// --- Arp ---
int arp_divider_range       = 2;        // range of clock divider for
                                        // arps.
int arp_divider_min         = 1;        // start of divider 2^N


// --- Arp Oscillator ---
int   arp_octave            = 5;        // Octave used for fast-clock
                                        // arp note selection
float arp_mix_level         = 0.015f;   // Mix level of the filtered
                                        // arp signal into the output


// --- Noise Source ---
float noise_mix_level        = 0.2f;    // Volume of the pink noise
                                        // before bandpass filtering
float noise_filter_min       = 400.0f;  // Minimum cutoff (Hz) for the
                                        // noise bandpass; also the
                                        // base for the LFO sweep
float noise_filter_lfo_depth = 50.0f;  // LFO modulation depth (Hz)
                                        // on the noise filter's upper
                                        // cutoff bound

// --- Main Lowpass Filter Sweep ---
float filter_cutoff_min     = 500.0f;   // Lower bound (Hz) of the
                                        // filterLFO cutoff sweep
float filter_cutoff_max     = 3000.0f;  // Upper bound (Hz) of the
                                        // filterLFO cutoff sweep

// --- Output Highpass (rumble / DC-block) ---
float output_highpass_freq  = 180.0f;   // Cutoff (Hz) of the final
                                        // highpass filter on the
                                        // output

// --- Delay ---
float delay_time_range      = 50.0f;
float delay_time_max        = 14100.0f; // Maximum delay read position
                                        // in samples during LFO
                                        // wobble
float delay_time_min        = delay_time_max - delay_time_range;

float delay_feedback_min    = 0.6f;     // Minimum feedback gain
                                        // during LFO modulation
float delay_feedback_max    = 0.9f;     // Maximum feedback gain
                                        // during LFO modulation
float delay_dry_mix         = 0.80f;    // Level of the dry signal in
                                        // the final delay mix
float delay_wet_mix         = 0.20f;    // Level of the delayed signal
                                        // in the final delay mix

// --- LFO Rates ---
float filter_lfo_freq       = 0.025f;   // Initial rate (Hz) of the
                                        // main filter sweep LFO
float filter_lfo_freq_min   = 0.0125f;  // Minimum rate (Hz) when the
                                        // filterLFO is re-randomized
float filter_lfo_freq_range = 0.0250f;  // Random range added to
                                        // filter_lfo_freq_min on
                                        // re-randomize
float filter_lfo_lfo_freq   = 0.00125f; // Rate (Hz) of the meta-LFO
                                        // that slowly shifts
                                        // filterLFO's upper depth
float delay_lfo_freq        = 1.75f;    // Rate (Hz) of the delay time
                                        // wobble LFO
float delay_fb_lfo_freq     = 0.025f;   // Rate (Hz) of the delay
                                        // feedback modulation LFO

// --- Probability Thresholds (runtime-evolved values) ---
float tung_prob             = 0.82f;    // Current probability that a
                                        // clock tick does NOT fire a
                                        // melody note (higher =
                                        // sparser)
float tung_prob_min         = 0.82f;    // Minimum tung_prob after a
                                        // colour evolution event
float tung_prob_range       = 0.14f;    // Range added to
                                        // tung_prob_min when
                                        // re-randomizing
float arp_trigger_prob      = 0.75f;    // Current threshold: arp
                                        // envelope fires if rng
                                        // exceeds this value
float arp_trigger_prob_min  = 0.75f;    // Minimum arp_trigger_prob
                                        // after evolution
float arp_trigger_prob_range = 0.24f;   // Range added to
                                        // arp_trigger_prob_min when
                                        // re-randomizing
float arp_clock_prob        = 0.5f;     // Current arp clock trigger
                                        // probability (reserved for
                                        // future gating logic)
float arp_clock_prob_min    = 0.33f;    // Minimum arp_clock_prob
                                        // after evolution
float arp_clock_prob_range  = 0.66f;    // Range added to
                                        // arp_clock_prob_min when
                                        // re-randomizing
float fast_clock_note_prob  = 0.75f;    // Probability that the arp
                                        // note updates on each fast
                                        // clock tick
float colour_change_prob    = 0.7f;     // Probability threshold for
                                        // triggering a colour/tempo
                                        // evolution event

// --- Evolution Timing (in clock ticks) ---
int colour_change_ticks     = 128;      // Ticks between colour/tempo
                                        // evolution checks
int filter_lfo_update_ticks = 32;       // Ticks between filterLFO
                                        // rate re-randomizations
int arp_trigger_ticks       = 16;       // Ticks between arp envelope
                                        // trigger checks

// --- Output Stage ---
float output_gain           = 1.0f;    // Master output gain — scales
                                       // the summed mix before the
                                       // final limiter.  Set to 0.65
                                       // for 3W speakers (limits to
                                       // ~1.3x of base gains); raise
                                       // to 1.0 when 5W speakers are
                                       // installed.
float softclip_knee         = 1.2f;     // Softclip knee — higher
                                        // allows more headroom before
                                        // saturation
float output_scale          = 32000.0f; // Float-to-int16 scale factor
                                        // (leaves a little headroom
                                        // below 32767)

// -- Rain ---
float rain_max_vol          = 0.005f;
float rain_vol              = rain_max_vol;
float rain_max_freq         = 0.04f;


// --- Scene ---
int initial_colour          = 7;       // Colour/scale index loaded
                                       // at startup (0-15)
int colour_melody_range     = 6;       // how many notes a melody can
                                       // range;



// ============================================================
// DSP Objects & Runtime State
// ============================================================

PinkNoise pink_noise(69);
PinkNoise rain(42);

Oscillator osc;
Oscillator osc2;
Oscillator arp;

Envelope env;
Envelope arp_env;
TempoClock clock(tempo_bpm, 2);
TempoClock fast_clock(tempo_bpm, 8);

LFSR rng;

SVFilter filter1;
SVFilter noise_filter;
SVFilter output_highpass;

DelayLine<20000> delay1;

LFO filterLFO;
LFO delayLFO;
LFO delayFeedbackLFO;
LFO filterLFOLFO;
LFO rainLFO;

Colour colour;

uint8_t bass_tracker = 0;
uint8_t bass_note = 0;

float arp_sample = 0;
float melody_overtone_vol = 0.125f;

// noise source
AnalogInput lightSensor(PA0, 0.03f);


void mixItUp() {
  // pick a new scale.
  // TODO: a way to map the light sensor to scale choices, can we order the scales from bright to dark sounding?

  float ldrVal = lightSensor.readLDR();

  int mColourRangeStart = ldrVal > 0.5 ? 7 : 0;
  int mColourRangeEnd = mColourRangeStart + 8;

  int mColourMelodyRange = ldrVal > 0.5 ? colour_melody_range : 4;
  
  colour.setColour(rng.nextRange(mColourRangeStart, mColourRangeEnd));
  colour.newMelody(&rng, colour_melody_range);
  
  // pick a new tempo
  float mTempoRange = (0.5 + ldrVal * 0.5) * tempo_range;
  float new_tempo = tempo_min + (rng.nextFloat() * mTempoRange);
  clock.setBPM(new_tempo);
  fast_clock.setBPM(new_tempo);
  
  // adjust the volume of the odd harmonic on the tung drum
  melody_overtone_vol = rng.nextFloat() * (melody_overtone_vol_max * ldrVal);
  
  // adjust the timing on the tung drum
  tung_prob = tung_prob_min + rng.nextFloat() * tung_prob_range;
  
  
  // adjust parameters for the fleeting arpeggio
  arp_trigger_prob = arp_trigger_prob_min + rng.nextFloat() * arp_trigger_prob_range;
  arp_clock_prob = arp_clock_prob_min + rng.nextFloat() * arp_clock_prob_range;
  
  fast_clock.setDivision(1 << (arp_divider_min + rng.nextRange(0, arp_divider_range)));

  // adjust the rainLFO
  rainLFO.setFrequency(0.001f + rng.nextFloat() * rain_max_freq);
  rain_vol = rng.nextFloat() * (rain_max_vol * ldrVal);
}

void audioCallback(int16_t *buffer, uint16_t length) {
  for (uint16_t i = 0; i < length; i += 2) {

    // On each clock tick, conduct some evolutions.
    if (clock.process()) {

      // maybe flip a bit in the LFSR to mix things up
      rng.add_entropy(lightSensor.readReallyRaw());

      // do we strike the tung?
      if (rng.nextFloat() > tung_prob) {
        int step = clock.getTick() % 16;

        float n = colour.getNote(tung_oct_min, colour.getMelody(step));

        // tune the second oscillator
        osc2.setFrequency(n * osc2_detune_ratio);
        osc.setFrequency(n);
        env.setRelease(env_release_min + rng.nextFloat() * env_release_range);
        env.reset();
        env.trigger(); 
      }

      // change up colour
      if ((clock.getTick() % colour_change_ticks) == 0 && rng.nextFloat() > colour_change_prob) {
        mixItUp();
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
    float noise_sample = pink_noise.process() * noise_mix_level;
    float noise_lfo_max = noise_filter_min + (filterLFOLFO.process() * noise_filter_lfo_depth);
    noise_filter.setFrequency(mapf(lfotime, -1.0f, 1.0f, noise_filter_min, noise_lfo_max));
    noise_filter.process(noise_sample);
    noise_sample = noise_filter.bandpass();

    // RAIN
    // Layer 2: High patter — filter swept by LFO
    float rain_sample = rain.process() * rainLFO.process() * rain_max_vol;

    // Calculate the tung drum melody levels
    float tung_sample = (((osc.process() * osc1_gain) + (osc2.process() * melody_overtone_vol)) * env.process()) * melody_mix_level;

    float arp_sample = (arp.process() * arp_env.process() * arp_mix_level);

    // mix tung and arpeggio
    float sample = (tung_sample + arp_sample);

    // apply delay to the melodic elements
    float dlfo = delayLFO.process();
    float delay_fb = mapf(delayFeedbackLFO.process(), -1.0f, 1.0f, delay_feedback_min, delay_feedback_max);
    float delayed = delay1.read(mapf(dlfo, -1.0f, 1.0f, delay_time_min, delay_time_max));

    delay1.write(sample + delayed * delay_fb);

    // now add the noise, the noise into the delay isnt fun.
    sample = (sample + noise_sample + rain_sample) * output_gain;

    // apply the cut off
    float cutoff = mapf(lfotime, -1.0f, 1.0f, filter_cutoff_min, filter_cutoff_max);
    cutoff = cutoff * (0.25 + lightSensor.readLDR());
    filter1.setFrequency(cutoff);
    filter1.process(sample);
    sample = filter1.lowpass();

    float out = (sample * delay_dry_mix) + (delayed * delay_wet_mix);
    //out += rain_sample;

    // apply a low pass filter to remove sub bass frequencies.
    output_highpass.process(out);
    out = output_highpass.highpass();

    int16_t s = (int16_t)(softclip(out, softclip_knee) * output_scale);
    buffer[i] = s;
    buffer[i + 1] = s;
  }
}

void setup() {
  Serial.begin(115200);
  osc.setWaveform(Waveform::SINE);
  osc.setAmplitude(1.0f);
  osc2.setWaveform(Waveform::SINE);

  arp.setWaveform(Waveform::SINE);

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

  rainLFO.setWaveform(Waveform::TRIANGLE);
  rainLFO.setDivider(256); 

  // seed the RNG with some noise, after 32 passes of this the LFSR
  // should be seeded with a pretty random value based on ADC noise
  for (int i = 0; i < 32; i++) {
    lightSensor.update();
    rng.add_entropy(lightSensor.readReallyRaw());
    rng.next();
    delay(5);
  }

  // now the RNG is seeded, set the initial parameters.
  mixItUp();
  
  AudioEngine::begin();
}

void loop() {
  delay(10);
  lightSensor.update();
}
