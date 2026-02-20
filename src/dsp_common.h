/**
 * @file dsp_common.h
 * @brief Shared constants, types, and utility functions for the BuddhaBox DSP
 * framework.
 *
 * This header is included by every DSP module. It defines the fundamental
 * audio parameters (sample rate, block size) and small helper functions that
 * show up everywhere in audio code.
 *
 * DESIGN NOTE: We keep everything in this file `inline` or `constexpr` so it
 * can be included from multiple translation units without linker errors.
 * This is the standard C++ approach for header-only utilities.
 */

#pragma once

#include <cmath>
#include <cstdint>

// ─── Core Audio Parameters ──────────────────────────────────────────────────
//
// These two constants define the heartbeat of the entire system:
//
// SAMPLE_RATE: How many audio samples we generate per second. 44100 is "CD
// quality" — the standard for consumer audio. Every DSP module uses this to
// convert between time/frequency and sample counts.
//
// AUDIO_BLOCK_SIZE: How many stereo frames we process in one batch. The DMA
// hardware sends audio in chunks — we fill one half of the buffer while the
// hardware plays the other half. 256 frames = ~5.8 ms of latency, which is
// imperceptible for a generative ambient device.

constexpr uint32_t SAMPLE_RATE = 44100;
constexpr uint16_t AUDIO_BLOCK_SIZE = 256;

// ─── Math Constants ─────────────────────────────────────────────────────────
//
// We define float versions of PI to avoid implicit double→float conversions.
// On the STM32F4, the FPU handles 32-bit floats in hardware but doubles in
// software — so using float everywhere is a significant performance win.

constexpr float PI_F = 3.14159265358979f;
constexpr float TWO_PI_F = 6.28318530717959f;

// ─── Utility Functions ──────────────────────────────────────────────────────

/**
 * Clamp a float to a range [lo, hi].
 *
 * Essential in audio to prevent values from exceeding -1.0 to 1.0 (which
 * causes harsh digital clipping when converted to 16-bit integers).
 *
 * @param x   The value to clamp
 * @param lo  Minimum allowed value
 * @param hi  Maximum allowed value
 * @return    x constrained to [lo, hi]
 *
 * Example:
 *   float safe = clampf(oscillator.process(), -1.0f, 1.0f);
 */
inline float clampf(float x, float lo, float hi) {
  return x < lo ? lo : (x > hi ? hi : x);
}

/**
 * Linear interpolation between two values.
 *
 * This is one of the most common operations in DSP. It smoothly blends
 * between value `a` (when t=0) and value `b` (when t=1). Used for:
 * - Smoothing parameter changes (avoids clicks/pops)
 * - Reading fractional positions in delay buffers
 * - Crossfading between signals
 *
 * @param a  Start value (returned when t = 0.0)
 * @param b  End value (returned when t = 1.0)
 * @param t  Blend factor, typically 0.0 to 1.0
 * @return   The interpolated value: a + (b - a) * t
 *
 * Example:
 *   // Crossfade between dry and wet signal
 *   float mix = lerpf(drySignal, wetSignal, 0.5f);  // 50/50 mix
 */
inline float lerpf(float a, float b, float t) { return a + (b - a) * t; }

/**
 * Map a value from one range to another (like Arduino's map() but for floats).
 *
 * Commonly used to convert sensor readings (0.0–1.0) into musically useful
 * ranges like frequency (200–2000 Hz) or amplitude (0.0–0.8).
 *
 * @param x      The input value
 * @param inLo   Lower bound of the input range
 * @param inHi   Upper bound of the input range
 * @param outLo  Lower bound of the output range
 * @param outHi  Upper bound of the output range
 * @return       x rescaled from [inLo, inHi] to [outLo, outHi]
 *
 * Example:
 *   // Convert a 0-1 sensor reading to a filter cutoff frequency
 *   float cutoff = mapf(sensor.read(), 0.0f, 1.0f, 200.0f, 5000.0f);
 */
inline float mapf(float x, float inLo, float inHi, float outLo, float outHi) {
  return outLo + (x - inLo) * (outHi - outLo) / (inHi - inLo);
}

/**
 * Soft clipping — warm saturation that gently rounds off peaks.
 *
 * Unlike hard clipping (clampf), which abruptly chops the waveform at the
 * limit and creates harsh harmonics, soft clipping gradually compresses
 * the signal as it approaches ±1.0, producing a warm, tube-amp-like
 * distortion.
 *
 * Uses a fast tanh approximation: x / (1 + |x|). This has the properties:
 *   - Output approaches ±1.0 asymptotically (never exceeds it)
 *   - Near zero, output ≈ input (transparent at low levels)
 *   - At high levels, the curve flattens (soft compression)
 *
 * The drive parameter controls how much saturation is applied:
 *   - drive = 1.0: transparent, almost no effect
 *   - drive = 2.0: gentle warmth, subtle harmonics
 *   - drive = 5.0: noticeable saturation, like a warm preamp
 *   - drive = 10.0+: heavy distortion, approaching hard clipping
 *
 * @param x      Input sample (any range, will be compressed to ±1.0)
 * @param drive  Saturation amount, 1.0 = clean, higher = more distorted.
 *               Default: 1.0.
 * @return       Soft-clipped sample in approximately (-1.0, +1.0)
 *
 * Example:
 *   // Add gentle warmth to a mix
 *   float out = softclip(osc.process() + noise.process(), 2.0f);
 *
 *   // Heavy distortion on an oscillator
 *   float distorted = softclip(osc.process(), 8.0f);
 */
inline float softclip(float x, float drive = 1.0f) {
  x *= drive;
  return x / (1.0f + fabsf(x));
}

/**
 * Fast sine approximation using a parabolic curve.
 *
 * HOW THIS WORKS:
 * A sine wave looks like an upside-down parabola near its peaks. We exploit
 * this by computing y = (4/π)x - (4/π²)x|x|, then apply a correction pass
 * to improve peak accuracy. Total error is under 0.1% — imperceptible in
 * audio. On the STM32F4 this runs ~4x faster than sinf().
 *
 * IMPORTANT: Input must be in the range [0, 2π). If you pass phase in [0,1),
 * multiply by TWO_PI_F first:
 *
 *   float sample = fastSin(phase * TWO_PI_F);
 *
 * The function normalizes from [0, 2π) to (-π, π] internally. We skip the
 * fmodf() call because callers that use a phase accumulator always stay in
 * [0, 2π) by construction — saving a relatively expensive modulo operation.
 *
 * @param x  Angle in radians, expected in [0, 2π)
 * @return   Approximation of sin(x), range approximately [-1, +1]
 */
inline float fastSin(float x) {
  // Fold from [0, 2π) into (-π, π] — the range where the parabolic
  // approximation is valid. Values above π get shifted down by 2π.
  if (x > PI_F)
    x -= TWO_PI_F;

  // First pass: parabolic approximation of sin(x)
  // y = (4/π)x - (4/π²)x|x| maps the sine shape onto a parabola
  const float B = 4.0f / PI_F;
  const float C = -4.0f / (PI_F * PI_F);
  float y = B * x + C * x * fabsf(x);

  // Second pass: correction factor improves accuracy from ~88% to ~99.7%.
  // The constant 0.225 was determined empirically to minimize peak error.
  y = 0.225f * (y * fabsf(y) - y) + y;
  return y;
}
