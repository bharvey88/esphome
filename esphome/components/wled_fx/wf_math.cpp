/* See wf_math.h for the combined MIT (FastLED) and EUPL-to-GPLv3 (WLED) notice
 * that covers this file. */

#include "wf_math.h"

#include <cmath>
#include <numbers>

namespace esphome::wled_fx {

namespace {

constexpr float PI_F = std::numbers::pi_v<float>;
constexpr float HALF_PI_F = PI_F / 2.0f;
constexpr float QUARTER_PI_F = PI_F / 4.0f;
constexpr float TWO_PI_F = PI_F * 2.0f;

}  // namespace

// --- waves and easing -----------------------------------------------------------

uint8_t ease8_in_out_cubic(uint8_t i) {
  uint32_t ii = static_cast<uint32_t>(i) * i;
  uint32_t factor = (3u << 8) - (static_cast<uint32_t>(i) << 1);
  return (ii * factor) >> 16;
}

uint16_t ease16_in_out_cubic(uint16_t i) {
  uint32_t ii = (static_cast<uint32_t>(i) * i) >> 16;
  uint32_t factor = (3u << 16) - (static_cast<uint32_t>(i) << 1);
  return (ii * factor) >> 16;
}

uint8_t ease8_in_out_quad(uint8_t i) {
  uint32_t j = i;
  if (j & 0x80)
    j = 255 - j;
  uint32_t jj = (j * j) >> 7;
  return (i & 0x80) ? (255 - jj) : jj;
}

uint8_t triwave8(uint8_t in) {
  if (in & 0x80)
    in = 255 - in;
  return in << 1;
}

uint16_t triwave16(uint16_t in) {
  if (in < 0x8000)
    return in * 2;
  return 0xFFFF - (in - 0x8000) * 2;
}

uint8_t quadwave8(uint8_t in) { return ease8_in_out_quad(triwave8(in)); }

uint8_t cubicwave8(uint8_t in) { return ease8_in_out_cubic(triwave8(in)); }

// --- trigonometry ---------------------------------------------------------------

int16_t sin16_t(uint16_t theta) {
  int scale = 1;
  if (theta > 0x7FFF) {
    theta = 0xFFFF - theta;
    scale = -1;
  }
  uint32_t precal = static_cast<uint32_t>(theta) * (0x7FFF - theta);
  uint64_t numerator = static_cast<uint64_t>(precal) * (4 * 0x7FFF);
  int32_t denominator = 1342095361 - static_cast<int32_t>(precal);
  int16_t result = static_cast<int16_t>(numerator / denominator);
  return result * scale;
}

int16_t cos16_t(uint16_t theta) { return sin16_t(theta + 0x4000); }

uint8_t sin8_t(uint8_t theta) {
  int32_t sin16 = sin16_t(static_cast<uint16_t>(theta) * 257);
  sin16 += 0x7FFF + 128;
  return (sin16 > 0xFFFF ? 0xFFFF : sin16) >> 8;
}

uint8_t cos8_t(uint8_t theta) { return sin8_t(theta + 64); }

float sin_approx(float theta) {
  uint16_t scaled_theta = static_cast<uint16_t>(static_cast<int>(theta * (65535.0f / TWO_PI_F)));
  return static_cast<float>(sin16_t(scaled_theta)) / 32767.0f;
}

float cos_approx(float theta) {
  uint16_t scaled_theta = static_cast<uint16_t>(static_cast<int>(theta * (65535.0f / TWO_PI_F)));
  return static_cast<float>(sin16_t(static_cast<uint16_t>(scaled_theta + 0x4000))) / 32767.0f;
}

float tan_approx(float x) {
  float c = cos_approx(x);
  if (c == 0.0f)
    return 0;
  return sin_approx(x) / c;
}

float atan2_t(float y, float x) {
  constexpr float const_a = 0.1963f;
  constexpr float const_b = 0.9817f;
  float abs_y = std::fabs(y);
  float abs_x = std::fabs(x);
  float r = (abs_x - abs_y) / (abs_y + abs_x + 1e-10f);
  float angle;
  if (x < 0) {
    r = -r;
    angle = HALF_PI_F + QUARTER_PI_F;
  } else {
    angle = HALF_PI_F - QUARTER_PI_F;
  }
  angle += (const_a * (r * r) - const_b) * r;
  return y < 0 ? -angle : angle;
}

float acos_t(float x) {
  float negate = static_cast<float>(x < 0);
  float xabs = std::fabs(x);
  float ret = -0.0187293f;
  ret = ret * xabs;
  ret = ret + 0.0742610f;
  ret = ret * xabs;
  ret = ret - 0.2121144f;
  ret = ret * xabs;
  ret = ret + HALF_PI_F;
  ret = ret * std::sqrt(1.0f - xabs);
  ret = ret - 2 * negate * ret;
  return negate * PI_F + ret;
}

float asin_t(float x) { return HALF_PI_F - acos_t(x); }

float atan_t(float x) {
  constexpr float a = 0.0776509570923569f;
  constexpr float b = -0.287434475393028f;
  constexpr float c = QUARTER_PI_F - a - b;
  constexpr float c0 = 0.089494f;
  constexpr float c1 = 0.974207f;
  constexpr float c2 = -0.326175f;
  constexpr float c3 = 0.05375f;
  constexpr float c4 = -0.003445f;

  bool neg = (x < 0);
  x = std::fabs(x);
  float res;
  if (x > 5.0f) {
    res = HALF_PI_F - (1.0f / x);
  } else if (x > 1.0f) {
    float xx = x * x;
    res = (c4 * xx * xx) + (c3 * xx * x) + (c2 * xx) + (c1 * x) + c0;
  } else {
    float xx = x * x;
    res = ((a * xx + b) * xx + c) * x;
  }
  return neg ? -res : res;
}

float floor_t(float x) {
  bool neg = x < 0;
  int val = static_cast<int>(x);
  if (neg)
    val--;
  return static_cast<float>(val);
}

float fmod_t(float num, float denom) {
  int tquot = static_cast<int>(num / denom);
  return num - tquot * denom;
}

uint32_t sqrt32_bw(uint32_t x) {
  uint32_t res = 0;
  uint32_t bit;
  uint32_t num = x;

  if (num < (1u << 10)) {
    bit = 1u << 10;
  } else if (num < (1u << 20)) {
    bit = 1u << 20;
  } else {
    bit = 1u << 30;
  }

  while (bit > num) {
    bit >>= 2;
  }

  while (bit != 0) {
    if (num >= res + bit) {
      num -= res + bit;
      res = (res >> 1) + bit;
    } else {
      res >>= 1;
    }
    bit >>= 2;
  }
  return res;
}

float mapf(float x, float in_min, float in_max, float out_min, float out_max) {
  if (in_max == in_min)
    return out_min;
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

// --- beats ----------------------------------------------------------------------

uint16_t beat88(uint16_t beats_per_minute_88, uint32_t now, uint32_t timebase) {
  return ((now - timebase) * beats_per_minute_88 * 280) >> 16;
}

uint16_t beat16(uint16_t beats_per_minute, uint32_t now, uint32_t timebase) {
  if (beats_per_minute < 256)
    beats_per_minute <<= 8;
  return beat88(beats_per_minute, now, timebase);
}

uint8_t beat8(uint16_t beats_per_minute, uint32_t now, uint32_t timebase) {
  return beat16(beats_per_minute, now, timebase) >> 8;
}

uint16_t beatsin88_t(uint16_t beats_per_minute_88, uint16_t lowest, uint16_t highest, uint32_t now, uint32_t timebase,
                     uint16_t phase_offset) {
  uint16_t beat = beat88(beats_per_minute_88, now, timebase);
  uint16_t beatsin = static_cast<uint16_t>(sin16_t(beat + phase_offset) + 32768);
  uint16_t rangewidth = highest - lowest;
  return lowest + scale16(beatsin, rangewidth);
}

uint16_t beatsin16_t(uint16_t beats_per_minute, uint16_t lowest, uint16_t highest, uint32_t now, uint32_t timebase,
                     uint16_t phase_offset) {
  uint16_t beat = beat16(beats_per_minute, now, timebase);
  uint16_t beatsin = static_cast<uint16_t>(sin16_t(beat + phase_offset) + 32768);
  uint16_t rangewidth = highest - lowest;
  return lowest + scale16(beatsin, rangewidth);
}

uint8_t beatsin8_t(uint16_t beats_per_minute, uint8_t lowest, uint8_t highest, uint32_t now, uint32_t timebase,
                   uint8_t phase_offset) {
  uint8_t beat = beat8(beats_per_minute, now, timebase);
  uint8_t beatsin = sin8_t(beat + phase_offset);
  uint8_t rangewidth = highest - lowest;
  return lowest + scale8(beatsin, rangewidth);
}

// --- Perlin noise ---------------------------------------------------------------

namespace {

constexpr int PERLIN_SHIFT = 1;

inline int32_t hash_to_gradient(uint32_t h) { return static_cast<int32_t>(h & 0x03) - 2; }

inline int32_t gradient_1d(uint32_t x0, int32_t dx) {
  uint32_t h = x0 * 0x27D4EB2Du;
  h ^= h >> 15;
  h *= 0x92C3412Bu;
  h ^= h >> 13;
  h ^= h >> 7;
  return (hash_to_gradient(h) * dx) >> PERLIN_SHIFT;
}

inline int32_t gradient_2d(uint32_t x0, int32_t dx, uint32_t y0, int32_t dy) {
  uint32_t h = (x0 * 0x27D4EB2Du) ^ (y0 * 0xB5297A4Du);
  h ^= h >> 15;
  h *= 0x92C3412Bu;
  h ^= h >> 13;
  return (hash_to_gradient(h) * dx + hash_to_gradient(h >> PERLIN_SHIFT) * dy) >> (1 + PERLIN_SHIFT);
}

inline int32_t gradient_3d(uint32_t x0, int32_t dx, uint32_t y0, int32_t dy, uint32_t z0, int32_t dz) {
  uint32_t h = (x0 * 0x27D4EB2Du) ^ (y0 * 0xB5297A4Du) ^ (z0 * 0x1B56C4E9u);
  h ^= h >> 15;
  h *= 0x92C3412Bu;
  h ^= h >> 13;
  return ((hash_to_gradient(h) * dx + hash_to_gradient(h >> (1 + PERLIN_SHIFT)) * dy +
           hash_to_gradient(h >> (1 + 2 * PERLIN_SHIFT)) * dz) *
          85) >>
         (8 + PERLIN_SHIFT);
}

uint32_t smoothstep(const uint32_t t) {
  uint32_t t_squared = (t * t) >> 16;
  uint32_t factor = (3u << 16) - (t << 1);
  return (t_squared * factor) >> 18;
}

inline int32_t lerp_perlin(int32_t a, int32_t b, int32_t t) { return a + (((b - a) * t) >> 14); }

}  // namespace

int32_t perlin1d_raw(uint32_t x, bool is16bit) {
  int32_t x0 = x >> 16;
  int32_t x1 = x0 + 1;
  if (is16bit)
    x1 = x1 & 0xFF;

  int32_t dx0 = x & 0xFFFF;
  int32_t dx1 = dx0 - 0x10000;
  int32_t g0 = gradient_1d(x0, dx0);
  int32_t g1 = gradient_1d(x1, dx1);
  int32_t tx = smoothstep(dx0);
  return lerp_perlin(g0, g1, tx);
}

int32_t perlin2d_raw(uint32_t x, uint32_t y, bool is16bit) {
  int32_t x0 = x >> 16;
  int32_t y0 = y >> 16;
  int32_t x1 = x0 + 1;
  int32_t y1 = y0 + 1;

  if (is16bit) {
    x1 = x1 & 0xFF;
    y1 = y1 & 0xFF;
  }

  int32_t dx0 = x & 0xFFFF;
  int32_t dy0 = y & 0xFFFF;
  int32_t dx1 = dx0 - 0x10000;
  int32_t dy1 = dy0 - 0x10000;

  int32_t g00 = gradient_2d(x0, dx0, y0, dy0);
  int32_t g10 = gradient_2d(x1, dx1, y0, dy0);
  int32_t g01 = gradient_2d(x0, dx0, y1, dy1);
  int32_t g11 = gradient_2d(x1, dx1, y1, dy1);

  uint32_t tx = smoothstep(dx0);
  uint32_t ty = smoothstep(dy0);

  int32_t nx0 = lerp_perlin(g00, g10, tx);
  int32_t nx1 = lerp_perlin(g01, g11, tx);

  return lerp_perlin(nx0, nx1, ty);
}

int32_t perlin3d_raw(uint32_t x, uint32_t y, uint32_t z, bool is16bit) {
  int32_t x0 = x >> 16;
  int32_t y0 = y >> 16;
  int32_t z0 = z >> 16;
  int32_t x1 = x0 + 1;
  int32_t y1 = y0 + 1;
  int32_t z1 = z0 + 1;

  if (is16bit) {
    x1 = x1 & 0xFF;
    y1 = y1 & 0xFF;
    z1 = z1 & 0xFF;
  }

  int32_t dx0 = x & 0xFFFF;
  int32_t dy0 = y & 0xFFFF;
  int32_t dz0 = z & 0xFFFF;
  int32_t dx1 = dx0 - 0x10000;
  int32_t dy1 = dy0 - 0x10000;
  int32_t dz1 = dz0 - 0x10000;

  int32_t g000 = gradient_3d(x0, dx0, y0, dy0, z0, dz0);
  int32_t g001 = gradient_3d(x0, dx0, y0, dy0, z1, dz1);
  int32_t g010 = gradient_3d(x0, dx0, y1, dy1, z0, dz0);
  int32_t g011 = gradient_3d(x0, dx0, y1, dy1, z1, dz1);
  int32_t g100 = gradient_3d(x1, dx1, y0, dy0, z0, dz0);
  int32_t g101 = gradient_3d(x1, dx1, y0, dy0, z1, dz1);
  int32_t g110 = gradient_3d(x1, dx1, y1, dy1, z0, dz0);
  int32_t g111 = gradient_3d(x1, dx1, y1, dy1, z1, dz1);

  uint32_t tx = smoothstep(dx0);
  uint32_t ty = smoothstep(dy0);
  uint32_t tz = smoothstep(dz0);

  int32_t nx0 = lerp_perlin(g000, g100, tx);
  int32_t nx1 = lerp_perlin(g010, g110, tx);
  int32_t nx2 = lerp_perlin(g001, g101, tx);
  int32_t nx3 = lerp_perlin(g011, g111, tx);
  int32_t ny0 = lerp_perlin(nx0, nx1, ty);
  int32_t ny1 = lerp_perlin(nx2, nx3, ty);

  return lerp_perlin(ny0, ny1, tz);
}

uint16_t perlin16(uint32_t x) { return ((perlin1d_raw(x) * 1159) >> 10) + 32803; }

uint16_t perlin16(uint32_t x, uint32_t y) { return ((perlin2d_raw(x, y) * 1537) >> 10) + 32725; }

uint16_t perlin16(uint32_t x, uint32_t y, uint32_t z) { return ((perlin3d_raw(x, y, z) * 1731) >> 10) + 33147; }

uint8_t perlin8(uint16_t x) {
  return (((perlin1d_raw(static_cast<uint32_t>(x) << 8, true) * 1353) >> 10) + 32769) >> 8;
}

uint8_t perlin8(uint16_t x, uint16_t y) {
  return (((perlin2d_raw(static_cast<uint32_t>(x) << 8, static_cast<uint32_t>(y) << 8, true) * 1620) >> 10) + 32771) >>
         8;
}

uint8_t perlin8(uint16_t x, uint16_t y, uint16_t z) {
  return (((perlin3d_raw(static_cast<uint32_t>(x) << 8, static_cast<uint32_t>(y) << 8, static_cast<uint32_t>(z) << 8,
                         true) *
            2015) >>
           10) +
          33168) >>
         8;
}

uint32_t hash_int(uint32_t s) {
  s ^= s >> 16;
  s *= 0x7FEB352Du;
  s ^= s >> 15;
  s *= 0x846CA68Bu;
  s ^= s >> 16;
  return s;
}

// --- random ---------------------------------------------------------------------

uint32_t hw_random(uint32_t upperlimit) {
  uint32_t rnd = platform_random_u32();
  uint64_t scaled = static_cast<uint64_t>(rnd) * upperlimit;
  return static_cast<uint32_t>(scaled >> 32);
}

int32_t hw_random(int32_t lowerlimit, int32_t upperlimit) {
  if (lowerlimit >= upperlimit)
    return lowerlimit;
  return lowerlimit + static_cast<int32_t>(hw_random(static_cast<uint32_t>(upperlimit - lowerlimit)));
}

uint16_t hw_random16(uint32_t upperlimit) { return static_cast<uint16_t>(hw_random(upperlimit)); }

uint16_t hw_random16(uint32_t lowerlimit, uint32_t upperlimit) {
  if (lowerlimit >= upperlimit)
    return static_cast<uint16_t>(lowerlimit);
  return static_cast<uint16_t>(lowerlimit + hw_random(upperlimit - lowerlimit));
}

uint8_t hw_random8(uint32_t upperlimit) { return static_cast<uint8_t>(hw_random(upperlimit)); }

uint8_t hw_random8(uint32_t lowerlimit, uint32_t upperlimit) {
  if (lowerlimit >= upperlimit)
    return static_cast<uint8_t>(lowerlimit);
  return static_cast<uint8_t>(lowerlimit + hw_random(upperlimit - lowerlimit));
}

}  // namespace esphome::wled_fx
