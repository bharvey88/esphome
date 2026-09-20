/* Derived from WLED 16.0.1 wled00/FX_fcn.cpp (Segment::loadPalette,
 * Segment::handleRandomPalette), wled00/colors.cpp (generateRandomPalette,
 * generateHarmonicRandomPalette) and the JSON_palette_names table.
 * Copyright (c) 2016-present Christian Schwinne and individual WLED contributors.
 * Licensed under the EUPL v. 1.2 or later, distributed here under GPLv3 per the
 * EUPL Article 5 compatibility clause.
 */

#include <cstring>
#include <utility>

#include "wf_palettes.h"

namespace esphome::wled_fx {

const char *const PALETTE_NAMES[] = {
    "Default",     "* Random Cycle", "* Color 1",     "* Colors 1&2", "* Color Gradient", "* Colors Only", "Party",
    "Cloud",       "Lava",           "Ocean",         "Forest",       "Rainbow",          "Rainbow Bands", "Sunset",
    "Rivendell",   "Breeze",         "Red & Blue",    "Yellowout",    "Analogous",        "Splash",        "Pastel",
    "Sunset 2",    "Beach",          "Vintage",       "Departure",    "Landscape",        "Beech",         "Sherbet",
    "Hult",        "Hult 64",        "Drywet",        "Jul",          "Grintage",         "Rewhi",         "Tertiary",
    "Fire",        "Icefire",        "Cyane",         "Light Pink",   "Autumn",           "Magenta",       "Magred",
    "Yelmag",      "Yelblu",         "Orange & Teal", "Tiamat",       "April Night",      "Orangery",      "C9",
    "Sakura",      "Aurora",         "Atlantica",     "C9 2",         "C9 New",           "Temperature",   "Aurora 2",
    "Retro Clown", "Candy",          "Toxy Reaf",     "Fairy Reaf",   "Semi Blue",        "Pink Candy",    "Red Reaf",
    "Aqua Flash",  "Yelblu Hot",     "Lite Light",    "Red Flash",    "Blink Red",        "Red Shift",     "Red Tide",
    "Candy2",      "Traffic Light",
};

size_t palette_count() { return DYNAMIC_PALETTE_COUNT + FASTLED_PALETTE_COUNT + GRADIENT_PALETTE_COUNT; }

namespace {

bool name_equals_ci(const char *a, const char *b) {
  while (*a && *b) {
    char ca = *a >= 'A' && *a <= 'Z' ? *a + 32 : *a;
    char cb = *b >= 'A' && *b <= 'Z' ? *b + 32 : *b;
    if (ca != cb)
      return false;
    a++;
    b++;
  }
  return *a == *b;
}

}  // namespace

int palette_id_by_name(const char *name) {
  if (name == nullptr)
    return -1;
  const size_t count = palette_count();
  for (size_t i = 0; i < count; i++) {
    if (name_equals_ci(PALETTE_NAMES[i], name))
      return static_cast<int>(i);
  }
  return -1;
}

CRGBPalette16 generate_random_palette() {
  return CRGBPalette16(CHSV(hw_random8(), hw_random8(160, 255), hw_random8(128, 255)),
                       CHSV(hw_random8(), hw_random8(160, 255), hw_random8(128, 255)),
                       CHSV(hw_random8(), hw_random8(160, 255), hw_random8(128, 255)),
                       CHSV(hw_random8(), hw_random8(160, 255), hw_random8(128, 255)));
}

CRGBPalette16 generate_harmonic_random_palette(const CRGBPalette16 &basepalette) {
  CHSV palettecolors[4];
  // The mask is a no-op on hw_random8(4) and makes the bound visible to the
  // analyzer, which cannot see across the translation unit to prove it.
  uint8_t keepcolorposition = hw_random8(4) & 0x03;
  palettecolors[keepcolorposition] = rgb2hsv(basepalette.entries[keepcolorposition * 5]);
  palettecolors[keepcolorposition].hue += hw_random8(10) - 5;

  for (int i = 0; i < 3; i++) {
    palettecolors[i].saturation = hw_random8(200, 255);
    palettecolors[i].value = hw_random8(220, 255);
  }
  palettecolors[3].saturation = hw_random8(20, 255);
  palettecolors[3].value = hw_random8(80, 255);

  for (int i = 3; i > 0; i--) {
    std::swap(palettecolors[i].saturation, palettecolors[hw_random8(i + 1)].saturation);
    std::swap(palettecolors[i].value, palettecolors[hw_random8(i + 1)].value);
  }

  uint8_t basehue = palettecolors[keepcolorposition].hue;
  uint8_t harmonics[3]{};
  uint8_t type = hw_random8(5);

  switch (type) {
    case 0:  // analogous
      harmonics[0] = basehue + hw_random8(30, 50);
      harmonics[1] = basehue + hw_random8(10, 30);
      harmonics[2] = basehue - hw_random8(10, 30);
      break;
    case 1:  // triadic
      harmonics[0] = basehue + 113 + hw_random8(15);
      harmonics[1] = basehue + 233 + hw_random8(15);
      harmonics[2] = basehue - 7 + hw_random8(15);
      break;
    case 2:  // split complementary
      harmonics[0] = basehue + 145 + hw_random8(10);
      harmonics[1] = basehue + 205 + hw_random8(10);
      harmonics[2] = basehue - 5 + hw_random8(10);
      break;
    case 3:  // square
      harmonics[0] = basehue + 85 + hw_random8(10);
      harmonics[1] = basehue + 175 + hw_random8(10);
      harmonics[2] = basehue + 265 + hw_random8(10);
      break;
    default:  // tetradic
      harmonics[0] = basehue + 80 + hw_random8(20);
      harmonics[1] = basehue + 170 + hw_random8(20);
      harmonics[2] = basehue - 15 + hw_random8(30);
      break;
  }

  if (hw_random8() < 128) {
    for (int i = 2; i > 0; i--)
      std::swap(harmonics[i], harmonics[hw_random8(i + 1) % 3]);
  }

  int j = 0;
  for (int i = 0; i < 4; i++) {
    if (i == static_cast<int>(keepcolorposition))
      continue;
    palettecolors[i].hue = harmonics[j];
    j++;
  }

  bool makepastelpalette = hw_random8() < 25;

  CRGB rgb_palette_colors[4];
  for (int i = 0; i < 4; i++) {
    if (makepastelpalette && palettecolors[i].saturation > 180)
      palettecolors[i].saturation -= 160;
    rgb_palette_colors[i] = CRGB(palettecolors[i]);
  }

  return CRGBPalette16(rgb_palette_colors[0], rgb_palette_colors[1], rgb_palette_colors[2], rgb_palette_colors[3]);
}

void load_palette(CRGBPalette16 &target, uint8_t pal, const uint32_t colors[3], const CRGBPalette16 &random_palette) {
  const size_t fixed_count = DYNAMIC_PALETTE_COUNT + FASTLED_PALETTE_COUNT + GRADIENT_PALETTE_COUNT;
  if (pal >= fixed_count)
    pal = 0;
  switch (pal) {
    case 0:
      target = *FASTLED_PALETTES[0];  // Party, WLED's fallback default
      break;
    case 1:
      target = random_palette;
      break;
    case 2: {
      CRGB prim = colors[0];
      target = CRGBPalette16(prim);
      break;
    }
    case 3: {
      CRGB prim = colors[0];
      CRGB sec = colors[1];
      target = CRGBPalette16(prim, prim, sec, sec);
      break;
    }
    case 4: {
      CRGB prim = colors[0];
      CRGB sec = colors[1];
      CRGB ter = colors[2];
      target = CRGBPalette16(ter, sec, prim);
      break;
    }
    case 5: {
      CRGB prim = colors[0];
      CRGB sec = colors[1];
      if (colors[2]) {
        CRGB ter = colors[2];
        target = CRGBPalette16(prim, prim, prim, prim, prim, sec, sec, sec, sec, sec, ter, ter, ter, ter, ter, prim);
      } else {
        target = CRGBPalette16(prim, prim, prim, prim, prim, prim, prim, prim, sec, sec, sec, sec, sec, sec, sec, sec);
      }
      break;
    }
    default:
      if (pal < DYNAMIC_PALETTE_COUNT + FASTLED_PALETTE_COUNT) {
        target = *FASTLED_PALETTES[pal - DYNAMIC_PALETTE_COUNT];
      } else {
        target = GRADIENT_PALETTES[pal - (DYNAMIC_PALETTE_COUNT + FASTLED_PALETTE_COUNT)];
      }
      break;
  }
}

void RandomPalette::step(uint32_t now) {
  constexpr uint32_t change_interval_ms = 5000;
  if (!this->started_) {
    this->current_ = generate_random_palette();
    this->target_ = generate_random_palette();
    this->last_change_ = now;
    this->started_ = true;
    return;
  }
  if (now - this->last_change_ > change_interval_ms) {
    this->target_ = generate_harmonic_random_palette(this->current_);
    this->last_change_ = now;
  }
  nblendPaletteTowardPalette(this->current_, this->target_, 48);
}

}  // namespace esphome::wled_fx
