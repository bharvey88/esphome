#pragma once

/* Helpers that WLED 16.0.1 keeps as file-scope code in wled00/FX.cpp, wled00/FX.h
 * and wled00/util.cpp, and that more than one effect translation unit needs.
 *
 * Copyright (c) 2016 Harm Aldick.
 * Copyright (c) 2016-present Christian Schwinne and individual WLED contributors.
 * Licensed under the EUPL v. 1.2 or later, distributed here under GPLv3 per the
 * EUPL Article 5 compatibility clause.
 *
 * Everything here is declared with external linkage on purpose. A `const` object
 * at namespace scope has internal linkage in C++, and an effect file that defines
 * its own copy of a shared helper cannot be diffed against upstream any more.
 * See PORTING.md section 6.
 */

#include <cstdint>

#include "wf_math.h"
#include "wf_segment.h"

namespace esphome::wled_fx {

/* WLED FX.cpp:87 keeps one file-static pseudo random generator that every effect
 * shares, so that saving and restoring its seed inside an effect leaves the same
 * sequence for the next one. Here it lives behind an accessor, so the hardware
 * random number generator is not read during static initialisation, before the
 * platform is up. */
Prng &fx_prng();

/* WLED FX.cpp:195. Blink and strobe base: alternate between color1 and color2,
 * and if strobe is set make it a strobe rather than an even blink. */
void blink(Segment &seg, uint32_t color1, uint32_t color2, bool strobe, bool do_palette);

// WLED FX.cpp:1948. Combined function from the original pride and colorwaves, by
// Mark Kriegsman.
void mode_colorwaves_pride_base(Segment &seg, bool is_pride_2015);

}  // namespace esphome::wled_fx
