#include "dsp_common.h"
#include "envelope.h"
#include "oscillator.h"

#pragma once

class SawVoice {
public:
  SawVoice() {
    env_.setAttack(2.2f);
    env_.setRelease(4.0f);
    osc1_.setWaveform(Waveform::SINE);
    osc1_.setAmplitude(0.5f);
    osc2_.setWaveform(Waveform::SINE);
    osc2_.setAmplitude(0.25f);
    lfo_.setFrequency(4.0f);
    lfo_.setDivider(64);
    clip_ = 1.0f;
  }

  void setFrequency(float f) { f_ = f; }

  void trigger() { env_.trigger(); }

  float process() {
    float fm = 1.0f + lfo_.process() * 0.009f;
    float f = f_ * fm;
    osc1_.setFrequency(f);
    osc2_.setFrequency(f * 2.00405f);
    float s = softclip((osc1_.process() + osc2_.process()) * env_.process(), clip_);
    return (s * 0.8f);
  }

  void setClip(float c) { clip_ = c; }

  void setLFO(float f) { lfo_.setFrequency(f); }

  bool isActive() { return env_.isActive(); }

private:
  Oscillator osc1_;
  Oscillator osc2_;
  Envelope env_;
  SVFilter filter_;
  LFO lfo_;
  float f_;
  float clip_;
};
