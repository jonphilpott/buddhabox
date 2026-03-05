#include "envelope.h"
#include "lfo.h"
#include "oscillator.h"

#pragma once

static float offsets[] = {2.5f, 2.0f, 3.0f, 1.5f};

class Tung {
public:
  Tung() {
    env_.setAttack(0.125f);
    env_.setRelease(4.0f);
    osc1_.setWaveform(Waveform::SINE);
    osc1_.setAmplitude(0.667f);
    osc2_.setWaveform(Waveform::SINE);
    osc2_.setAmplitude(0.333f);
    lfo_.setFrequency(2.0f);
  }

  void setFrequency(float f, int mode) {
    osc1_.setFrequency(f);

    float o = offsets[mode % 4];

    osc2_.setFrequency(f * o);
  }

  void trigger() { env_.trigger(); }

  float process() {
    return (osc1_.process() + osc2_.process()) * lfo_.process() *
           env_.process();
  }

  void setRelease(float r) { env_.setRelease(r); }

  bool isActive() { return env_.isActive(); }

private:
  Oscillator osc1_;
  Oscillator osc2_;
  Envelope env_;
  LFO lfo_;
};
