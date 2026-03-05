#include "envelope.h"
#include "lfo.h"
#include "oscillator.h"

#pragma once

class Organ {
public:
  Organ() {
    env_.setAttack(0.01f);
    env_.setRelease(4.0f);

    osc_[0].setWaveform(Waveform::SINE);
    osc_[0].setAmplitude(1.0f);
    osc_[1].setWaveform(Waveform::SINE);
    osc_[1].setAmplitude(1.0f);
    osc_[1].setWaveform(Waveform::SINE);
    osc_[1].setAmplitude(0.25f);

    lfo_.setFrequency(2.0f);
  }

  void setFrequency(float f) {
    osc_[0].setFrequency(f * 0.5f);
    osc_[1].setFrequency(f);
    osc_[2].setFrequency(f * 4.0f);
  }

  void trigger() { env_.trigger(); }

  float process() {
    float s = 0.0f;

    for (int i = 0; i < 3; i++) {
      s += osc_[i].process();
    }

    s = s * 0.444f;

    return (s * lfo_.process() * env_.process());
  }

  void setRelease(float r) { env_.setRelease(r); }

  bool isActive() { return env_.isActive(); }

private:
  Oscillator osc_[3];
  Envelope env_;
  LFO lfo_;
};
