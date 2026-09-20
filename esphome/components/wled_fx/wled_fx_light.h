#pragma once

#include "esphome/components/light/addressable_light.h"
#include "esphome/components/light/addressable_light_effect.h"
#include "wf_engine.h"

namespace esphome::wled_fx {

/* Addressable light front end. The canvas is width x height; for a plain strip
 * that is size() x 1. A matrix wired as one strip is described with width, height
 * and serpentine, the same vocabulary the addressable_light display platform uses.
 *
 * The canvas is allocated in init(), which the light calls once while it is
 * setting up, and kept for the lifetime of the effect object. The only later
 * allocation is the effect's own scratch data, which WLED's model sizes from the
 * effect and so cannot be sized before one is chosen; it is allocated on effect
 * change and never per frame. */
class WledFxLightEffect : public light::AddressableLightEffect {
 public:
  explicit WledFxLightEffect(const char *name) : light::AddressableLightEffect(name) {}

  Engine &engine() { return this->engine_; }

  void set_dimensions(int width, int height) {
    this->width_ = width;
    this->height_ = height;
  }
  void set_serpentine(bool serpentine) { this->serpentine_ = serpentine; }
  void set_frame_interval(uint32_t interval_ms) { this->frame_interval_ = interval_ms; }
  void set_use_light_color(bool use) { this->use_light_color_ = use; }

  void init() override;
  void start() override;
  void stop() override;
  void apply(light::AddressableLight &it, const Color &current_color) override;

 protected:
  Engine engine_;
  int width_{0};
  int height_{0};
  bool serpentine_{false};
  bool use_light_color_{true};
  // WLED's FRAMETIME at its default WLED_FPS of 42. Kept in step with the schema
  // default in __init__.py.
  uint32_t frame_interval_{1000 / 42};
  uint32_t last_frame_{0};
  bool allocated_{false};
  bool ready_{false};
};

}  // namespace esphome::wled_fx
