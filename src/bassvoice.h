#include "dsp_common.h"
#include "oscillator.h"

#pragma once

class BassVoice {
public:
  BassVoice() { osc1_.setWaveform(Waveform::SINE); }

  void setFrequency(float f) { osc1_.setFrequency(f); }

  float process() {
    float s = softclip(osc1_.process(), 1.0f) * 2.0f;
    return s;
  }

private:
  Oscillator osc1_;
};
