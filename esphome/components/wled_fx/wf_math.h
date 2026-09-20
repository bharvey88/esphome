#pragma once

/* Scaling, wave, easing and colour-container code in this file is derived from the
 * trimmed FastLED subset that WLED 16.0.1 vendors at
 * wled00/src/dependencies/fastled_slim/fastled_slim.h and .cpp.
 *
 * The MIT License (MIT)
 * Copyright (c) 2013 FastLED
 * Modified for WLED by @dedehai.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * The trigonometry, beat, Perlin noise and random helpers below are derived from
 * WLED 16.0.1 wled00/wled_math.cpp and wled00/util.cpp.
 * Copyright (c) 2016-present Christian Schwinne and individual WLED contributors.
 * Licensed under the EUPL v. 1.2 or later, distributed here under GPLv3 per the
 * EUPL Article 5 compatibility clause.
 */

#include <cmath>
#include <cstdint>
#include <cstring>

#include "wf_platform.h"

// Neither is standard C++: M_PI is a POSIX extension that some standard libraries
// hide behind a feature macro, and M_TWOPI is WLED's own. Effect bodies use both,
// so define whichever the toolchain did not.
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#ifndef M_TWOPI
#define M_TWOPI (M_PI * 2.0)
#endif

namespace esphome::wled_fx {

// --- scaling, derived from FastLED ---------------------------------------------

inline uint8_t scale8(uint8_t i, uint8_t scale) { return (static_cast<int>(i) * (1 + static_cast<int>(scale))) >> 8; }
inline uint8_t scale8_video(uint8_t i, uint8_t scale) {
  return ((static_cast<int>(i) * static_cast<int>(scale)) >> 8) + ((i && scale) ? 1 : 0);
}
inline uint16_t scale16(uint16_t i, uint16_t scale) {
  return (static_cast<uint32_t>(i) * (1 + static_cast<uint32_t>(scale))) >> 16;
}
inline uint8_t qadd8(uint8_t i, uint8_t j) {
  unsigned t = i + j;
  return t > 255 ? 255 : t;
}
inline uint8_t qsub8(uint8_t i, uint8_t j) {
  int t = i - j;
  return t < 0 ? 0 : t;
}
inline uint8_t qmul8(uint8_t i, uint8_t j) {
  unsigned p = static_cast<unsigned>(i) * static_cast<unsigned>(j);
  return p > 255 ? 255 : p;
}
inline int8_t abs8(int8_t i) { return i < 0 ? -i : i; }
inline uint8_t lerp8by8(uint8_t a, uint8_t b, uint8_t frac) {
  return a + (((static_cast<int32_t>(b) - static_cast<int32_t>(a)) * (static_cast<int32_t>(frac) + 1)) >> 8);
}

// --- waves and easing, derived from FastLED -------------------------------------

uint8_t ease8_in_out_cubic(uint8_t i);
uint16_t ease16_in_out_cubic(uint16_t i);
uint8_t ease8_in_out_quad(uint8_t i);
uint8_t triwave8(uint8_t in);
uint16_t triwave16(uint16_t in);
uint8_t quadwave8(uint8_t in);
uint8_t cubicwave8(uint8_t in);

// --- trigonometry, derived from WLED wled_math.cpp ------------------------------

int16_t sin16_t(uint16_t theta);
int16_t cos16_t(uint16_t theta);
uint8_t sin8_t(uint8_t theta);
uint8_t cos8_t(uint8_t theta);
float sin_approx(float theta);
float cos_approx(float theta);
float tan_approx(float x);
float atan2_t(float y, float x);
float acos_t(float x);
float asin_t(float x);
float atan_t(float x);
float floor_t(float x);
float fmod_t(float num, float denom);
uint32_t sqrt32_bw(uint32_t x);
float mapf(float x, float in_min, float in_max, float out_min, float out_max);

// --- beats, derived from WLED util.cpp ------------------------------------------
// WLED reads millis() inside these. The engine passes the frame timestamp instead so
// every effect in a frame sees one consistent clock; see PORTING.md.

uint16_t beat88(uint16_t beats_per_minute_88, uint32_t now, uint32_t timebase = 0);
uint16_t beat16(uint16_t beats_per_minute, uint32_t now, uint32_t timebase = 0);
uint8_t beat8(uint16_t beats_per_minute, uint32_t now, uint32_t timebase = 0);
uint16_t beatsin88_t(uint16_t beats_per_minute_88, uint16_t lowest, uint16_t highest, uint32_t now,
                     uint32_t timebase = 0, uint16_t phase_offset = 0);
uint16_t beatsin16_t(uint16_t beats_per_minute, uint16_t lowest, uint16_t highest, uint32_t now, uint32_t timebase = 0,
                     uint16_t phase_offset = 0);
uint8_t beatsin8_t(uint16_t beats_per_minute, uint8_t lowest, uint8_t highest, uint32_t now, uint32_t timebase = 0,
                   uint8_t phase_offset = 0);

// --- Perlin noise, derived from WLED util.cpp -----------------------------------

int32_t perlin1d_raw(uint32_t x, bool is16bit = false);
int32_t perlin2d_raw(uint32_t x, uint32_t y, bool is16bit = false);
int32_t perlin3d_raw(uint32_t x, uint32_t y, uint32_t z, bool is16bit = false);
uint16_t perlin16(uint32_t x);
uint16_t perlin16(uint32_t x, uint32_t y);
uint16_t perlin16(uint32_t x, uint32_t y, uint32_t z);
uint8_t perlin8(uint16_t x);
uint8_t perlin8(uint16_t x, uint16_t y);
uint8_t perlin8(uint16_t x, uint16_t y, uint16_t z);
uint32_t hash_int(uint32_t s);

// --- random, wrappers over the platform RNG -------------------------------------

inline uint32_t hw_random() { return platform_random_u32(); }
uint32_t hw_random(uint32_t upperlimit);
int32_t hw_random(int32_t lowerlimit, int32_t upperlimit);
inline uint16_t hw_random16() { return static_cast<uint16_t>(platform_random_u32() >> 16); }
uint16_t hw_random16(uint32_t upperlimit);
uint16_t hw_random16(uint32_t lowerlimit, uint32_t upperlimit);
inline uint8_t hw_random8() { return static_cast<uint8_t>(platform_random_u32() >> 24); }
uint8_t hw_random8(uint32_t upperlimit);
uint8_t hw_random8(uint32_t lowerlimit, uint32_t upperlimit);

/* Repeatable pseudo random generator, derived from WLED 16.0.1 wled00/prng.h. */
class Prng {
 public:
  explicit Prng(uint16_t initial_seed = 0x1234) : seed_(initial_seed) {}
  void set_seed(uint16_t s) { this->seed_ = s; }
  uint16_t get_seed() const { return this->seed_; }
  uint16_t random16() {
    this->seed_ = this->seed_ * 3001 + 31683;
    this->seed_ ^= this->seed_ >> 7;
    return this->seed_;
  }
  uint16_t random16(uint16_t lim) { return (static_cast<uint32_t>(this->random16()) * lim) >> 16; }
  uint16_t random16(uint16_t min, uint16_t lim) { return this->random16(static_cast<uint16_t>(lim - min)) + min; }
  uint8_t random8() { return static_cast<uint8_t>(this->random16()); }
  uint8_t random8(uint8_t lim) { return static_cast<uint8_t>((static_cast<uint16_t>(this->random8()) * lim) >> 8); }
  uint8_t random8(uint8_t min, uint8_t lim) { return this->random8(static_cast<uint8_t>(lim - min)) + min; }

 private:
  uint16_t seed_;
};

// Integer map with WLED / Arduino semantics. Effect bodies use bare map().
inline int32_t wf_map(int32_t x, int32_t in_min, int32_t in_max, int32_t out_min, int32_t out_max) {
  if (in_max == in_min)
    return out_min;
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

// WLED spells the float trigonometry sin_t / cos_t / tan_t, which are macros over
// the approximations above. Effect bodies keep those names.
inline float sin_t(float theta) { return sin_approx(theta); }
inline float cos_t(float theta) { return cos_approx(theta); }
inline float tan_t(float x) { return tan_approx(x); }

}  // namespace esphome::wled_fx
