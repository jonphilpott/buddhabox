
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

// global output scale
float output_scale = 32000.0f;

// global gain
float gain = 1.0f;

// where to set the saturation clip point
float softclip_knee = 1.0f;

//
// Objects
//

// Main RNG used everywhere.
LFSR rng;

// Main TempoClock. gives 8th notes.
TempoClock clock(80.0f, 2);

// useful define here for the clock for making bar-based calcs (assuming 4/4)
#define BARS (8)

// Colour, for melodic content.
Colour colour;

// Audio Generators
Tung tung;
Organ terry;
Tanpura tanpura;
SawVoice sawvoice;
BassVoice bass;
PinkNoise noise(69);
LFO noise_lfo;
SVFilter noise_filter;
Envelope master_env;

// LDR on PA0, used for day/night decisions and entropy source.
AnalogInput lightSensor(PA0, 0.03f);

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
bool play_bass = 0;
bool play_saw = 0;
bool play_tung = 0;
bool play_noise = 0;
bool play_terry = 0;
bool play_tanpura = 0;

int tung_mode = 0;
int tung_melody_idx = 0;
int saw_melody_idx = 0;

// probabilities for melodies
float tung_prob = 0.5f;
float saw_prob = 0.5f;

int terry_notes[] = {0, 2, 3, 5, 1};

float delay_samples = 18000.0f;

bool dice_rolls[6];

void audioCallback(int16_t *buffer, uint16_t length) {
  for (uint16_t i = 0; i < length; i += 2) {
    float mix = 0.0f;
    if (clock.process()) {
      int tick = clock.getTick();
      // order tick handlers fastest to slowest.

      terry.setFrequency(colour.getNote(3, terry_notes[tick % 5]));
      terry.trigger();

      if ((tick % 1) == 0) {
        if (rng.nextFloat() > tung_prob) {
          tung.setFrequency(
              colour.getNote(4, colour.getMelody(tung_melody_idx++)),
              tung_mode);
          tung.trigger();
        }
      }

      if ((tick % 4) == 0) {
        terry_notes[3] = rng.nextRange(0, 3);
        
        if (rng.nextFloat() > saw_prob) {
          int m = colour.getMelody(saw_melody_idx++);
          sawvoice.setFrequency(colour.getNote(3, m));
          sawvoice.trigger();
        }

        delay_lfo.setFrequency(0.2f + rng.nextFloat() * 1.2f);
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
              if (rng.nextFloat() > 0.33f && dr < 3) {
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

            bass_mode = rng.nextRange(0, 1);
            tung_mode = 0;

            tanpura.setFrequency(colour.getNote(2, 0));

            tung_prob = rng.nextFloat() * 0.25f;
            saw_prob = rng.nextFloat() * 0.33f;

            tung.setRelease(2.0f + rng.nextFloat() * 2.0f);

            sawvoice.setLFO(1.2f + rng.nextFloat() * 4.0f);
            sawvoice.setClip(0.5f + rng.nextFloat() * 1.2f);

            colour.setColour(rng.nextRange(0, 15));
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

            noise_lfo.setFrequency(0.01f + (rng.nextFloat() * 0.05f));
            noise_filter.setFrequency(3000.0f + (rng.nextFloat() * 2000.0f));

            float bpm = rng.nextRange(72, 88);
            float dotted_eighth_seconds = (60.0f / bpm) * 0.66f;
            delay_samples = (int)(dotted_eighth_seconds * 44100.0f);
            clock.setBPM(bpm);

            master_env.gate(true);
          } else {
            master_env.gate(false);
            state = 0;
          }

          target_ticks = tick + (8 * BARS);
        }
      }
    }

    if (play_bass) {
      mix += bass.process() * 0.215f;
    }

    if (play_tung) {
      mix += tung.process() * 0.215f;
    }

    if (play_saw) {
      mix += sawvoice.process() * 0.57f;
    }

    if (play_terry) {
      mix += terry.process() * 0.172f;
    }

    if (play_tanpura) {
      mix += tanpura.process() * 0.172f;
    }

    if (play_noise) {
      noise_filter.process(noise.process());
      mix += noise_filter.highpass() *
             mapf(noise_lfo.process(), -1.0f, 1.0f, 0.035f, 0.05f);
    }

    mix = mix * master_env.process();

    // output filter stage
    filter.setFrequency(5000.0f);
    filter.process(mix);

    mix = filter.lowpass();

    hpf.process(mix);
    mix = hpf.highpass();

    // delay
    float read_time_lfo = delay_lfo.process() * 5.0f;
    float delayed = delay_line.read(delay_samples + read_time_lfo);
    delay_filter.process(delayed);
    delayed = delay_filter.lowpass();
    mix = (mix * 0.5f) + (delayed * 0.5f);
    delay_line.write(mix);

    float out = mix * gain;
    int16_t s = (int16_t)(softclip(out, softclip_knee) * output_scale);
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
  colour.setRoot(NOTE_C1 + 2);
  colour.newMelody(&rng, 5);
  colour.setColour(5);

  delay_lfo.setFrequency(2.0f);
  hpf.setFrequency(80.0f);
  noise_lfo.setFrequency(0.02f);
  noise_filter.setFrequency(300.0f);
  delay_filter.setFrequency(800.0f);

  master_env.setAttack(15.0f);
  master_env.setRelease(15.0f);

  state = 0;
  AudioEngine::begin();
}

void loop() {
  delay(10);
  lightSensor.update();
}
