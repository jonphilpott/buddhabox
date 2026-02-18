/**
 * @file lfo.h
 * @brief Low Frequency Oscillator for modulating audio parameters over time.
 *
 * An LFO is structurally identical to a regular oscillator, but operates at
 * sub-audio frequencies (typically 0.01 Hz to 20 Hz). Instead of producing
 * sound directly, its output is used to modulate other parameters:
 *
 *   - Filter cutoff: makes the tone "breathe" or "sweep"
 *   - Amplitude: creates tremolo (volume wobble)
 *   - Pitch: creates vibrato (pitch wobble)
 *   - Delay time: creates chorus/flanger effects
 *   - Mix levels: slowly crossfade between textures
 *
 * WHY A SEPARATE CLASS FROM OSCILLATOR?
 * ─────────────────────────────────────
 * While the math is the same, the LFO class differs in intent:
 *   - No amplitude parameter — always outputs full -1 to +1 range.
 *     The user scales the output as needed for the target parameter.
 *   - Includes reset() for syncing to tempo clocks
 *   - Default frequency is 1 Hz (not 440 Hz)
 *
 * Keeping them separate also makes code more readable: when you see `LFO`
 * you immediately know it's a modulation source, not an audio source.
 *
 * Example usage:
 *   LFO filterSweep;
 *   filterSweep.setWaveform(Waveform::TRIANGLE);
 *   filterSweep.setFrequency(0.1f);  // One cycle every 10 seconds
 *
 *   // In audio callback:
 *   float mod = filterSweep.process();                    // -1.0 to +1.0
 *   float cutoff = mapf(mod, -1.0f, 1.0f, 200.0f, 3000.0f);  // Scale to Hz
 *   filter.setFrequency(cutoff);
 *
 * Waveform character for modulation:
 *   SINE     — Smooth, natural-sounding sweep
 *   TRIANGLE — Linear sweep, slightly more "mechanical" than sine
 *   SAW      — Ramp up then snap back. Good for rhythmic filter sweeps.
 *   SQUARE   — Abrupt switch between two values. Useful for alternating states.
 */

#pragma once

#include "dsp_common.h"
#include "oscillator.h" // For the Waveform enum

class LFO {
public:
    LFO() = default;

    /**
     * Set the LFO frequency in Hz.
     *
     * @param hz  Frequency in Hertz. Typical range: 0.01 to 20 Hz.
     *            0.1 Hz = one cycle every 10 seconds (slow, meditative)
     *            1.0 Hz = one cycle per second (gentle pulse)
     *            5.0 Hz = five cycles per second (vibrato speed)
     *
     * Example:
     *   lfo.setFrequency(0.05f);  // Very slow: 20-second cycle
     */
    void setFrequency(float hz) {
        phaseInc_ = hz / SAMPLE_RATE;
    }

    /**
     * Set the LFO waveform shape.
     * @param w  One of: Waveform::SINE, SAW, SQUARE, TRIANGLE
     *
     * See Waveform enum in oscillator.h for harmonic characteristics.
     */
    void setWaveform(Waveform w) { waveform_ = w; }

    /**
     * Generate the next LFO sample.
     *
     * Call this once per audio sample in your callback. The output is
     * always in the range [-1.0, +1.0] — you scale it to whatever
     * parameter range you need using mapf() or simple multiplication.
     *
     * @return  Current LFO value in [-1.0, +1.0]
     *
     * Example:
     *   float mod = lfo.process();
     *   // Use mod to control delay feedback (0.2 to 0.8 range):
     *   float feedback = mapf(mod, -1.0f, 1.0f, 0.2f, 0.8f);
     */
    float process() {
        float out = 0.0f;

        // Same waveform math as Oscillator — see oscillator.h for detailed
        // explanations of each shape's computation.
        switch (waveform_) {
            case Waveform::SINE:
                out = fastSin(phase_ * TWO_PI_F);
                break;
            case Waveform::SAW:
                out = 2.0f * phase_ - 1.0f;
                break;
            case Waveform::SQUARE:
                out = phase_ < 0.5f ? 1.0f : -1.0f;
                break;
            case Waveform::TRIANGLE:
                out = phase_ < 0.5f
                    ? 4.0f * phase_ - 1.0f
                    : 3.0f - 4.0f * phase_;
                break;
        }

        // Advance and wrap phase (see Oscillator::process() for explanation)
        phase_ += phaseInc_;
        if (phase_ >= 1.0f) phase_ -= 1.0f;

        return out;
    }

    /** Get the current phase (0.0–1.0). */
    float getPhase() const { return phase_; }

    /** Set phase directly. Useful for syncing multiple LFOs together. */
    void setPhase(float p) { phase_ = p; }

    /**
     * Reset phase to zero (beginning of cycle).
     * Useful for syncing the LFO to a tempo clock on each beat.
     *
     * Example:
     *   if (clock.process()) {  // On each beat...
     *       lfo.reset();        // Restart the LFO cycle
     *   }
     */
    void reset() { phase_ = 0.0f; }

private:
    // Same fast sine approximation as Oscillator (duplicated here so LFO
    // stays self-contained — the compiler will inline it anyway)
    static float fastSin(float x) {
        x = fmodf(x, TWO_PI_F);
        if (x > PI_F) x -= TWO_PI_F;
        if (x < -PI_F) x += TWO_PI_F;
        const float B = 4.0f / PI_F;
        const float C = -4.0f / (PI_F * PI_F);
        float y = B * x + C * x * fabsf(x);
        y = 0.225f * (y * fabsf(y) - y) + y;
        return y;
    }

    Waveform waveform_ = Waveform::SINE;
    float phase_ = 0.0f;
    float phaseInc_ = 1.0f / SAMPLE_RATE; // Default: 1 Hz
};
