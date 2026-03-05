#include "dsp_common.h"
#include "envelope.h"
#include "oscillator.h"

#pragma once

#define PHASE_OFFSET (0.167f)

class Tanpura {
public:
  Tanpura() {
    for (int i = 0; i < 4; i++) {
      osc_[i].setWaveform(Waveform::TRIANGLE);
      lfo_[i].setFrequency(0.01f);
    }

    // 6 beat tanpura pattern mapped to LFO phases
    lfo_[0].setPhase(0);
    lfo_[1].setPhase(2 * PHASE_OFFSET);
    lfo_[2].setPhase(3 * PHASE_OFFSET);
    lfo_[3].setPhase(5 * PHASE_OFFSET);

    clip_ = 1.0f;
  }

  void setFrequency(float f) {
    f_ = f;
    osc_[0].setFrequency(f * 1.5f);
    osc_[1].setFrequency(f * 2.0f);
    osc_[2].setFrequency(f * 2.0f);
    osc_[3].setFrequency(f);

    // maybe?
    filter_.setFrequency(f * 3.0f);
  }

  float process() {
    float m = 0.0f;

    for (int i = 0; i < 4; i++) {
      m += osc_[i].process() * lfo_[i].process();
    }

    filter_.process(m);
    m = filter_.lowpass() * 0.333f;

    float s = softclip(m, clip_);
    return (s * 2.0f);
  }

  void setClip(float c) { clip_ = c; }

  void setLFO(float f) {
    for (int i = 0; i < 4; i++) {
      lfo_[i].setFrequency(f);
    }
  }

private:
  Oscillator osc_[4];
  SVFilter filter_;
  LFO lfo_[4];
  float f_;
  float clip_;
};
