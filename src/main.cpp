
#include "analog_input.h"
#include "audio_engine.h"
#include "bassvoice.h"
#include "clock.h"
#include "colour.h"
#include "delay.h"
#include "envelope.h"
#include "filter.h"
#include "lfo.h"
#include "midi_utils.h"
#include "noise.h"
#include "organ.h"
#include "oscillator.h"
#include "random.h"
#include "sawvoice.h"
#include "tanpura.h"
#include "tung.h"

// ============================================================
// Patch Parameters
// These globals shape every aspect of the sound. They are
// intentionally non-const so they can be mutated at runtime
// to let the patch evolve over time.
// ============================================================

// Ticks per bar — with a 4/4 clock at 8th-note resolution, one bar = 8 ticks
#define BARS (8)

// ── Output stage ─────────────────────────────────────────────

// Scales the final float sample into the 16-bit integer range
float output_scale = 32000.0f;

// Master volume multiplier applied just before output conversion
float gain = 0.5f;

// Soft saturation threshold; signals above this get gently compressed
float softclip_knee = 1.0f;

// Final lowpass cutoff to roll off harshness in the top frequencies (Hz)
float output_lpf_freq = 5000.0f;

// ── Mix gains ─────────────────────────────────────────────────

// How much each voice contributes to the mix (0 = silent, 1 = full scale)
float gain_bass    = 0.115f;
float gain_tung    = 0.215f;
float gain_saw     = 0.57f;
float gain_terry   = 0.172f;
float gain_tanpura = 0.172f;

// Noise level is LFO-modulated slowly between these two values
float gain_noise_min = 0.05f;
float gain_noise_max = 0.08f;

// ── Delay / echo ──────────────────────────────────────────────

// Wet/dry balance into the feedback delay; 0.5 = equal dry and wet
float delay_wet_dry = 0.5f;

// How far the LFO sweeps the delay read position in samples (chorus shimmer)
float delay_lfo_depth = 5.0f;

// Initial LFO rate for delay time modulation at startup (Hz)
float delay_lfo_init_freq = 2.0f;

// Lowpass on the delay return to darken the repeats over time (Hz)
float delay_filter_freq = 1800.0f;

// Per-movement delay LFO rate range (Hz) — randomised at each new section
float delay_lfo_rate_min = 0.2f;
float delay_lfo_rate_max = 1.4f;

// Fraction of the beat period used as the echo delay time (~2/3 = swung feel)
float delay_time_ratio = 0.66f;

// ── Noise ─────────────────────────────────────────────────────

// Initial noise LFO rate — how fast the filtered noise breathing effect moves
float noise_lfo_init_freq = 0.02f;

// Initial noise highpass cutoff frequency at startup (Hz)
float noise_filter_init_freq = 300.0f;

// Per-movement noise LFO rate range (Hz) — randomised each section
float noise_lfo_rate_min = 0.01f;
float noise_lfo_rate_max = 0.06f;

// Per-movement noise filter cutoff range (Hz) — selects the noise colour
float noise_filter_min = 500.0f;
float noise_filter_max = 1000.0f;

// Seed for the pink noise generator
int noise_seed = 69;

// ── Master envelope ───────────────────────────────────────────

// Fade-in duration when a new movement begins (seconds)
float master_attack_time = 15.0f;

// Fade-out duration when a movement ends (seconds)
float master_release_time = 15.0f;

// ── Filters ───────────────────────────────────────────────────

// High-pass cutoff to remove subsonic rumble from the output (Hz)
float hpf_freq = 80.0f;

// ── Musical setup ─────────────────────────────────────────────

// Root note for the piece (D1 = NOTE_C1 + 2 semitones)
int root_note = NOTE_C1 + 2;

// Initial scale/mode selection (colour index)
int initial_colour = 5;

// Initial melody pattern seed
int initial_melody = 5;

// Initial BPM before the first random tempo is chosen
float initial_bpm = 80.0f;

// Smoothing factor for light sensor ADC readings (0 = no smoothing, 1 = frozen)
float light_sensor_smoothing = 0.03f;

// ── BPM ───────────────────────────────────────────────────────

// Tempo is randomly chosen within this range at the start of each movement
float bpm_min = 72.0f;
float bpm_max = 88.0f;

// ── Movement structure ────────────────────────────────────────

// How long each movement plays before the patch considers a change (ticks)
int movement_length_ticks = 16 * BARS;

// gap between movements
int movement_gap_ticks = 4 * BARS;

// Maximum number of voices allowed to play simultaneously in a movement
int max_active_voices = 3;

// Probability threshold for including a voice — higher = fewer voices selected
float voice_select_threshold = 0.33f;

// ── Per-movement voice parameters ────────────────────────────

// Tung release envelope range (seconds) — randomised each movement
float tung_release_min = 2.0f;
float tung_release_max = 4.0f;

// Upper bound on tung trigger probability threshold; lower = tung fires more
float tung_prob_max = 0.1f;

// Upper bound on saw trigger probability threshold; lower = saw fires more
float saw_prob_max = 0.25f;


// Saw voice LFO (vibrato/chorus) rate range (Hz)
float saw_lfo_min = 1.2f;
float saw_lfo_max = 4.2f;

// Saw softclip drive range; higher values add more harmonic saturation
float saw_clip_min = 0.5f;
float saw_clip_max = 1.0f;


//
// Objects
//

// Main RNG used everywhere.
LFSR rng;

// Main TempoClock. gives 8th notes.
TempoClock clock(initial_bpm, 2);

// Colour, for melodic content.
Colour colour;

// Audio Generators
Tung tung;
Organ terry;
Tanpura tanpura;
SawVoice sawvoice;
BassVoice bass;
PinkNoise noise(noise_seed);
LFO noise_lfo;
SVFilter noise_filter;
Envelope master_env;

// LDR on PA0, used for day/night decisions and entropy source.
AnalogInput lightSensor(PA0, light_sensor_smoothing);

// output stage
SVFilter filter;
SVFilter hpf;
SVFilter delay_filter;
DelayLine<20000> delay_line;
LFO delay_lfo;

// melody / state
int bass_note = 0;

// 0 = drone, 1 = descending
int bass_mode = 0;
int bass_line[] = {5, 3, 2, 0};
int bass_line_idx = 0;

int tung_melody_range = 2;

// 0 = not playing
// 1 = playing
int state = 0;

// when we're going to change again.
int target_ticks = 0;

// flags for voices we're going to play in each movement
bool play_bass    = 0;
bool play_saw     = 0;
bool play_tung    = 0;
bool play_noise   = 0;
bool play_terry   = 0;
bool play_tanpura = 0;

int tung_mode       = 0;
int tung_melody_idx = 0;
int saw_melody_idx  = 0;

// probabilities for melodies
float tung_prob = 0.5f;
float saw_prob  = 0.5f;

int terry_notes[] = {0, 2, 3, 5, 1};

float delay_samples = 18000.0f;

bool dice_rolls[6];

void audioCallback(int16_t *buffer, uint16_t length) {
  for (uint16_t i = 0; i < length; i += 2) {
    float mix = 0.0f;
    if (clock.process()) {
      int tick = clock.getTick();
      // order tick handlers fastest to slowest.

      if (rng.nextFloat() < 0.75f) {
        terry.setFrequency(colour.getNote(3, terry_notes[tick % 5]));
        terry.trigger();
      }

      if ((tick % 1) == 0) {
        if (rng.nextFloat() < tung_prob) {
          tung.setFrequency(
              colour.getNote(3, colour.getMelody(tung_melody_idx++)));
          tung.trigger();
        }
      }

      if ((tick % 4) == 0) {
        terry_notes[3] = rng.nextRange(0, 3);

        if (rng.nextFloat() < saw_prob && !sawvoice.isActive()) {
          int m = colour.getMelody(saw_melody_idx++);
          sawvoice.setFrequency(colour.getNote(3, m));
          sawvoice.trigger();
        }

        delay_lfo.setFrequency(
            delay_lfo_rate_min +
            rng.nextFloat() * (delay_lfo_rate_max - delay_lfo_rate_min));
      }

      if ((tick % (1 * BARS)) == 0) {
        if (bass_mode == 0) {
          bass_line_idx++;
          bass_note = bass_line[bass_line_idx % 4];
        } else {
          bass_note = 0;
        }
      }

      bass.setFrequency(colour.getNote(1, bass_note));

      if ((tick % (4 * BARS)) == 0) {
        // add some entropy to the RNG to keep the random, random.
        rng.add_entropy(lightSensor.readReallyRaw());
        tung_melody_range = rng.nextRange(0, 5);

        colour.newMelody(&rng, rng.nextRange(0, 8));

        // if we're not playing, start playing, else...
        if (tick > target_ticks) {
          if (state == 0) {
            state = 1;

            int dr = 0;
            for (int i = 0; i < 6; i++) {
              if (rng.nextFloat() > voice_select_threshold &&
                  dr < max_active_voices) {
                dice_rolls[i] = true;
                dr++;
              } else {
                dice_rolls[i] = false;
              }
            }

            dr = 0;

            play_saw     = dice_rolls[dr++];
            play_tung    = dice_rolls[dr++];
            play_noise   = dice_rolls[dr++];
            play_terry   = dice_rolls[dr++];
            play_tanpura = dice_rolls[dr++];
            play_bass    = dice_rolls[dr++];

            bass_mode = 1;
            tung_mode = 0;

            tanpura.setFrequency(colour.getNote(3, 0));

            tung_prob = rng.nextFloat() * tung_prob_max;
            saw_prob  = rng.nextFloat() * saw_prob_max;

            tung.setRelease(
                tung_release_min +
                rng.nextFloat() * (tung_release_max - tung_release_min));

            sawvoice.setLFO(
                saw_lfo_min +
                rng.nextFloat() * (saw_lfo_max - saw_lfo_min));
            sawvoice.setClip(
                saw_clip_min +
                rng.nextFloat() * (saw_clip_max - saw_clip_min));

            colour.setColour(
                             COLOUR_BY_BRIGHTNESS[rng.nextRange(8, 15)]
                             );
            colour.newMelody(&rng, rng.nextRange(0, 8));

            bass_line[0] = rng.nextRange(0, 1) * 5;
            bass_line[1] = 2 + rng.nextRange(0, 1);
            bass_line[2] = rng.nextRange(0, 3);
            bass_line[3] = 0;

            terry_notes[0] = rng.nextRange(0, 1) * 5;
            terry_notes[1] = 2 + rng.nextRange(0, 1);
            terry_notes[2] = rng.nextRange(0, 3);
            terry_notes[3] = rng.nextRange(0, 2);
            terry_notes[4] = 1;

            noise_lfo.setFrequency(
                noise_lfo_rate_min +
                rng.nextFloat() * (noise_lfo_rate_max - noise_lfo_rate_min));
            noise_filter.setFrequency(
                noise_filter_min +
                rng.nextFloat() * (noise_filter_max - noise_filter_min));

            tanpura.setLFO(rng.nextFloat() * 0.5f);

            float bpm = rng.nextRange(bpm_min, bpm_max);
            float beat_seconds = 60.0f / bpm;
            delay_samples = (int)(beat_seconds * delay_time_ratio * SAMPLE_RATE);
            clock.setBPM(bpm);

            master_env.gate(true);
            target_ticks = tick + ((8 * rng.nextRange(2, 10)) * BARS);
          } else {
            master_env.gate(false);
            state = 0;
            target_ticks = tick + ((8 * rng.nextRange(2, 10)) * BARS);
          }
        }
      }
    }

    if (play_bass) {
      mix += bass.process() * gain_bass;
    }

    if (play_tung) {
      mix += tung.process() * gain_tung;
    }

    if (play_saw) {
      mix += sawvoice.process() * gain_saw;
    }

    if (play_terry) {
      mix += terry.process() * gain_terry;
    }

    if (play_tanpura) {
      mix += tanpura.process() * gain_tanpura;
    }

    if (play_noise) {
      noise_filter.process(noise.process());
      mix += noise_filter.lowpass() *
             mapf(noise_lfo.process(), -1.0f, 1.0f,
                  gain_noise_min, gain_noise_max);
    }

    mix = mix * master_env.process();

    // output filter stage
    filter.setFrequency(output_lpf_freq);
    filter.process(mix);

    mix = filter.lowpass();

    //hpf.process(mix);
    //mix = hpf.highpass();

    // delay
    float read_time_lfo = delay_lfo.process() * delay_lfo_depth;
    float delayed = delay_line.read(delay_samples + read_time_lfo);
    delay_filter.process(delayed);
    delayed = delay_filter.lowpass();
    mix = (mix * delay_wet_dry) + (delayed * delay_wet_dry);
    delay_line.write(mix);

    float out = mix * gain;
    // Hard clip for output protection. softclip()'s second arg is a drive
    // multiplier, not a threshold — softclip(x, 1.0) saturates every sample,
    // not just peaks. Intentional saturation lives in the voice modules.
    int16_t s = (int16_t)(clampf(out, -1.0f, 1.0f) * output_scale);
    buffer[i] = s;
    buffer[i + 1] = s;
  }
}

void setup() {
  Serial.begin(115200);

  // seed the RNG with some noise, after 32 passes of this the LFSR
  // should be seeded with a pretty random value based on ADC noise
  for (int i = 0; i < 32; i++) {
    lightSensor.update();
    rng.add_entropy(lightSensor.readReallyRaw());
    rng.next();
    delay(5);
  }

  // the universe is tuned to D
  colour.setRoot(root_note);
  colour.newMelody(&rng, initial_melody);
  colour.setColour(initial_colour);

  delay_lfo.setFrequency(delay_lfo_init_freq);
  hpf.setFrequency(hpf_freq);
  noise_lfo.setFrequency(noise_lfo_init_freq);
  noise_filter.setFrequency(noise_filter_init_freq);
  delay_filter.setFrequency(delay_filter_freq);

  master_env.setAttack(master_attack_time);
  master_env.setRelease(master_release_time);

  noise_lfo.setDivider(64);
  delay_lfo.setDivider(64);
  
  state = 0;
  AudioEngine::begin();
}

void loop() {
  delay(10);
  lightSensor.update();
}
