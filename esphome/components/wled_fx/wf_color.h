#pragma once

/* CRGB, CHSV, CRGBPalette16 and hsv2rgb_rainbow are derived from the trimmed
 * FastLED subset WLED 16.0.1 vendors at wled00/src/dependencies/fastled_slim/.
 * The MIT License (MIT), Copyright (c) 2013 FastLED, modified by @dedehai.
 * The full MIT notice is reproduced in wf_math.h.
 *
 * CRGBW, CHSV32, color_blend, color_add, color_fade, ColorFromPalette and the
 * HSV conversions are derived from WLED 16.0.1 wled00/colors.h and
 * wled00/colors.cpp.
 * Copyright (c) 2016-present Christian Schwinne and individual WLED contributors.
 * Licensed under the EUPL v. 1.2 or later, distributed here under GPLv3 per the
 * EUPL Article 5 compatibility clause.
 *
 * Type names keep their WLED spelling on purpose so effect bodies can be diffed
 * against future WLED releases. See PORTING.md.
 */

#include <cstdint>
#include <cstring>

#include "wf_math.h"

namespace esphome {
namespace wled_fx {

// 32 bit colour packing, 0xWWRRGGBB. WLED spells these as macros; here they are
// namespaced inline functions so effect bodies stay verbatim without polluting the
// global namespace.
inline constexpr uint32_t RGBW32(uint32_t r, uint32_t g, uint32_t b, uint32_t w = 0) {
  return ((w & 0xFF) << 24) | ((r & 0xFF) << 16) | ((g & 0xFF) << 8) | (b & 0xFF);
}
inline constexpr uint8_t R(uint32_t c) { return static_cast<uint8_t>(c >> 16); }
inline constexpr uint8_t G(uint32_t c) { return static_cast<uint8_t>(c >> 8); }
inline constexpr uint8_t B(uint32_t c) { return static_cast<uint8_t>(c); }
inline constexpr uint8_t W(uint32_t c) { return static_cast<uint8_t>(c >> 24); }

inline constexpr uint32_t BLACK = 0x000000;
inline constexpr uint32_t WHITE = 0xFFFFFF;
inline constexpr uint32_t RED = 0xFF0000;
inline constexpr uint32_t GREEN = 0x00FF00;
inline constexpr uint32_t BLUE = 0x0000FF;
inline constexpr uint32_t YELLOW = 0xFFFF00;
inline constexpr uint32_t CYAN = 0x00FFFF;
inline constexpr uint32_t MAGENTA = 0xFF00FF;
inline constexpr uint32_t PURPLE = 0x400080;
inline constexpr uint32_t ORANGE = 0xFF3000;
// WLED FX.h: #define ULTRAWHITE (uint32_t)0xFFFFFFFF, white with the white channel
// driven as well.
inline constexpr uint32_t ULTRAWHITE = 0xFFFFFFFF;
// WLED FX.h: #define DARKSLATEGRAY (uint32_t)0x2F4F4F
inline constexpr uint32_t DARKSLATEGRAY = 0x2F4F4F;

struct CRGB;
struct CHSV;
struct CRGBW;
struct CHSV32;
class CRGBPalette16;

using TProgmemRGBPalette16 = uint32_t[16];

enum TBlendType : uint8_t {
  NOBLEND = 0,
  LINEARBLEND = 1,
  LINEARBLEND_NOWRAP = 2,
};

union TRGBGradientPaletteEntryUnion {
  struct {
    uint8_t index;
    uint8_t r;
    uint8_t g;
    uint8_t b;
  };
  uint32_t dword;
  uint8_t bytes[4];
};

void hsv2rgb_rainbow(uint16_t h, uint8_t s, uint8_t v, uint8_t *rgbdata, bool is_rgbw);

struct CHSV {
  union {
    struct {
      union {
        uint8_t hue;
        uint8_t h;
      };
      union {
        uint8_t saturation;
        uint8_t sat;
        uint8_t s;
      };
      union {
        uint8_t value;
        uint8_t val;
        uint8_t v;
      };
    };
    uint8_t raw[3];
  };

  inline CHSV() = default;
  inline CHSV(uint8_t ih, uint8_t is, uint8_t iv) : h(ih), s(is), v(iv) {}
  inline CHSV(const CHSV &rhs) = default;
  inline CHSV &operator=(const CHSV &rhs) = default;
  inline uint8_t &operator[](uint8_t x) { return raw[x]; }
  inline const uint8_t &operator[](uint8_t x) const { return raw[x]; }
};

struct CRGB {
  union {
    struct {
      union {
        uint8_t r;
        uint8_t red;
      };
      union {
        uint8_t g;
        uint8_t green;
      };
      union {
        uint8_t b;
        uint8_t blue;
      };
    };
    uint8_t raw[3];
  };

  inline CRGB() = default;
  inline CRGB(uint8_t ir, uint8_t ig, uint8_t ib) : r(ir), g(ig), b(ib) {}
  inline CRGB(uint32_t colorcode)  // NOLINT(google-explicit-constructor)
      : r(static_cast<uint8_t>(colorcode >> 16)),
        g(static_cast<uint8_t>(colorcode >> 8)),
        b(static_cast<uint8_t>(colorcode)) {}
  inline CRGB(const CRGB &rhs) = default;
  inline CRGB(const CHSV &rhs) {  // NOLINT(google-explicit-constructor)
    hsv2rgb_rainbow(static_cast<uint16_t>(rhs.h) << 8, rhs.s, rhs.v, raw, false);
  }

  inline uint8_t &operator[](uint8_t x) { return raw[x]; }
  inline const uint8_t &operator[](uint8_t x) const { return raw[x]; }

  inline CRGB &operator=(const CRGB &rhs) = default;
  inline CRGB &operator=(const CHSV &rhs) {
    hsv2rgb_rainbow(static_cast<uint16_t>(rhs.h) << 8, rhs.s, rhs.v, raw, false);
    return *this;
  }
  inline CRGB &operator=(const uint32_t colorcode) {
    r = static_cast<uint8_t>(colorcode >> 16);
    g = static_cast<uint8_t>(colorcode >> 8);
    b = static_cast<uint8_t>(colorcode);
    return *this;
  }
  inline CRGB &setRGB(uint8_t nr, uint8_t ng, uint8_t nb) {
    r = nr;
    g = ng;
    b = nb;
    return *this;
  }
  inline CRGB &setHSV(uint8_t hue, uint8_t sat, uint8_t val) {
    hsv2rgb_rainbow(static_cast<uint16_t>(hue) << 8, sat, val, raw, false);
    return *this;
  }
  inline CRGB &setHue(uint8_t hue) {
    hsv2rgb_rainbow(static_cast<uint16_t>(hue) << 8, 255, 255, raw, false);
    return *this;
  }
  inline CRGB &setColorCode(uint32_t colorcode) { return *this = colorcode; }

  inline CRGB &operator+=(const CRGB &rhs) {
    r = qadd8(r, rhs.r);
    g = qadd8(g, rhs.g);
    b = qadd8(b, rhs.b);
    return *this;
  }
  inline CRGB &addToRGB(uint8_t d) {
    r = qadd8(r, d);
    g = qadd8(g, d);
    b = qadd8(b, d);
    return *this;
  }
  inline CRGB &operator-=(const CRGB &rhs) {
    r = qsub8(r, rhs.r);
    g = qsub8(g, rhs.g);
    b = qsub8(b, rhs.b);
    return *this;
  }
  inline CRGB &subtractFromRGB(uint8_t d) {
    r = qsub8(r, d);
    g = qsub8(g, d);
    b = qsub8(b, d);
    return *this;
  }
  inline CRGB &operator/=(uint8_t d) {
    r /= d;
    g /= d;
    b /= d;
    return *this;
  }
  inline CRGB &operator>>=(uint8_t d) {
    r >>= d;
    g >>= d;
    b >>= d;
    return *this;
  }
  inline CRGB &operator*=(uint8_t d) {
    r = qmul8(r, d);
    g = qmul8(g, d);
    b = qmul8(b, d);
    return *this;
  }
  inline CRGB &nscale8_video(uint8_t scaledown) {
    uint8_t nonzeroscale = (scaledown != 0) ? 1 : 0;
    r = (r == 0) ? 0 : (((int) r * (int) scaledown) >> 8) + nonzeroscale;
    g = (g == 0) ? 0 : (((int) g * (int) scaledown) >> 8) + nonzeroscale;
    b = (b == 0) ? 0 : (((int) b * (int) scaledown) >> 8) + nonzeroscale;
    return *this;
  }
  inline CRGB &nscale8(uint8_t scaledown) {
    uint32_t scale_fixed = scaledown + 1;
    r = (static_cast<uint32_t>(r) * scale_fixed) >> 8;
    g = (static_cast<uint32_t>(g) * scale_fixed) >> 8;
    b = (static_cast<uint32_t>(b) * scale_fixed) >> 8;
    return *this;
  }
  inline CRGB &nscale8(const CRGB &scaledown) {
    r = wled_fx::scale8(r, scaledown.r);
    g = wled_fx::scale8(g, scaledown.g);
    b = wled_fx::scale8(b, scaledown.b);
    return *this;
  }
  inline CRGB scale8(uint8_t scaledown) const {
    CRGB out = *this;
    out.nscale8(scaledown);
    return out;
  }
  inline CRGB &fadeToBlackBy(uint8_t fadefactor) {
    uint32_t scale_fixed = 256 - fadefactor;
    r = (static_cast<uint32_t>(r) * scale_fixed) >> 8;
    g = (static_cast<uint32_t>(g) * scale_fixed) >> 8;
    b = (static_cast<uint32_t>(b) * scale_fixed) >> 8;
    return *this;
  }
  inline CRGB &operator|=(const CRGB &rhs) {
    if (rhs.r > r)
      r = rhs.r;
    if (rhs.g > g)
      g = rhs.g;
    if (rhs.b > b)
      b = rhs.b;
    return *this;
  }
  inline CRGB &operator&=(const CRGB &rhs) {
    if (rhs.r < r)
      r = rhs.r;
    if (rhs.g < g)
      g = rhs.g;
    if (rhs.b < b)
      b = rhs.b;
    return *this;
  }
  inline explicit operator bool() const { return r || g || b; }
  inline explicit operator uint32_t() const {
    return (static_cast<uint32_t>(r) << 16) | (static_cast<uint32_t>(g) << 8) | static_cast<uint32_t>(b);
  }
  inline CRGB operator-() const { return CRGB(255 - r, 255 - g, 255 - b); }
  inline uint8_t getAverageLight() const { return ((r + g + b) * 21846) >> 16; }

  enum HTMLColorCode : uint32_t {
    AliceBlue = 0xF0F8FF,
    Amethyst = 0x9966CC,
    AntiqueWhite = 0xFAEBD7,
    Aqua = 0x00FFFF,
    Aquamarine = 0x7FFFD4,
    Azure = 0xF0FFFF,
    Beige = 0xF5F5DC,
    Bisque = 0xFFE4C4,
    Black = 0x000000,
    BlanchedAlmond = 0xFFEBCD,
    Blue = 0x0000FF,
    BlueViolet = 0x8A2BE2,
    Brown = 0xA52A2A,
    BurlyWood = 0xDEB887,
    CadetBlue = 0x5F9EA0,
    Chartreuse = 0x7FFF00,
    Chocolate = 0xD2691E,
    Coral = 0xFF7F50,
    CornflowerBlue = 0x6495ED,
    Cornsilk = 0xFFF8DC,
    Crimson = 0xDC143C,
    Cyan = 0x00FFFF,
    DarkBlue = 0x00008B,
    DarkCyan = 0x008B8B,
    DarkGoldenrod = 0xB8860B,
    DarkGray = 0xA9A9A9,
    DarkGrey = 0xA9A9A9,
    DarkGreen = 0x006400,
    DarkKhaki = 0xBDB76B,
    DarkMagenta = 0x8B008B,
    DarkOliveGreen = 0x556B2F,
    DarkOrange = 0xFF8C00,
    DarkOrchid = 0x9932CC,
    DarkRed = 0x8B0000,
    DarkSalmon = 0xE9967A,
    DarkSeaGreen = 0x8FBC8F,
    DarkSlateBlue = 0x483D8B,
    DarkSlateGray = 0x2F4F4F,
    DarkSlateGrey = 0x2F4F4F,
    DarkTurquoise = 0x00CED1,
    DarkViolet = 0x9400D3,
    DeepPink = 0xFF1493,
    DeepSkyBlue = 0x00BFFF,
    DimGray = 0x696969,
    DimGrey = 0x696969,
    DodgerBlue = 0x1E90FF,
    FireBrick = 0xB22222,
    FloralWhite = 0xFFFAF0,
    ForestGreen = 0x228B22,
    Fuchsia = 0xFF00FF,
    Gainsboro = 0xDCDCDC,
    GhostWhite = 0xF8F8FF,
    Gold = 0xFFD700,
    Goldenrod = 0xDAA520,
    Gray = 0x808080,
    Grey = 0x808080,
    Green = 0x008000,
    GreenYellow = 0xADFF2F,
    Honeydew = 0xF0FFF0,
    HotPink = 0xFF69B4,
    IndianRed = 0xCD5C5C,
    Indigo = 0x4B0082,
    Ivory = 0xFFFFF0,
    Khaki = 0xF0E68C,
    Lavender = 0xE6E6FA,
    LavenderBlush = 0xFFF0F5,
    LawnGreen = 0x7CFC00,
    LemonChiffon = 0xFFFACD,
    LightBlue = 0xADD8E6,
    LightCoral = 0xF08080,
    LightCyan = 0xE0FFFF,
    LightGoldenrodYellow = 0xFAFAD2,
    LightGreen = 0x90EE90,
    LightGrey = 0xD3D3D3,
    LightPink = 0xFFB6C1,
    LightSalmon = 0xFFA07A,
    LightSeaGreen = 0x20B2AA,
    LightSkyBlue = 0x87CEFA,
    LightSlateGray = 0x778899,
    LightSlateGrey = 0x778899,
    LightSteelBlue = 0xB0C4DE,
    LightYellow = 0xFFFFE0,
    Lime = 0x00FF00,
    LimeGreen = 0x32CD32,
    Linen = 0xFAF0E6,
    Magenta = 0xFF00FF,
    Maroon = 0x800000,
    MediumAquamarine = 0x66CDAA,
    MediumBlue = 0x0000CD,
    MediumOrchid = 0xBA55D3,
    MediumPurple = 0x9370DB,
    MediumSeaGreen = 0x3CB371,
    MediumSlateBlue = 0x7B68EE,
    MediumSpringGreen = 0x00FA9A,
    MediumTurquoise = 0x48D1CC,
    MediumVioletRed = 0xC71585,
    MidnightBlue = 0x191970,
    MintCream = 0xF5FFFA,
    MistyRose = 0xFFE4E1,
    Moccasin = 0xFFE4B5,
    NavajoWhite = 0xFFDEAD,
    Navy = 0x000080,
    OldLace = 0xFDF5E6,
    Olive = 0x808000,
    OliveDrab = 0x6B8E23,
    Orange = 0xFFA500,
    OrangeRed = 0xFF4500,
    Orchid = 0xDA70D6,
    PaleGoldenrod = 0xEEE8AA,
    PaleGreen = 0x98FB98,
    PaleTurquoise = 0xAFEEEE,
    PaleVioletRed = 0xDB7093,
    PapayaWhip = 0xFFEFD5,
    PeachPuff = 0xFFDAB9,
    Peru = 0xCD853F,
    Pink = 0xFFC0CB,
    Plaid = 0xCC5533,
    Plum = 0xDDA0DD,
    PowderBlue = 0xB0E0E6,
    Purple = 0x800080,
    Red = 0xFF0000,
    RosyBrown = 0xBC8F8F,
    RoyalBlue = 0x4169E1,
    SaddleBrown = 0x8B4513,
    Salmon = 0xFA8072,
    SandyBrown = 0xF4A460,
    SeaGreen = 0x2E8B57,
    Seashell = 0xFFF5EE,
    Sienna = 0xA0522D,
    Silver = 0xC0C0C0,
    SkyBlue = 0x87CEEB,
    SlateBlue = 0x6A5ACD,
    SlateGray = 0x708090,
    SlateGrey = 0x708090,
    Snow = 0xFFFAFA,
    SpringGreen = 0x00FF7F,
    SteelBlue = 0x4682B4,
    Tan = 0xD2B48C,
    Teal = 0x008080,
    Thistle = 0xD8BFD8,
    Tomato = 0xFF6347,
    Turquoise = 0x40E0D0,
    Violet = 0xEE82EE,
    Wheat = 0xF5DEB3,
    White = 0xFFFFFF,
    WhiteSmoke = 0xF5F5F5,
    Yellow = 0xFFFF00,
    YellowGreen = 0x9ACD32,
    FairyLight = 0xFFE42D,
    FairyLightNCC = 0xFF9D2A,
  };
};

inline CRGB operator+(const CRGB &p1, const CRGB &p2) {
  return CRGB(qadd8(p1.r, p2.r), qadd8(p1.g, p2.g), qadd8(p1.b, p2.b));
}
inline CRGB operator-(const CRGB &p1, const CRGB &p2) {
  return CRGB(qsub8(p1.r, p2.r), qsub8(p1.g, p2.g), qsub8(p1.b, p2.b));
}
inline bool operator==(const CRGB &lhs, const CRGB &rhs) { return lhs.r == rhs.r && lhs.g == rhs.g && lhs.b == rhs.b; }
inline bool operator!=(const CRGB &lhs, const CRGB &rhs) { return !(lhs == rhs); }

struct CHSV32 {
  union {
    struct {
      uint16_t h;
      uint8_t s;
      uint8_t v;
    };
    uint32_t hsv32;
  };

  inline CHSV32() = default;
  inline CHSV32(uint16_t ih, uint8_t is, uint8_t iv) : h(ih), s(is), v(iv) {}
  inline CHSV32(uint8_t ih, uint8_t is, uint8_t iv) : h(static_cast<uint16_t>(ih) << 8), s(is), v(iv) {}
  inline CHSV32(const CHSV &chsv)  // NOLINT(google-explicit-constructor)
      : h(static_cast<uint16_t>(chsv.h) << 8), s(chsv.s), v(chsv.v) {}
  inline operator CHSV() const {  // NOLINT(google-explicit-constructor)
    return CHSV(static_cast<uint8_t>(h >> 8), s, v);
  }
  inline CHSV32(const CRGBW &rgb);  // NOLINT(google-explicit-constructor)
  inline CHSV32 &operator=(const CRGBW &rgb);
};

struct CRGBW {
  union {
    uint32_t color32;  // 0xWWRRGGBB
    struct {
      uint8_t b;
      uint8_t g;
      uint8_t r;
      uint8_t w;
    };
    uint8_t raw[4];  // B, G, R, W
  };

  inline CRGBW() = default;
  constexpr CRGBW(uint32_t color) : color32(color) {}  // NOLINT(google-explicit-constructor)
  constexpr CRGBW(uint8_t red, uint8_t green, uint8_t blue, uint8_t white = 0) : b(blue), g(green), r(red), w(white) {}
  constexpr CRGBW(CRGB rgb) : b(rgb.b), g(rgb.g), r(rgb.r), w(0) {}  // NOLINT(google-explicit-constructor)
  inline CRGBW(CHSV32 hsv) {                                         // NOLINT(google-explicit-constructor)
    hsv2rgb_rainbow(hsv.h, hsv.s, hsv.v, raw, true);
    w = 0;
  }
  inline CRGBW(CHSV hsv) {  // NOLINT(google-explicit-constructor)
    hsv2rgb_rainbow(static_cast<uint16_t>(hsv.h) << 8, hsv.s, hsv.v, raw, true);
    w = 0;
  }

  inline const uint8_t &operator[](uint8_t x) const { return raw[x]; }
  inline CRGBW &operator=(uint32_t color) {
    color32 = color;
    return *this;
  }
  inline CRGBW &operator=(CHSV32 hsv) {
    hsv2rgb_rainbow(hsv.h, hsv.s, hsv.v, raw, true);
    w = 0;
    return *this;
  }
  inline CRGBW &operator=(CHSV hsv) {
    hsv2rgb_rainbow(static_cast<uint16_t>(hsv.h) << 8, hsv.s, hsv.v, raw, true);
    w = 0;
    return *this;
  }
  inline CRGBW &operator=(const CRGB &rgb) {
    b = rgb.b;
    g = rgb.g;
    r = rgb.r;
    w = 0;
    return *this;
  }
  inline operator uint32_t() const { return color32; }  // NOLINT(google-explicit-constructor)
  inline void adjust_hue(int hueshift);
  inline uint8_t getAverageLight() const { return (r + g + b + w) >> 2; }
  inline uint8_t getRGBaverage() const { return ((r + g + b) * 21846) >> 16; }
};

class CRGBPalette16 {
 public:
  CRGB entries[16];

  CRGBPalette16() { memset(entries, 0, sizeof(entries)); }
  CRGBPalette16(const CRGBPalette16 &rhs) { memmove(&entries[0], &rhs.entries[0], sizeof(entries)); }
  CRGBPalette16 &operator=(const CRGBPalette16 &rhs) {
    memmove(&entries[0], &rhs.entries[0], sizeof(entries));
    return *this;
  }
  CRGBPalette16(const CRGB rhs[16]) {  // NOLINT(google-explicit-constructor)
    memmove(&entries[0], &rhs[0], sizeof(entries));
  }
  CRGBPalette16 &operator=(const CRGB rhs[16]) {
    memmove(&entries[0], &rhs[0], sizeof(entries));
    return *this;
  }
  CRGBPalette16(const TProgmemRGBPalette16 &rhs) { *this = rhs; }  // NOLINT(google-explicit-constructor)
  CRGBPalette16 &operator=(const TProgmemRGBPalette16 &rhs) {
    for (int i = 0; i < 16; ++i)
      entries[i] = rhs[i];
    return *this;
  }
  CRGBPalette16(const CRGB &c00, const CRGB &c01, const CRGB &c02, const CRGB &c03, const CRGB &c04, const CRGB &c05,
                const CRGB &c06, const CRGB &c07, const CRGB &c08, const CRGB &c09, const CRGB &c10, const CRGB &c11,
                const CRGB &c12, const CRGB &c13, const CRGB &c14, const CRGB &c15) {
    entries[0] = c00;
    entries[1] = c01;
    entries[2] = c02;
    entries[3] = c03;
    entries[4] = c04;
    entries[5] = c05;
    entries[6] = c06;
    entries[7] = c07;
    entries[8] = c08;
    entries[9] = c09;
    entries[10] = c10;
    entries[11] = c11;
    entries[12] = c12;
    entries[13] = c13;
    entries[14] = c14;
    entries[15] = c15;
  }
  explicit CRGBPalette16(const CRGB &c1);
  CRGBPalette16(const CRGB &c1, const CRGB &c2);
  CRGBPalette16(const CRGB &c1, const CRGB &c2, const CRGB &c3);
  CRGBPalette16(const CRGB &c1, const CRGB &c2, const CRGB &c3, const CRGB &c4);
  CRGBPalette16(const uint8_t *gradient_palette) {  // NOLINT(google-explicit-constructor)
    *this = gradient_palette;
  }
  CRGBPalette16 &operator=(const uint8_t *gradient_palette);

  bool operator==(const CRGBPalette16 &rhs) const { return memcmp(entries, rhs.entries, sizeof(entries)) == 0; }
  bool operator!=(const CRGBPalette16 &rhs) const { return !(*this == rhs); }
  inline CRGB &operator[](uint8_t x) { return entries[x]; }
  inline const CRGB &operator[](uint8_t x) const { return entries[x]; }
  inline CRGB &operator[](int x) { return entries[static_cast<uint8_t>(x)]; }
  inline const CRGB &operator[](int x) const { return entries[static_cast<uint8_t>(x)]; }
  operator CRGB *() { return &entries[0]; }  // NOLINT(google-explicit-constructor)
};

// --- colour maths ---------------------------------------------------------------

uint32_t color_blend(uint32_t color1, uint32_t color2, uint8_t blend);
inline uint32_t color_blend16(uint32_t c1, uint32_t c2, uint16_t b) { return color_blend(c1, c2, b >> 8); }
uint32_t color_add(uint32_t c1, uint32_t c2, bool preserve_cr = false);
uint32_t color_fade(uint32_t c1, uint8_t amount, bool video = false);
uint32_t ColorFromPalette(const CRGBPalette16 &pal, unsigned index, uint8_t brightness = 255,
                          TBlendType blend_type = LINEARBLEND);
void hsv2rgb_spectrum(const CHSV32 &hsv, CRGBW &rgb);
void hsv2rgb_spectrum(const CHSV &hsv, CRGB &rgb);
void rgb2hsv(const CRGBW &rgb, CHSV32 &hsv);
CHSV rgb2hsv(const CRGB c);
void adjust_color(CRGBW &rgb, int32_t hue_shift, int32_t sat_change, int32_t value_change);
CRGB HeatColor(uint8_t temperature);
void fill_solid_RGB(CRGB *colors, uint32_t num, const CRGB &c1);
void fill_gradient_RGB(CRGB *colors, uint32_t startpos, CRGB startcolor, uint32_t endpos, CRGB endcolor);
void fill_gradient_RGB(CRGB *colors, uint32_t num, const CRGB &c1, const CRGB &c2);
void fill_gradient_RGB(CRGB *colors, uint32_t num, const CRGB &c1, const CRGB &c2, const CRGB &c3);
void fill_gradient_RGB(CRGB *colors, uint32_t num, const CRGB &c1, const CRGB &c2, const CRGB &c3, const CRGB &c4);
void nblendPaletteTowardPalette(CRGBPalette16 &current, CRGBPalette16 &target, uint8_t max_changes);

// Fast colour scale: c * scale / 256 on all four channels, speed over accuracy.
inline uint32_t fast_color_scale(const uint32_t c, const uint8_t scale) {
  uint32_t rb = (((c & 0x00FF00FF) * scale) >> 8) & 0x00FF00FF;
  uint32_t wg = (((c >> 8) & 0x00FF00FF) * scale) & ~0x00FF00FFu;
  return rb | wg;
}

// WLED's global gamma is deliberately off in this engine: the ESPHome light layer
// owns gamma for strips and the display front end has its own option. These stay as
// identity so effect bodies that call them keep working unchanged.
inline uint8_t gamma8(uint8_t c) { return c; }
inline uint8_t gamma8inv(uint8_t c) { return c; }
inline uint32_t gamma32(uint32_t c) { return c; }
inline uint32_t gamma32inv(uint32_t c) { return c; }

inline CHSV32::CHSV32(const CRGBW &rgb) { rgb2hsv(rgb, *this); }
inline CHSV32 &CHSV32::operator=(const CRGBW &rgb) {
  rgb2hsv(rgb, *this);
  return *this;
}
inline void CRGBW::adjust_hue(int hueshift) {
  CHSV32 hsv = *this;
  hsv.h += hueshift << 8;
  hsv2rgb_spectrum(hsv, *this);
}
inline CRGBW hsv2rgb(const CHSV32 &hsv) { return CRGBW(hsv); }

}  // namespace wled_fx
}  // namespace esphome
