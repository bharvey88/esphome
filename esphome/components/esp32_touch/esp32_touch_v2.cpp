#if defined(USE_ESP32_VARIANT_ESP32S2) || defined(USE_ESP32_VARIANT_ESP32S3)

#include "esp32_touch.h"
#include "esphome/core/application.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace esp32_touch {

static const char *const TAG = "esp32_touch";

void ESP32TouchComponent::setup() {
  // Create queue for touch events first
  if (!this->create_touch_queue_()) {
    return;
  }

  // Initialize touch pad peripheral
  esp_err_t init_err = touch_pad_init();
  if (init_err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to initialize touch pad: %s", esp_err_to_name(init_err));
    this->mark_failed();
    return;
  }

  // Configure each touch pad first
  for (auto *child : this->children_) {
    esp_err_t config_err = touch_pad_config(child->get_touch_pad());
    if (config_err != ESP_OK) {
      ESP_LOGE(TAG, "Failed to configure touch pad %d: %s", child->get_touch_pad(), esp_err_to_name(config_err));
    }
  }

  // Set up filtering if configured
  if (this->filter_configured_()) {
    touch_filter_config_t filter_info = {
        .mode = this->filter_mode_,
        .debounce_cnt = this->debounce_count_,
        .noise_thr = this->noise_threshold_,
        .jitter_step = this->jitter_step_,
        .smh_lvl = this->smooth_level_,
    };
    touch_pad_filter_set_config(&filter_info);
    touch_pad_filter_enable();
  }

  if (this->denoise_configured_()) {
    touch_pad_denoise_t denoise = {
        .grade = this->grade_,
        .cap_level = this->cap_level_,
    };
    touch_pad_denoise_set_config(&denoise);
    touch_pad_denoise_enable();
  }

  if (this->waterproof_configured_()) {
    touch_pad_waterproof_t waterproof = {
        .guard_ring_pad = this->waterproof_guard_ring_pad_,
        .shield_driver = this->waterproof_shield_driver_,
    };
    touch_pad_waterproof_set_config(&waterproof);
    touch_pad_waterproof_enable();
  }

  // Configure measurement parameters
  touch_pad_set_voltage(this->high_voltage_reference_, this->low_voltage_reference_, this->voltage_attenuation_);
  // ESP32-S2/S3 always use the older API
  touch_pad_set_meas_time(this->sleep_cycle_, this->meas_cycle_);

  // Configure timeout if needed
  touch_pad_timeout_set(true, TOUCH_PAD_THRESHOLD_MAX);

  // Register ISR handler with interrupt mask
  esp_err_t err =
      touch_pad_isr_register(touch_isr_handler, this, static_cast<touch_pad_intr_mask_t>(TOUCH_PAD_INTR_MASK_ALL));
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to register touch ISR: %s", esp_err_to_name(err));
    this->cleanup_touch_queue_();
    this->mark_failed();
    return;
  }

  // Enable interrupts
  touch_pad_intr_enable(static_cast<touch_pad_intr_mask_t>(TOUCH_PAD_INTR_MASK_ACTIVE | TOUCH_PAD_INTR_MASK_INACTIVE |
                                                           TOUCH_PAD_INTR_MASK_TIMEOUT));

  // Set FSM mode before starting
  touch_pad_set_fsm_mode(TOUCH_FSM_MODE_TIMER);

  // Start FSM
  touch_pad_fsm_start();

  // Set thresholds for each pad
  for (auto *child : this->children_) {
    if (child->get_threshold() != 0) {
      touch_pad_set_thresh(child->get_touch_pad(), child->get_threshold());
    }
  }

  // Read initial states after all hardware is initialized
  for (auto *child : this->children_) {
    // Read current value
    uint32_t value = this->read_touch_value(child->get_touch_pad());

    // IMPORTANT: ESP32-S2/S3 v2 touch detection logic - INVERTED compared to v1!
    // ESP32-S2/S3 v2: Touch is detected when capacitance INCREASES, causing the measured value to INCREASE
    // Therefore: touched = (value > threshold)
    // This is opposite to original ESP32 v1 where touched = (value < threshold)
    bool is_touched = value > child->get_threshold();
    child->last_state_ = is_touched;
    child->publish_initial_state(is_touched);

    // Note: ESP32-S2/S3 v2 uses inverted logic compared to v1 - touched when value > threshold
    ESP_LOGD(TAG, "Touch Pad '%s' initial state: %s (value: %d %s threshold: %d)", child->get_name().c_str(),
             is_touched ? "touched" : "released", value, is_touched ? ">" : "<=", child->get_threshold());
  }
}

void ESP32TouchComponent::dump_config() {
  this->dump_config_base_();

  if (this->filter_configured_()) {
    const char *filter_mode_s;
    switch (this->filter_mode_) {
      case TOUCH_PAD_FILTER_IIR_4:
        filter_mode_s = "IIR_4";
        break;
      case TOUCH_PAD_FILTER_IIR_8:
        filter_mode_s = "IIR_8";
        break;
      case TOUCH_PAD_FILTER_IIR_16:
        filter_mode_s = "IIR_16";
        break;
      case TOUCH_PAD_FILTER_IIR_32:
        filter_mode_s = "IIR_32";
        break;
      case TOUCH_PAD_FILTER_IIR_64:
        filter_mode_s = "IIR_64";
        break;
      case TOUCH_PAD_FILTER_IIR_128:
        filter_mode_s = "IIR_128";
        break;
      case TOUCH_PAD_FILTER_IIR_256:
        filter_mode_s = "IIR_256";
        break;
      case TOUCH_PAD_FILTER_JITTER:
        filter_mode_s = "JITTER";
        break;
      default:
        filter_mode_s = "UNKNOWN";
        break;
    }
    ESP_LOGCONFIG(TAG,
                  "  Filter mode: %s\n"
                  "  Debounce count: %" PRIu32 "\n"
                  "  Noise threshold coefficient: %" PRIu32 "\n"
                  "  Jitter filter step size: %" PRIu32,
                  filter_mode_s, this->debounce_count_, this->noise_threshold_, this->jitter_step_);
    const char *smooth_level_s;
    switch (this->smooth_level_) {
      case TOUCH_PAD_SMOOTH_OFF:
        smooth_level_s = "OFF";
        break;
      case TOUCH_PAD_SMOOTH_IIR_2:
        smooth_level_s = "IIR_2";
        break;
      case TOUCH_PAD_SMOOTH_IIR_4:
        smooth_level_s = "IIR_4";
        break;
      case TOUCH_PAD_SMOOTH_IIR_8:
        smooth_level_s = "IIR_8";
        break;
      default:
        smooth_level_s = "UNKNOWN";
        break;
    }
    ESP_LOGCONFIG(TAG, "  Smooth level: %s", smooth_level_s);
  }

  if (this->denoise_configured_()) {
    const char *grade_s;
    switch (this->grade_) {
      case TOUCH_PAD_DENOISE_BIT12:
        grade_s = "BIT12";
        break;
      case TOUCH_PAD_DENOISE_BIT10:
        grade_s = "BIT10";
        break;
      case TOUCH_PAD_DENOISE_BIT8:
        grade_s = "BIT8";
        break;
      case TOUCH_PAD_DENOISE_BIT4:
        grade_s = "BIT4";
        break;
      default:
        grade_s = "UNKNOWN";
        break;
    }
    ESP_LOGCONFIG(TAG, "  Denoise grade: %s", grade_s);

    const char *cap_level_s;
    switch (this->cap_level_) {
      case TOUCH_PAD_DENOISE_CAP_L0:
        cap_level_s = "L0";
        break;
      case TOUCH_PAD_DENOISE_CAP_L1:
        cap_level_s = "L1";
        break;
      case TOUCH_PAD_DENOISE_CAP_L2:
        cap_level_s = "L2";
        break;
      case TOUCH_PAD_DENOISE_CAP_L3:
        cap_level_s = "L3";
        break;
      case TOUCH_PAD_DENOISE_CAP_L4:
        cap_level_s = "L4";
        break;
      case TOUCH_PAD_DENOISE_CAP_L5:
        cap_level_s = "L5";
        break;
      case TOUCH_PAD_DENOISE_CAP_L6:
        cap_level_s = "L6";
        break;
      case TOUCH_PAD_DENOISE_CAP_L7:
        cap_level_s = "L7";
        break;
      default:
        cap_level_s = "UNKNOWN";
        break;
    }
    ESP_LOGCONFIG(TAG, "  Denoise capacitance level: %s", cap_level_s);
  }

  if (this->setup_mode_) {
    ESP_LOGCONFIG(TAG, "  Setup Mode ENABLED");
  }

  this->dump_config_sensors_();
}

void ESP32TouchComponent::loop() {
  const uint32_t now = App.get_loop_component_start_time();

  // In setup mode, periodically log all pad values
  if (this->setup_mode_ && now - this->setup_mode_last_log_print_ > SETUP_MODE_LOG_INTERVAL_MS) {
    for (auto *child : this->children_) {
      // Read the value being used for touch detection
      uint32_t value = this->read_touch_value(child->get_touch_pad());

      ESP_LOGD(TAG, "Touch Pad '%s' (T%d): %d", child->get_name().c_str(), child->get_touch_pad(), value);
    }
    this->setup_mode_last_log_print_ = now;
  }

  // Process any queued touch events from interrupts
  TouchPadEventV2 event;
  while (xQueueReceive(this->touch_queue_, &event, 0) == pdTRUE) {
    // Handle timeout events
    if (event.intr_mask & TOUCH_PAD_INTR_MASK_TIMEOUT) {
      // Resume measurement after timeout
      touch_pad_timeout_resume();
      continue;
    }

    // Skip if not an active/inactive event
    if (!(event.intr_mask & (TOUCH_PAD_INTR_MASK_ACTIVE | TOUCH_PAD_INTR_MASK_INACTIVE))) {
      continue;
    }

    bool is_touch_event = (event.intr_mask & TOUCH_PAD_INTR_MASK_ACTIVE) != 0;

    // Find the child for the pad that triggered the interrupt
    for (auto *child : this->children_) {
      if (child->get_touch_pad() != event.pad) {
        continue;
      }

      // Skip if state hasn't changed
      if (child->last_state_ == is_touch_event) {
        break;
      }

      // Read current value
      uint32_t value = this->read_touch_value(event.pad);

      child->last_state_ = is_touch_event;
      child->publish_state(is_touch_event);
      // Note: ESP32-S2/S3 v2 uses inverted logic compared to v1 - touched when value > threshold
      ESP_LOGD(TAG, "Touch Pad '%s' %s (value: %d %s threshold: %d)", child->get_name().c_str(),
               is_touch_event ? "touched" : "released", value, is_touch_event ? ">" : "<=", child->get_threshold());
      break;
    }
  }
}

void ESP32TouchComponent::on_shutdown() {
  // Disable interrupts
  touch_pad_intr_disable(static_cast<touch_pad_intr_mask_t>(TOUCH_PAD_INTR_MASK_ACTIVE | TOUCH_PAD_INTR_MASK_INACTIVE |
                                                            TOUCH_PAD_INTR_MASK_TIMEOUT));
  touch_pad_isr_deregister(touch_isr_handler, this);
  this->cleanup_touch_queue_();

  // Configure wakeup pads if any are set
  this->configure_wakeup_pads_();
}

void IRAM_ATTR ESP32TouchComponent::touch_isr_handler(void *arg) {
  ESP32TouchComponent *component = static_cast<ESP32TouchComponent *>(arg);
  BaseType_t x_higher_priority_task_woken = pdFALSE;

  // Read interrupt status
  TouchPadEventV2 event;
  event.intr_mask = touch_pad_read_intr_status_mask();
  event.pad = touch_pad_get_current_meas_channel();

  // Send event to queue for processing in main loop
  xQueueSendFromISR(component->touch_queue_, &event, &x_higher_priority_task_woken);

  if (x_higher_priority_task_woken) {
    portYIELD_FROM_ISR();
  }
}

uint32_t ESP32TouchComponent::read_touch_value(touch_pad_t pad) const {
  // Unlike ESP32 v1, touch reads on ESP32-S2/S3 v2 are non-blocking operations.
  // The hardware continuously samples in the background and we can read the
  // latest value at any time without waiting.
  uint32_t value = 0;
  if (this->filter_configured_()) {
    // Read filtered/smoothed value when filter is enabled
    touch_pad_filter_read_smooth(pad, &value);
  } else {
    // Read raw value when filter is not configured
    touch_pad_read_raw_data(pad, &value);
  }
  return value;
}

}  // namespace esp32_touch
}  // namespace esphome

#endif  // USE_ESP32_VARIANT_ESP32S2 || USE_ESP32_VARIANT_ESP32S3
