#include "envelope.h"
#include "lfo.h"
#include "oscillator.h"

#pragma once

class Tung {
public:
  Tung() {
    env_.setAttack(0.25f);
    env_.setRelease(8.0f);
    osc1_.setWaveform(Waveform::TRIANGLE);
    osc1_.setAmplitude(0.667f);
    osc2_.setWaveform(Waveform::TRIANGLE);
    osc2_.setAmplitude(0.133f);
    lfo_.setFrequency(1.2f);
    lfo_.setDivider(64);
  }

  void setFrequency(float f) {
    osc1_.setFrequency(f);
    osc2_.setFrequency(f * 3.01f);
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
