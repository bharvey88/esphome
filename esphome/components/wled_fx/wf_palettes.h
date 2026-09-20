#pragma once

/* Palette handling derived from WLED 16.0.1 wled00/palettes.cpp, wled00/colors.cpp
 * and Segment::loadPalette() in wled00/FX_fcn.cpp.
 * Copyright (c) 2016-present Christian Schwinne and individual WLED contributors.
 * Licensed under the EUPL v. 1.2 or later, distributed here under GPLv3 per the
 * EUPL Article 5 compatibility clause.
 */

#include <cstddef>
#include <cstdint>

#include "wf_color.h"

namespace esphome::wled_fx {

// Palette ID layout, identical to WLED:
//   0             "Default", resolves to the effect's own default palette
//   1             randomly generated, morphing
//   2 to 5        built from the segment colours
//   6 to 12       FastLED palettes
//   13 to 71      cpt-city gradient palettes
inline constexpr uint8_t DYNAMIC_PALETTE_COUNT = 6;

extern const TProgmemRGBPalette16 *const FASTLED_PALETTES[];
extern const uint8_t *const GRADIENT_PALETTES[];
extern const size_t FASTLED_PALETTE_COUNT;
extern const size_t GRADIENT_PALETTE_COUNT;

// Display names, in palette ID order. Taken from WLED's JSON_palette_names.
extern const char *const PALETTE_NAMES[];
size_t palette_count();

// Resolves a palette name to its ID. Returns -1 when unknown. Case insensitive.
int palette_id_by_name(const char *name);

// Fills target with palette pal. Colours 0 to 2 are the segment colours, used by
// the dynamic palettes 2 to 5. random_palette is the shared morphing palette.
void load_palette(CRGBPalette16 &target, uint8_t pal, const uint32_t colors[3], const CRGBPalette16 &random_palette);

CRGBPalette16 generate_random_palette();
CRGBPalette16 generate_harmonic_random_palette(const CRGBPalette16 &basepalette);

// Shared "Random Cycle" palette. Morphed once per frame by the front end.
class RandomPalette {
 public:
  void step(uint32_t now);
  const CRGBPalette16 &current() const { return this->current_; }

 protected:
  CRGBPalette16 current_{};
  CRGBPalette16 target_{};
  uint32_t last_change_{0};
  uint32_t next_blend_{0};
  bool started_{false};
};

}  // namespace esphome::wled_fx
