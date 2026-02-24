/* @file color.h
 * @brief pentatonic scales generator
 *
 */

#include "midi_utils.h"
#include "random.h"

#pragma once

#define MELODY_LENGTH (16)

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
    uint8_t o = num / 5;

    return (root_ * offsets_[n]) * (1 << (o + octave));
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

  void newMelody(LFSR *rng, uint8_t rmax) {
    melody_[0] = 0;
    
    for (int i = 0; i < (MELODY_LENGTH-2); i++) {
      melody_[i+1] = rng->nextRange(0, rmax);
    }

    if (rng->next() > 0.5) {
      melody_[MELODY_LENGTH-1] = 0;
    }
    else {
      melody_[MELODY_LENGTH-1] = 3;
    }
  }

  uint8_t getMelody(uint8_t idx) {
    return melody_[idx % MELODY_LENGTH];
  }
  
 private:
  uint8_t colour_ = 0;
  float offsets_[5] = { JUST_OFFSETS[0], JUST_OFFSETS[1], JUST_OFFSETS[5], JUST_OFFSETS[7], JUST_OFFSETS[8] };
  // everything is in fez major
  float root_ = (NOTE_C2)+2;
  uint8_t melody_[MELODY_LENGTH];
};
