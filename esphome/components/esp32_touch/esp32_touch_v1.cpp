#ifdef USE_ESP32_VARIANT_ESP32

#include "esp32_touch.h"
#include "esphome/core/application.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"

#include <algorithm>
#include <cinttypes>

// Include HAL for ISR-safe touch reading
#include "hal/touch_sensor_ll.h"
// Include for RTC clock frequency
#include "soc/rtc.h"

namespace esphome {
namespace esp32_touch {

static const char *const TAG = "esp32_touch";

void ESP32TouchComponent::setup() {
  // Create queue for touch events
  // Queue size calculation: children * 4 allows for burst scenarios where ISR
  // fires multiple times before main loop processes. This is important because
  // ESP32 v1 scans all pads on each interrupt, potentially sending multiple events.
  if (!this->create_touch_queue_()) {
    return;
  }

  touch_pad_init();
  touch_pad_set_fsm_mode(TOUCH_FSM_MODE_TIMER);

  // Set up IIR filter if enabled
  if (this->iir_filter_enabled_()) {
    touch_pad_filter_start(this->iir_filter_);
  }

  // Configure measurement parameters
#if ESP_IDF_VERSION_MAJOR >= 5
  touch_pad_set_measurement_clock_cycles(this->meas_cycle_);
  touch_pad_set_measurement_interval(this->sleep_cycle_);
#else
  touch_pad_set_meas_time(this->sleep_cycle_, this->meas_cycle_);
#endif
  touch_pad_set_voltage(this->high_voltage_reference_, this->low_voltage_reference_, this->voltage_attenuation_);

  // Configure each touch pad
  for (auto *child : this->children_) {
    touch_pad_config(child->get_touch_pad(), child->get_threshold());
  }

  // Register ISR handler
  esp_err_t err = touch_pad_isr_register(touch_isr_handler, this);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to register touch ISR: %s", esp_err_to_name(err));
    this->cleanup_touch_queue_();
    this->mark_failed();
    return;
  }

  // Calculate release timeout based on sleep cycle
  // Design note: ESP32 v1 hardware limitation - interrupts only fire on touch (not release)
  // We must use timeout-based detection for release events
  // Formula: 3 sleep cycles converted to ms, with MINIMUM_RELEASE_TIME_MS minimum
  // The division by 2 accounts for the fact that sleep_cycle is in half-cycles
  uint32_t rtc_freq = rtc_clk_slow_freq_get_hz();
  this->release_timeout_ms_ = (this->sleep_cycle_ * 1000 * 3) / (rtc_freq * 2);
  if (this->release_timeout_ms_ < MINIMUM_RELEASE_TIME_MS) {
    this->release_timeout_ms_ = MINIMUM_RELEASE_TIME_MS;
  }
  // Check for releases at 1/4 the timeout interval, capped at 50ms
  this->release_check_interval_ms_ = std::min(this->release_timeout_ms_ / 4, (uint32_t) 50);

  // Enable touch pad interrupt
  touch_pad_intr_enable();
}

void ESP32TouchComponent::dump_config() {
  this->dump_config_base_();

  if (this->iir_filter_enabled_()) {
    ESP_LOGCONFIG(TAG, "    IIR Filter: %" PRIu32 "ms", this->iir_filter_);
  } else {
    ESP_LOGCONFIG(TAG, "  IIR Filter DISABLED");
  }

  if (this->setup_mode_) {
    ESP_LOGCONFIG(TAG, "  Setup Mode ENABLED");
  }

  this->dump_config_sensors_();
}

void ESP32TouchComponent::loop() {
  const uint32_t now = App.get_loop_component_start_time();

  // Print debug info for all pads in setup mode
  if (this->setup_mode_ && now - this->setup_mode_last_log_print_ > SETUP_MODE_LOG_INTERVAL_MS) {
    for (auto *child : this->children_) {
      ESP_LOGD(TAG, "Touch Pad '%s' (T%" PRIu32 "): %" PRIu32, child->get_name().c_str(),
               (uint32_t) child->get_touch_pad(), child->value_);
    }
    this->setup_mode_last_log_print_ = now;
  }

  // Process any queued touch events from interrupts
  // Note: Events are only sent by ISR for pads that were measured in that cycle (value != 0)
  // This is more efficient than sending all pad states every interrupt
  TouchPadEventV1 event;
  while (xQueueReceive(this->touch_queue_, &event, 0) == pdTRUE) {
    // Find the corresponding sensor - O(n) search is acceptable since events are infrequent
    for (auto *child : this->children_) {
      if (child->get_touch_pad() != event.pad) {
        continue;
      }

      // Found matching pad - process it
      child->value_ = event.value;

      // The interrupt gives us the touch state directly
      bool new_state = event.is_touched;

      // Track when we last saw this pad as touched
      if (new_state) {
        this->last_touch_time_[event.pad] = now;
      }

      // Only publish if state changed - this filters out repeated events
      if (new_state != child->last_state_) {
        child->last_state_ = new_state;
        child->publish_state(new_state);
        // Original ESP32: ISR only fires when touched, release is detected by timeout
        // Note: ESP32 v1 uses inverted logic - touched when value < threshold
        ESP_LOGV(TAG, "Touch Pad '%s' state: ON (value: %" PRIu32 " < threshold: %" PRIu32 ")",
                 child->get_name().c_str(), event.value, child->get_threshold());
      }
      break;  // Exit inner loop after processing matching pad
    }
  }

  // Check for released pads periodically
  static uint32_t last_release_check = 0;
  if (now - last_release_check < this->release_check_interval_ms_) {
    return;
  }
  last_release_check = now;

  for (auto *child : this->children_) {
    touch_pad_t pad = child->get_touch_pad();
    uint32_t last_time = this->last_touch_time_[pad];

    // Design note: Sentinel value pattern explanation
    // - 0: Never touched since boot (waiting for initial timeout)
    // - 1: Initial OFF state has been published (prevents repeated publishes)
    // - >1: Actual timestamp of last touch event
    // This avoids needing a separate boolean flag for initial state tracking

    // If we've never seen this pad touched (last_time == 0) and enough time has passed
    // since startup, publish OFF state and mark as published with value 1
    if (last_time == 0 && now > this->release_timeout_ms_) {
      child->publish_initial_state(false);
      this->last_touch_time_[pad] = 1;  // Mark as "initial state published"
      ESP_LOGV(TAG, "Touch Pad '%s' state: OFF (initial)", child->get_name().c_str());
    } else if (child->last_state_ && last_time > 1) {  // last_time > 1 means it's a real timestamp
      uint32_t time_diff = now - last_time;

      // Check if we haven't seen this pad recently
      if (time_diff > this->release_timeout_ms_) {
        // Haven't seen this pad recently, assume it's released
        child->last_state_ = false;
        child->publish_state(false);
        this->last_touch_time_[pad] = 1;  // Reset to "initial published" state
        ESP_LOGV(TAG, "Touch Pad '%s' state: OFF (timeout)", child->get_name().c_str());
      }
    }
  }
}

void ESP32TouchComponent::on_shutdown() {
  touch_pad_intr_disable();
  touch_pad_isr_deregister(touch_isr_handler, this);
  this->cleanup_touch_queue();

  if (this->iir_filter_enabled_()) {
    touch_pad_filter_stop();
    touch_pad_filter_delete();
  }

  // Configure wakeup pads if any are set
  this->configure_wakeup_pads_();
}

void IRAM_ATTR ESP32TouchComponent::touch_isr_handler(void *arg) {
  ESP32TouchComponent *component = static_cast<ESP32TouchComponent *>(arg);

  touch_pad_clear_status();

  // Process all configured pads to check their current state
  // Note: ESP32 v1 doesn't tell us which specific pad triggered the interrupt,
  // so we must scan all configured pads to find which ones were touched
  for (auto *child : component->children_) {
    touch_pad_t pad = child->get_touch_pad();

    // Read current value using ISR-safe API
    uint32_t value;
    if (component->iir_filter_enabled_()) {
      uint16_t temp_value = 0;
      touch_pad_read_filtered(pad, &temp_value);
      value = temp_value;
    } else {
      // Use low-level HAL function when filter is not enabled
      value = touch_ll_read_raw_data(pad);
    }

    // Skip pads with 0 value - they haven't been measured in this cycle
    // This is important: not all pads are measured every interrupt cycle,
    // only those that the hardware has updated
    if (value == 0) {
      continue;
    }

    // IMPORTANT: ESP32 v1 touch detection logic - INVERTED compared to v2!
    // ESP32 v1: Touch is detected when capacitance INCREASES, causing the measured value to DECREASE
    // Therefore: touched = (value < threshold)
    // This is opposite to ESP32-S2/S3 v2 where touched = (value > threshold)
    bool is_touched = value < child->get_threshold();

    // Always send the current state - the main loop will filter for changes
    // We send both touched and untouched states because the ISR doesn't
    // track previous state (to keep ISR fast and simple)
    TouchPadEventV1 event;
    event.pad = pad;
    event.value = value;
    event.is_touched = is_touched;

    // Send to queue from ISR - non-blocking, drops if queue full
    BaseType_t x_higher_priority_task_woken = pdFALSE;
    xQueueSendFromISR(component->touch_queue_, &event, &x_higher_priority_task_woken);
    if (x_higher_priority_task_woken) {
      portYIELD_FROM_ISR();
    }
  }
}

}  // namespace esp32_touch
}  // namespace esphome

#endif  // USE_ESP32_VARIANT_ESP32
