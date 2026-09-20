#pragma once

/* Effect registry. Effects are keyed by NAME, not by the WLED numeric ID, because
 * WLED and WLED-MM assign different IDs to the same effect.
 *
 * The metadata string parsing mirrors extractModeName() and extractModeDefaults()
 * in WLED 16.0.1 wled00/util.cpp.
 * Copyright (c) 2016-present Christian Schwinne and individual WLED contributors.
 * Licensed under the EUPL v. 1.2 or later, distributed here under GPLv3 per the
 * EUPL Article 5 compatibility clause.
 */

#include <cstddef>
#include <cstdint>

#include "wf_segment.h"

namespace esphome::wled_fx {

using EffectFn = void (*)(Segment &seg);

// Dimensionality and audio flags, parsed from the 4th group of the WLED metadata
// string. An empty or missing group means 1D only.
enum EffectFlag : uint8_t {
  EFFECT_FLAG_0D = 1 << 0,
  EFFECT_FLAG_1D = 1 << 1,
  EFFECT_FLAG_2D = 1 << 2,
  EFFECT_FLAG_VOLUME = 1 << 3,
  EFFECT_FLAG_FFT = 1 << 4,
};

// One registered effect. `metadata` is the WLED metadata string copied verbatim,
// which is what carries the display name, the slider labels and the defaults.
struct EffectInfo {
  const char *metadata;
  EffectFn fn;
};

// Segment field defaults for an effect, read out of the metadata string.
struct EffectDefaults {
  uint8_t speed{128};
  uint8_t intensity{128};
  uint8_t custom1{128};
  uint8_t custom2{128};
  uint8_t custom3{16};
  bool check1{false};
  bool check2{false};
  bool check3{false};
  uint8_t palette{0};
  uint8_t map1d2d{M12_PIXELS};
  uint8_t flags{EFFECT_FLAG_1D};
};

// Copies the display name (everything before the first '@') into dest.
size_t effect_name(const EffectInfo &info, char *dest, size_t dest_size);
// Case insensitive comparison against the display name.
bool effect_name_equals(const EffectInfo &info, const char *name);
EffectDefaults effect_defaults(const EffectInfo &info);

/* One effect translation unit's table. Declare exactly one of these per effect
 * file, at namespace scope and with external linkage:
 *
 *   extern const EffectGroup EFFECT_GROUP_1D_A;
 *   const EffectGroup EFFECT_GROUP_1D_A{"1d_a", ENTRIES, std::size(ENTRIES)};
 *
 * The extern matters: a plain `const` at namespace scope has internal linkage, and
 * the group would never be visible to the table below. */
struct EffectGroup {
  const char *group_name;
  const EffectInfo *entries;
  size_t count;
};

/* The list of groups that were compiled in. It is emitted by codegen into the
 * generated main.cpp (and by the host simulator's CMake into groups_generated.cpp)
 * rather than assembled by static initialisers, because ESPHome links the
 * generated sources as a static library: a translation unit nothing names is never
 * pulled out of the archive, so a self-registering object in it would never run. */
extern const EffectGroup *const LINKED_EFFECT_GROUPS[];
extern const unsigned LINKED_EFFECT_GROUP_COUNT;

class EffectRegistry {
 public:
  static size_t count();
  static const EffectInfo *at(size_t index);
  static const EffectInfo *find(const char *name);
  static int index_of(const char *name);
};

}  // namespace esphome::wled_fx
