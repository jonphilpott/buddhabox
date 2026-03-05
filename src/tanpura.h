#include "dsp_common.h"
#include "envelope.h"
#include "oscillator.h"

#pragma once

#define PHASE_OFFSET (0.167f)

class Tanpura {
public:
  Tanpura() {
    for (int i = 0; i < 3; i++) {
      osc_[i].setWaveform(Waveform::SAW);
      lfo_[i].setFrequency(0.05f);
      lfo_[i].setDivider(64);
    }

    // 6 beat tanpura pattern mapped to LFO phases
    lfo_[0].setPhase(0);
    lfo_[1].setPhase(2 * PHASE_OFFSET);
    lfo_[2].setPhase(4 * PHASE_OFFSET);

    clip_ = 1.0f;
  }

  void setFrequency(float f) {
    f_ = f;
    osc_[0].setFrequency(f * 1.5f);
    osc_[1].setFrequency(f * 2.0f);
    osc_[2].setFrequency(f);

    // maybe?
    filter_.setFrequency(400.0f);
  }

  float process() {
    float m = 0.0f;

    for (int i = 0; i < 3; i++) {
      // Convert LFO from bipolar (-1..+1) to unipolar (0..1) so each
      // string swells from silence to full amplitude without a polarity
      // flip at the zero crossing (which would cause an audible click).
      float trem = lfo_[i].process() * 0.5f + 0.5f;
      m += osc_[i].process() * trem;
    }

    m += noise_.process() * 0.125f;
    
    filter_.process(m);
    m = filter_.lowpass() * 0.333f;

    float s = softclip(m, clip_);
    return (s * 2.0f);
  }

  void setClip(float c) { clip_ = c; }

  void setLFO(float f) {
    for (int i = 0; i < 3; i++) {
      lfo_[i].setFrequency(f);
    }
  }

private:
  Oscillator osc_[3];
  SVFilter filter_;
  PinkNoise noise_;
  LFO lfo_[3];
  float f_;
  float clip_;
};
