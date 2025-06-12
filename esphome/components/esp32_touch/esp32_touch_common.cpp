#ifdef USE_ESP32

#include "esp32_touch.h"
#include "esphome/core/log.h"
#include <cinttypes>

namespace esphome {
namespace esp32_touch {

static const char *const TAG = "esp32_touch";

void ESP32TouchComponent::dump_config_base_() {
  const char *lv_s = get_low_voltage_reference_str(this->low_voltage_reference_);
  const char *hv_s = get_high_voltage_reference_str(this->high_voltage_reference_);
  const char *atten_s = get_voltage_attenuation_str(this->voltage_attenuation_);

  ESP_LOGCONFIG(TAG,
                "Config for ESP32 Touch Hub:\n"
                "  Meas cycle: %.2fms\n"
                "  Sleep cycle: %.2fms\n"
                "  Low Voltage Reference: %s\n"
                "  High Voltage Reference: %s\n"
                "  Voltage Attenuation: %s",
                this->meas_cycle_ / (8000000.0f / 1000.0f), this->sleep_cycle_ / (150000.0f / 1000.0f), lv_s, hv_s,
                atten_s);
}

void ESP32TouchComponent::dump_config_sensors_() {
  for (auto *child : this->children_) {
    LOG_BINARY_SENSOR("  ", "Touch Pad", child);
    ESP_LOGCONFIG(TAG, "    Pad: T%" PRIu32, (uint32_t) child->get_touch_pad());
    ESP_LOGCONFIG(TAG, "    Threshold: %" PRIu32, child->get_threshold());
  }
}

bool ESP32TouchComponent::create_touch_queue_() {
  // Queue size calculation: children * 4 allows for burst scenarios where ISR
  // fires multiple times before main loop processes.
  size_t queue_size = this->children_.size() * 4;
  if (queue_size < 8)
    queue_size = 8;

#ifdef USE_ESP32_VARIANT_ESP32
  this->touch_queue_ = xQueueCreate(queue_size, sizeof(TouchPadEventV1));
#else
  this->touch_queue_ = xQueueCreate(queue_size, sizeof(TouchPadEventV2));
#endif

  if (this->touch_queue_ == nullptr) {
    ESP_LOGE(TAG, "Failed to create touch event queue of size %d", queue_size);
    this->mark_failed();
    return false;
  }
  return true;
}

void ESP32TouchComponent::cleanup_touch_queue_() {
  if (this->touch_queue_) {
    vQueueDelete(this->touch_queue_);
    this->touch_queue_ = nullptr;
  }
}

void ESP32TouchComponent::configure_wakeup_pads_() {
  bool is_wakeup_source = false;

  // Check if any pad is configured for wakeup
  for (auto *child : this->children_) {
    if (child->get_wakeup_threshold() != 0) {
      is_wakeup_source = true;

#ifdef USE_ESP32_VARIANT_ESP32
      // ESP32 v1: No filter available when using as wake-up source.
      touch_pad_config(child->get_touch_pad(), child->get_wakeup_threshold());
#else
      // ESP32-S2/S3 v2: Set threshold for wakeup
      touch_pad_set_thresh(child->get_touch_pad(), child->get_wakeup_threshold());
#endif
    }
  }

  if (!is_wakeup_source) {
    // If no pad is configured for wakeup, deinitialize touch pad
    touch_pad_deinit();
  }
}

}  // namespace esp32_touch
}  // namespace esphome

#endif  // USE_ESP32
