#include "envelope.h"
#include "lfo.h"
#include "oscillator.h"

#pragma once

class Organ {
public:
  Organ() {
    env_.setAttack(0.25f);
    env_.setRelease(2.0f);

    osc_[0].setWaveform(Waveform::SINE);
    osc_[0].setAmplitude(1.0f);
    osc_[1].setWaveform(Waveform::SINE);
    osc_[1].setAmplitude(0.125f);

    lfo_.setFrequency(4.0f);
    lfo_.setDivider(32);
  }

  void setFrequency(float f) {
    osc_[0].setFrequency(f);
    osc_[1].setFrequency(f * 2.0f);
  }

  void trigger() { env_.trigger(); }

  float process() {
    float s = 0.0f;

    // Sum only the two initialised oscillators (fundamental + 2nd harmonic)
    for (int i = 0; i < 2; i++) {
      s += osc_[i].process();
    }

    // Scale down to prevent clipping when harmonics stack
    s *= 0.5f;

    // Convert LFO from bipolar (-1..+1) to unipolar (0..1) for tremolo.
    // A bipolar multiply would flip signal polarity at every zero crossing,
    // creating a click/thump twice per LFO cycle. Unipolar keeps the output
    // smoothly modulated between silence and full amplitude.
    float trem = lfo_.process() * 0.5f + 0.5f;

    return s * trem * env_.process();
  }

  void setRelease(float r) { env_.setRelease(r); }

  bool isActive() { return env_.isActive(); }

private:
  Oscillator osc_[2];
  Envelope env_;
  LFO lfo_;
};
