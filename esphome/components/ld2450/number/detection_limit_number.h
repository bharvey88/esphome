#pragma once

#include "esphome/components/number/number.h"
#include "../ld2450.h"

namespace esphome::ld2450 {

class DetectionLimitNumber : public number::Number, public Parented<LD2450Component> {
 public:
  DetectionLimitNumber(uint8_t index);

 protected:
  uint8_t index_;
  void control(float value) override;
};

}  // namespace esphome::ld2450
