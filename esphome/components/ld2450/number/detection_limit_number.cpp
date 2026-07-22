#include "detection_limit_number.h"

namespace esphome::ld2450 {

DetectionLimitNumber::DetectionLimitNumber(uint8_t index) : index_(index) {}

void DetectionLimitNumber::control(float value) {
  this->publish_state(value);
  this->parent_->set_detection_limit(this->index_);
}

}  // namespace esphome::ld2450
