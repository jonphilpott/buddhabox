#include "oscillator.h"
#include "envelope.h"

#pragma once

class TriggeredOsc {
 public:
  TriggeredOsc() {
    env_.setAttack(0.25f);
    env_.setRelease(1.0f);
    osc_.setWaveform(Waveform::SAW);
  }

  void setFrequency(float f) {
    osc_.setFrequency(f);
  }

  void trigger() {
    env_.gate(true);
    //env_.trigger();
    //env_.gate(false);
  }

  void released() {
    env_.gate(false);
  }
  
  float process() {
    return osc_.process() * env_.process();
  }

  Oscillator *getOscillator() {
    return &osc_;
  }

  Envelope *getEnvelope() {
    return &env_;
  }

  bool isActive() {
    return env_.isActive();
  }

 private:
  Oscillator osc_;
  Envelope   env_;
};
