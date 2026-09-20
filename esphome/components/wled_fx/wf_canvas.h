#pragma once

/* The canvas replaces WLED's per-segment pixel buffer (wled00/FX.h Segment::pixels).
 * It is allocated once at setup and never resized while an effect is running, which
 * is what lets the effect bodies keep WLED's lossless read-back behaviour on every
 * output type.
 *
 * Copyright (c) 2016-present Christian Schwinne and individual WLED contributors
 * for the parts derived from WLED 16.0.1.
 * Licensed under the EUPL v. 1.2 or later, distributed here under GPLv3 per the
 * EUPL Article 5 compatibility clause.
 */

#include <cstddef>
#include <cstdint>

#include "wf_platform.h"

namespace esphome {
namespace wled_fx {

// Words of sentinel written either side of the framebuffer. The host simulator
// checks them after every frame; on device they are simply unused headroom.
inline constexpr size_t CANVAS_GUARD_WORDS = 8;
inline constexpr uint32_t CANVAS_GUARD_PATTERN = 0xA5C3F00Du;

class Canvas {
 public:
  ~Canvas() { this->release(); }

  // One-shot allocation. Returns false when the allocation failed.
  bool allocate(uint16_t width, uint16_t height);
  void release();

  uint16_t width() const { return this->width_; }
  uint16_t height() const { return this->height_; }
  size_t size() const { return static_cast<size_t>(this->width_) * this->height_; }
  bool is_allocated() const { return this->pixels_ != nullptr; }

  uint32_t *pixels() { return this->pixels_; }
  const uint32_t *pixels() const { return this->pixels_; }

  uint32_t get(size_t i) const { return this->pixels_[i]; }
  void set(size_t i, uint32_t c) { this->pixels_[i] = c; }

  void clear();

  // True while both guard bands still hold the sentinel pattern.
  bool guards_intact() const;

 protected:
  uint32_t *buffer_{nullptr};  // allocation base, guard band first
  uint32_t *pixels_{nullptr};  // buffer_ + CANVAS_GUARD_WORDS
  uint16_t width_{0};
  uint16_t height_{0};
};

}  // namespace wled_fx
}  // namespace esphome
