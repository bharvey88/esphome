#pragma once

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"

#ifdef USE_ESP32

namespace esphome {
namespace esp32_hosted {

class ESP32Hosted : public Component {
 public:
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::HARDWARE; }

 protected:
#ifdef USE_ESP32_BLE
  void init_bluetooth_();
#endif
};

}  // namespace esp32_hosted
}  // namespace esphome

#endif  // USE_ESP32