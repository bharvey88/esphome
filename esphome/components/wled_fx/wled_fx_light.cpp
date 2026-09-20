#include "wled_fx_light.h"

#include "esphome/core/application.h"
#include "esphome/core/log.h"
#include "wf_platform_esphome.h"

namespace esphome::wled_fx {

static const char *const TAG = "wled_fx";

void WledFxLightEffect::init() {
  install_platform();

  auto *out = static_cast<light::AddressableLight *>(this->state_->get_output());
  const int size = out->size();

  int width = this->width_;
  int height = this->height_;
  if (width <= 0 && height <= 0) {
    width = size;
    height = 1;
  } else if (width <= 0) {
    width = height > 0 ? size / height : size;
  } else if (height <= 0) {
    height = width > 0 ? size / width : 1;
  }
  if (static_cast<int64_t>(width) * height > size) {
    ESP_LOGW(TAG, "%dx%d is larger than the light (%d pixels), clamping", width, height, size);
    height = size / width;
  }
  if (width <= 0 || height <= 0) {
    ESP_LOGE(TAG, "Bad geometry %dx%d", width, height);
    return;
  }

  if (!this->engine_.init(static_cast<uint16_t>(width), static_cast<uint16_t>(height))) {
    ESP_LOGE(TAG, "Canvas allocation failed for %dx%d", width, height);
    return;
  }
  this->width_ = width;
  this->height_ = height;
  this->allocated_ = true;
}

void WledFxLightEffect::start() {
  this->ready_ = this->allocated_;
  this->last_frame_ = 0;
}

void WledFxLightEffect::stop() {
  light::AddressableLightEffect::stop();
  this->ready_ = false;
}

void WledFxLightEffect::apply(light::AddressableLight &it, const Color &current_color) {
  if (!this->ready_)
    return;
  const uint32_t now = App.get_loop_component_start_time();
  if (this->last_frame_ != 0 && now - this->last_frame_ < this->frame_interval_)
    return;
  this->last_frame_ = now;

  if (this->use_light_color_) {
    this->engine_.set_primary_color(RGBW32(current_color.r, current_color.g, current_color.b, current_color.w));
  }

  this->engine_.render(now);

  const Canvas &canvas = this->engine_.canvas();
  const int32_t size = it.size();
  for (int y = 0; y < this->height_; y++) {
    const bool flip = this->serpentine_ && (y & 1);
    for (int x = 0; x < this->width_; x++) {
      const uint32_t c = canvas.get(static_cast<size_t>(x) + static_cast<size_t>(y) * this->width_);
      const int column = flip ? (this->width_ - 1 - x) : x;
      const int32_t index = y * this->width_ + column;
      if (index >= size)
        continue;
      it[index].set_rgbw((c >> 16) & 0xFF, (c >> 8) & 0xFF, c & 0xFF, (c >> 24) & 0xFF);
    }
  }
  it.schedule_show();
}

}  // namespace esphome::wled_fx
