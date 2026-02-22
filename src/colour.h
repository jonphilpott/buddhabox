/* @file color.h
 * @brief pentatonic scales generator
 *
 */

#include "midi_utils.h"

#pragma once

float JUST_OFFSETS[] = {
  1.0f,
  1.067f,
  1.125f,
  1.2f,
  1.25f,
  1.33f,
  1.406f,
  1.5f,
  1.6f,
  1.667f,
  1.8f,
  1.875f,
  2.0f
};
  

class Colour {
 public:
  float getNote(uint8_t octave, uint8_t num) {
    uint8_t n = num % 5;

    return (root_ * offsets_[n]) * (1 << octave);
  }

  void setColour(uint8_t colour) {
    uint8_t c = colour % 16;

    uint8_t bottom = c / 4;
    uint8_t top    = c % 4;

    offsets_[0] = JUST_OFFSETS[0];         // SA
    offsets_[1] = JUST_OFFSETS[bottom+1];  // note pick
    offsets_[2] = JUST_OFFSETS[5];         // ma
    offsets_[3] = JUST_OFFSETS[7];         // PA
    offsets_[4] = JUST_OFFSETS[top+7];     // second note pick;
    colour_ = c;
  }

  void setRoot(float r) {
    root_ = r;
  }
  
 private:
  uint8_t colour_ = 0;
  float offsets_[5] = { JUST_OFFSETS[0], JUST_OFFSETS[1], JUST_OFFSETS[5], JUST_OFFSETS[7], JUST_OFFSETS[8] };
  float root_ = (NOTE_C2) + 2;
};
