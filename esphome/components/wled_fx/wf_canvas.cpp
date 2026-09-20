#include "wf_canvas.h"

#include <cstring>

namespace esphome::wled_fx {

bool Canvas::allocate(uint16_t width, uint16_t height) {
  this->release();
  if (width == 0 || height == 0)
    return false;
  const size_t pixel_count = static_cast<size_t>(width) * height;
  const size_t words = pixel_count + 2 * CANVAS_GUARD_WORDS;
  this->buffer_ = static_cast<uint32_t *>(platform_alloc(words * sizeof(uint32_t)));
  if (this->buffer_ == nullptr)
    return false;
  this->pixels_ = this->buffer_ + CANVAS_GUARD_WORDS;
  this->width_ = width;
  this->height_ = height;
  for (size_t i = 0; i < CANVAS_GUARD_WORDS; i++) {
    this->buffer_[i] = CANVAS_GUARD_PATTERN;
    this->pixels_[pixel_count + i] = CANVAS_GUARD_PATTERN;
  }
  this->clear();
  return true;
}

void Canvas::release() {
  if (this->buffer_ != nullptr) {
    platform_free(this->buffer_);
    this->buffer_ = nullptr;
    this->pixels_ = nullptr;
  }
  this->width_ = 0;
  this->height_ = 0;
}

void Canvas::clear() {
  if (this->pixels_ == nullptr)
    return;
  memset(this->pixels_, 0, this->size() * sizeof(uint32_t));
}

bool Canvas::guards_intact() const {
  if (this->buffer_ == nullptr)
    return true;
  const size_t pixel_count = this->size();
  for (size_t i = 0; i < CANVAS_GUARD_WORDS; i++) {
    if (this->buffer_[i] != CANVAS_GUARD_PATTERN)
      return false;
    if (this->pixels_[pixel_count + i] != CANVAS_GUARD_PATTERN)
      return false;
  }
  return true;
}

}  // namespace esphome::wled_fx
