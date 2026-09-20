#pragma once

/* Shared prologue for every effect translation unit. Effect files include this and
 * nothing else from the engine. See PORTING.md for the transform rules. */

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "wf_color.h"
#include "wf_fx_shared.h"
#include "wf_math.h"
#include "wf_palettes.h"
#include "wf_registry.h"
#include "wf_segment.h"

/* Compile-time effect selection, all of it set by codegen as build flags.
 *
 * WLED_FX_DEFAULT_ENABLE is 1 when the YAML asks for every effect, which is the
 * default, and 0 when it gives an `effects:` allow-list. Each allowed effect then
 * gets its own WLED_FX_FX_<NAME>=1. An undefined macro evaluates to 0 inside #if,
 * so the per-effect guards need no `defined()`. */

namespace esphome::wled_fx {

// Paints the primary color over the whole canvas, which is what WLED falls back
// to when an effect cannot run on the current geometry.
inline void fill_with_primary(Segment &seg) { seg.fill(seg.color(0)); }

}  // namespace esphome::wled_fx

/* WLED's early-out for that case. It has to be a macro because it returns from
 * the effect, and upstream spells it the same way, which keeps effect bodies
 * comparable with FX.cpp. */
#define FX_FALLBACK_STATIC \
  { \
    fill_with_primary(seg); \
    return; \
  }
