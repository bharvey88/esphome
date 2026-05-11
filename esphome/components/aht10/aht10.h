#pragma once

#include <utility>

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome::aht10 {

enum AHT10Variant { AHT10, AHT20 };

class AHT10Component : public PollingComponent, public i2c::I2CDevice {
 public:
  void setup() override;
  void update() override;
  void dump_config() override;
  void set_variant(AHT10Variant variant) { this->variant_ = variant; }

  void set_temperature_sensor(sensor::Sensor *temperature_sensor) { temperature_sensor_ = temperature_sensor; }
  void set_humidity_sensor(sensor::Sensor *humidity_sensor) { humidity_sensor_ = humidity_sensor; }

 protected:
  sensor::Sensor *temperature_sensor_{nullptr};
  sensor::Sensor *humidity_sensor_{nullptr};
  AHT10Variant variant_{};
  unsigned read_count_{};
  void read_data_();
  void restart_read_();
  bool try_init_();
  uint32_t start_time_{};
  uint8_t init_retries_remaining_{0};
  bool init_complete_{false};
};

}  // namespace esphome::aht10
