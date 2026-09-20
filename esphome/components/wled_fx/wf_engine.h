#pragma once

/* Ties the canvas, the segment, the registry and the shared random palette
 * together. Both front ends and the host simulator drive the engine through this
 * one class; nothing else needs to know how an effect is invoked. */

#include <cstdint>

#include "wf_canvas.h"
#include "wf_registry.h"
#include "wf_segment.h"

namespace esphome {
namespace wled_fx {

// Which segment fields the user pinned in YAML or at runtime. Anything not pinned
// is refilled from the effect's own metadata defaults when the effect changes.
enum ControlOverride : uint16_t {
  OVERRIDE_SPEED = 1 << 0,
  OVERRIDE_INTENSITY = 1 << 1,
  OVERRIDE_CUSTOM1 = 1 << 2,
  OVERRIDE_CUSTOM2 = 1 << 3,
  OVERRIDE_CUSTOM3 = 1 << 4,
  OVERRIDE_CHECK1 = 1 << 5,
  OVERRIDE_CHECK2 = 1 << 6,
  OVERRIDE_CHECK3 = 1 << 7,
  OVERRIDE_PALETTE = 1 << 8,
};

class Engine {
 public:
  // Allocates the canvas. Call once from setup(). Returns false on failure.
  bool init(uint16_t width, uint16_t height);

  Canvas &canvas() { return this->canvas_; }
  Segment &segment() { return this->seg_; }

  const EffectInfo *effect() const { return this->effect_; }
  // Selects by display name. Returns false and leaves the effect alone when the
  // name is not registered (or was not compiled in).
  bool set_effect(const char *name);
  bool set_effect_index(size_t index);
  size_t effect_index() const { return this->effect_index_; }

  // Pinned controls. Setting one both applies it now and keeps it across effect
  // changes.
  void set_speed(uint8_t v);
  void set_intensity(uint8_t v);
  void set_custom1(uint8_t v);
  void set_custom2(uint8_t v);
  void set_custom3(uint8_t v);
  void set_check1(bool v);
  void set_check2(bool v);
  void set_check3(bool v);
  void set_palette(uint8_t v);
  bool set_palette_by_name(const char *name);
  void clear_override(uint16_t flag) { this->overrides_ &= ~flag; }

  void set_primary_color(uint32_t c) { this->seg_.colors[0] = c; }

  // Runs one frame at timestamp now (milliseconds).
  void render(uint32_t now);

 protected:
  void apply_effect_defaults_();

  Canvas canvas_;
  Segment seg_;
  RandomPalette random_palette_;
  const EffectInfo *effect_{nullptr};
  size_t effect_index_{0};
  uint16_t overrides_{0};
};

}  // namespace wled_fx
}  // namespace esphome
