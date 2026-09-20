/* Bodies of the shared WLED 16.0.1 base functions that more than one effect
 * translation unit calls. Ported from wled00/FX.cpp with the mechanical transform
 * described in PORTING.md.
 *
 * Copyright (c) 2016 Harm Aldick.
 * Copyright (c) 2016-present Christian Schwinne and individual WLED contributors.
 * Licensed under the EUPL v. 1.2 or later, distributed here under GPLv3 per the
 * EUPL Article 5 compatibility clause.
 *
 * Per-effect credits are kept on the code they belong to.
 */

#include "wf_effects.h"

namespace esphome {
namespace wled_fx {

// WLED util.cpp:727
Prng &fx_prng() {
  static Prng prng(hw_random());
  return prng;
}

// WLED FX.cpp:102
void blink(Segment &seg, uint32_t color1, uint32_t color2, bool strobe, bool do_palette) {
  const unsigned seg_len = seg.length();
  uint32_t cycleTime = (255 - seg.speed) * 20;
  uint32_t onTime = FRAMETIME;
  if (!strobe)
    onTime += ((cycleTime * seg.intensity) >> 8);
  cycleTime += FRAMETIME * 2;
  uint32_t it = seg.now / cycleTime;
  uint32_t rem = seg.now % cycleTime;

  bool on = false;
  if (it != seg.step  // new iteration, force on state for one frame, even if set time is too brief
      || rem <= onTime) {
    on = true;
  }

  seg.step = it;  // save previous iteration

  uint32_t color = on ? color1 : color2;
  if (color == color1 && do_palette) {
    for (unsigned i = 0; i < seg_len; i++) {
      seg.set_pixel_color(i, seg.color_from_palette(i, true, seg.palette_solid_wrap(), 0));
    }
  } else {
    seg.fill(color);
  }
}

///////////////////////
//   * GRAVCENTER    //
///////////////////////
// Gravcenter effects By Andrew Tuline.
// Gravcenter base function for Gravcenter (0), Gravcentric (1), Gravimeter (2), Gravfreq (3) (merged by @dedehai)

void mode_colorwaves_pride_base(Segment &seg, bool isPride2015) {
  const unsigned seg_len = seg.length();
  unsigned duration = 10 + seg.speed;
  unsigned sPseudotime = seg.step;
  unsigned sHue16 = seg.aux0;

  uint8_t sat8 = isPride2015 ? beatsin88_t(87, 220, 250, seg.now) : 255;
  unsigned brightdepth = beatsin88_t(341, 96, 224, seg.now);
  unsigned brightnessthetainc16 = beatsin88_t(203, (25 * 256), (40 * 256), seg.now);
  unsigned msmultiplier = beatsin88_t(147, 23, 60, seg.now);

  unsigned hue16 = sHue16;
  unsigned hueinc16 =
      isPride2015 ? beatsin88_t(113, 1, 3000, seg.now) : beatsin88_t(113, 60, 300, seg.now) * seg.intensity * 10 / 255;

  sPseudotime += duration * msmultiplier;
  sHue16 += duration * beatsin88_t(400, 5, 9, seg.now);
  unsigned brightnesstheta16 = sPseudotime;

  for (unsigned i = 0; i < seg_len; i++) {
    hue16 += hueinc16;
    uint8_t hue8;

    if (isPride2015) {
      hue8 = hue16 >> 8;
    } else {
      unsigned h16_128 = hue16 >> 7;
      hue8 = (h16_128 & 0x100) ? (255 - (h16_128 >> 1)) : (h16_128 >> 1);
    }

    brightnesstheta16 += brightnessthetainc16;
    unsigned b16 = sin16_t(brightnesstheta16) + 32768;
    unsigned bri16 = (uint32_t) ((uint32_t) b16 * (uint32_t) b16) / 65536;
    uint8_t bri8 = (uint32_t) (((uint32_t) bri16) * brightdepth) / 65536;
    bri8 += (255 - brightdepth);

    if (isPride2015) {
      CRGBW newcolor = CRGB(CHSV(hue8, sat8, bri8));
      newcolor.color32 = gamma32inv(newcolor.color32);
      seg.blend_pixel_color(i, newcolor, 64);
    } else {
      seg.blend_pixel_color(i, seg.color_from_palette(hue8, false, seg.palette_solid_wrap(), 0, bri8), 128);
    }
  }

  seg.step = sPseudotime;
  seg.aux0 = sHue16;
}

}  // namespace wled_fx
}  // namespace esphome
