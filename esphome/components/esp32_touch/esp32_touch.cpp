#ifdef USE_ESP32

#include "esp32_touch.h"
#include "esphome/core/application.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"

#include <algorithm>
#include <cinttypes>

// Include HAL for ISR-safe touch reading on all variants
#include "hal/touch_sensor_ll.h"
// Include for ISR-safe printing
#include "rom/ets_sys.h"
// Include for RTC clock frequency
#include "soc/rtc.h"

namespace esphome {
namespace esp32_touch {

static const char *const TAG = "esp32_touch";

void ESP32TouchComponent::setup() {
  ESP_LOGCONFIG(TAG, "Running setup");
  ESP_LOGI(TAG, "Number of touch pads configured: %d", this->children_.size());

  if (this->children_.empty()) {
    ESP_LOGE(TAG, "No touch pads configured!");
    this->mark_failed();
    return;
  }

  touch_pad_init();

  // Create queue for touch events - size based on number of touch pads
  // Each pad can have at most a few events queued (press/release)
  // Use 4x the number of pads to handle burst events
  size_t queue_size = this->children_.size() * 4;
  if (queue_size < 8)
    queue_size = 8;  // Minimum queue size

  // QUEUE SIZE likely doesn't make sense if its really ratelimited
  // to 1 per second, but this is a good starting point

  this->touch_queue_ = xQueueCreate(queue_size, sizeof(TouchPadEvent));
  if (this->touch_queue_ == nullptr) {
    ESP_LOGE(TAG, "Failed to create touch event queue of size %d", queue_size);
    this->mark_failed();
    return;
  }
// set up and enable/start filtering based on ESP32 variant
#if defined(USE_ESP32_VARIANT_ESP32S2) || defined(USE_ESP32_VARIANT_ESP32S3)
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
#else
  if (this->iir_filter_enabled_()) {
    touch_pad_filter_start(this->iir_filter_);
  }
#endif

#if ESP_IDF_VERSION_MAJOR >= 5 && defined(USE_ESP32_VARIANT_ESP32)
  ESP_LOGD(TAG, "Setting measurement_clock_cycles=%u, measurement_interval=%u", this->meas_cycle_, this->sleep_cycle_);
  touch_pad_set_measurement_clock_cycles(this->meas_cycle_);
  touch_pad_set_measurement_interval(this->sleep_cycle_);
#else
  ESP_LOGD(TAG, "Setting meas_time: sleep_cycle=%u, meas_cycle=%u", this->sleep_cycle_, this->meas_cycle_);
  touch_pad_set_meas_time(this->sleep_cycle_, this->meas_cycle_);
#endif
  touch_pad_set_voltage(this->high_voltage_reference_, this->low_voltage_reference_, this->voltage_attenuation_);

  for (auto *child : this->children_) {
#if defined(USE_ESP32_VARIANT_ESP32S2) || defined(USE_ESP32_VARIANT_ESP32S3)
    touch_pad_config(child->get_touch_pad());
    if (child->get_threshold() > 0) {
      touch_pad_set_thresh(child->get_touch_pad(), child->get_threshold());
    }
#else
    // Set interrupt threshold
    touch_pad_config(child->get_touch_pad(), child->get_threshold());
#endif
  }

#if defined(USE_ESP32_VARIANT_ESP32S2) || defined(USE_ESP32_VARIANT_ESP32S3)
  touch_pad_set_fsm_mode(TOUCH_FSM_MODE_TIMER);
  touch_pad_fsm_start();
#else
  // For ESP32, we'll use software mode with manual triggering
  // Timer mode seems to break touch measurements completely
  touch_pad_set_fsm_mode(TOUCH_FSM_MODE_SW);

  // Set trigger mode and source
  touch_pad_set_trigger_mode(TOUCH_TRIGGER_BELOW);
  touch_pad_set_trigger_source(TOUCH_TRIGGER_SOURCE_BOTH);
  // Clear any pending interrupts before starting
  touch_pad_clear_status();

  // Do an initial measurement
  touch_pad_sw_start();
#endif

  // Register ISR handler
  esp_err_t err = touch_pad_isr_register(touch_isr_handler, this);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to register touch ISR: %s", esp_err_to_name(err));
    vQueueDelete(this->touch_queue_);
    this->touch_queue_ = nullptr;
    this->mark_failed();
    return;
  }

  // Calculate release timeout based on sleep cycle
  // Sleep cycle is in RTC_SLOW_CLK cycles (typically 150kHz, but can be 32kHz)
  // Get actual RTC clock frequency
  uint32_t rtc_freq = rtc_clk_slow_freq_get_hz();

#if defined(USE_ESP32_VARIANT_ESP32S2) || defined(USE_ESP32_VARIANT_ESP32S3)
  // For S2/S3, calculate based on actual sleep cycle since they use timer mode
  this->release_timeout_ms_ = (this->sleep_cycle_ * 1000 * 3) / (rtc_freq * 2);
  if (this->release_timeout_ms_ < 100) {
    this->release_timeout_ms_ = 100;  // Minimum 100ms
  }
#else
  // For ESP32 in software mode, we're triggering manually
  // Since we're triggering every 1 second in the debug loop, use 1500ms timeout
  this->release_timeout_ms_ = 1500;  // 1.5 seconds
#endif

  // Calculate check interval
  this->release_check_interval_ms_ = std::min(this->release_timeout_ms_ / 4, (uint32_t) 50);

  // Read back the actual configuration to verify
  uint16_t actual_sleep_cycle = 0;
  uint16_t actual_meas_cycle = 0;
#if ESP_IDF_VERSION_MAJOR >= 5 && defined(USE_ESP32_VARIANT_ESP32)
  touch_pad_get_measurement_interval(&actual_sleep_cycle);
  touch_pad_get_measurement_clock_cycles(&actual_meas_cycle);
#else
  touch_pad_get_meas_time(&actual_sleep_cycle, &actual_meas_cycle);
#endif

  ESP_LOGI(TAG, "Touch timing config - requested: sleep=%u, meas=%u | actual: sleep=%u, meas=%u", this->sleep_cycle_,
           this->meas_cycle_, actual_sleep_cycle, actual_meas_cycle);
  ESP_LOGI(TAG, "Touch release timeout: %u ms, check interval: %u ms (RTC freq: %u Hz)", this->release_timeout_ms_,
           this->release_check_interval_ms_, rtc_freq);

  // Enable touch pad interrupt
  touch_pad_intr_enable();
  ESP_LOGI(TAG, "Touch pad interrupts enabled");

  // Check FSM state for debugging
  touch_fsm_mode_t fsm_mode;
  touch_pad_get_fsm_mode(&fsm_mode);
  ESP_LOGI(TAG, "FSM mode: %s", fsm_mode == TOUCH_FSM_MODE_TIMER ? "TIMER" : "SW");

  ESP_LOGI(TAG, "Initial touch status: 0x%04x", touch_pad_get_status());

  // Log which pads are configured and initialize their state
  ESP_LOGI(TAG, "Configured touch pads:");
  for (auto *child : this->children_) {
    uint32_t value = this->component_touch_pad_read(child->get_touch_pad());
    ESP_LOGI(TAG, "  Touch Pad %d: threshold=%d, current value=%d", (int) child->get_touch_pad(),
             (int) child->get_threshold(), (int) value);

    // Initialize the sensor state based on current value
#if defined(USE_ESP32_VARIANT_ESP32S2) || defined(USE_ESP32_VARIANT_ESP32S3)
    bool is_touched = value > child->get_threshold();
#else
    bool is_touched = value < child->get_threshold();
#endif

    child->last_state_ = is_touched;
    child->publish_initial_state(is_touched);

    if (is_touched) {
      this->last_touch_time_[child->get_touch_pad()] = App.get_loop_component_start_time();
    }
  }

  ESP_LOGI(TAG, "ESP32 Touch setup complete");
}

void ESP32TouchComponent::dump_config() {
  ESP_LOGCONFIG(TAG,
                "Config for ESP32 Touch Hub:\n"
                "  Meas cycle: %.2fms\n"
                "  Sleep cycle: %.2fms",
                this->meas_cycle_ / (8000000.0f / 1000.0f), this->sleep_cycle_ / (150000.0f / 1000.0f));

  const char *lv_s;
  switch (this->low_voltage_reference_) {
    case TOUCH_LVOLT_0V5:
      lv_s = "0.5V";
      break;
    case TOUCH_LVOLT_0V6:
      lv_s = "0.6V";
      break;
    case TOUCH_LVOLT_0V7:
      lv_s = "0.7V";
      break;
    case TOUCH_LVOLT_0V8:
      lv_s = "0.8V";
      break;
    default:
      lv_s = "UNKNOWN";
      break;
  }
  ESP_LOGCONFIG(TAG, "  Low Voltage Reference: %s", lv_s);

  const char *hv_s;
  switch (this->high_voltage_reference_) {
    case TOUCH_HVOLT_2V4:
      hv_s = "2.4V";
      break;
    case TOUCH_HVOLT_2V5:
      hv_s = "2.5V";
      break;
    case TOUCH_HVOLT_2V6:
      hv_s = "2.6V";
      break;
    case TOUCH_HVOLT_2V7:
      hv_s = "2.7V";
      break;
    default:
      hv_s = "UNKNOWN";
      break;
  }
  ESP_LOGCONFIG(TAG, "  High Voltage Reference: %s", hv_s);

  const char *atten_s;
  switch (this->voltage_attenuation_) {
    case TOUCH_HVOLT_ATTEN_1V5:
      atten_s = "1.5V";
      break;
    case TOUCH_HVOLT_ATTEN_1V:
      atten_s = "1V";
      break;
    case TOUCH_HVOLT_ATTEN_0V5:
      atten_s = "0.5V";
      break;
    case TOUCH_HVOLT_ATTEN_0V:
      atten_s = "0V";
      break;
    default:
      atten_s = "UNKNOWN";
      break;
  }
  ESP_LOGCONFIG(TAG, "  Voltage Attenuation: %s", atten_s);

#if defined(USE_ESP32_VARIANT_ESP32S2) || defined(USE_ESP32_VARIANT_ESP32S3)
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
#else
  if (this->iir_filter_enabled_()) {
    ESP_LOGCONFIG(TAG, "    IIR Filter: %" PRIu32 "ms", this->iir_filter_);
  } else {
    ESP_LOGCONFIG(TAG, "  IIR Filter DISABLED");
  }
#endif

  if (this->setup_mode_) {
    ESP_LOGCONFIG(TAG, "  Setup Mode ENABLED");
  }

  for (auto *child : this->children_) {
    LOG_BINARY_SENSOR("  ", "Touch Pad", child);
    ESP_LOGCONFIG(TAG, "    Pad: T%" PRIu32, (uint32_t) child->get_touch_pad());
    ESP_LOGCONFIG(TAG, "    Threshold: %" PRIu32, child->get_threshold());
  }
}

uint32_t ESP32TouchComponent::component_touch_pad_read(touch_pad_t tp) {
#if defined(USE_ESP32_VARIANT_ESP32S2) || defined(USE_ESP32_VARIANT_ESP32S3)
  uint32_t value = 0;
  if (this->filter_configured_()) {
    touch_pad_filter_read_smooth(tp, &value);
  } else {
    touch_pad_read_raw_data(tp, &value);
  }
#else
  uint16_t value = 0;
  if (this->iir_filter_enabled_()) {
    touch_pad_read_filtered(tp, &value);
  } else {
    touch_pad_read(tp, &value);
  }
#endif
  return value;
}

void ESP32TouchComponent::loop() {
  const uint32_t now = App.get_loop_component_start_time();
  bool should_print = now - this->setup_mode_last_log_print_ > 1000;  // Log every second

  // Always check touch status periodically
  if (should_print) {
    uint32_t current_status = touch_pad_get_status();
    uint32_t hal_status;
    touch_ll_read_trigger_status_mask(&hal_status);

    // Check if FSM is still in timer mode
    touch_fsm_mode_t fsm_mode;
    touch_pad_get_fsm_mode(&fsm_mode);

    ESP_LOGD(TAG, "Current touch status: 0x%04x (HAL: 0x%04x), FSM: %s", current_status, hal_status,
             fsm_mode == TOUCH_FSM_MODE_TIMER ? "TIMER" : "SW");

    // Try a manual software trigger to see if measurements are working at all
    if (current_status == 0 && hal_status == 0) {
      ESP_LOGD(TAG, "No touch status, trying manual trigger...");
      touch_pad_sw_start();
    }

    for (auto *child : this->children_) {
      uint32_t value = this->component_touch_pad_read(child->get_touch_pad());
      // Touch detection logic differs between ESP32 variants
#if defined(USE_ESP32_VARIANT_ESP32S2) || defined(USE_ESP32_VARIANT_ESP32S3)
      bool is_touched = value > child->get_threshold();
#else
      bool is_touched = value < child->get_threshold();
#endif
      ESP_LOGD(TAG, "Touch Pad '%s' (T%" PRIu32 "): value=%" PRIu32 ", threshold=%" PRIu32 ", touched=%s",
               child->get_name().c_str(), (uint32_t) child->get_touch_pad(), value, child->get_threshold(),
               is_touched ? "YES" : "NO");
    }
    this->setup_mode_last_log_print_ = now;
  }

  // Process any queued touch events from interrupts
  TouchPadEvent event;
  uint32_t processed_pads = 0;  // Bitmask of pads we processed events for
  while (xQueueReceive(this->touch_queue_, &event, 0) == pdTRUE) {
    processed_pads |= (1 << event.pad);
    // Find the corresponding sensor
    for (auto *child : this->children_) {
      if (child->get_touch_pad() == event.pad) {
        child->value_ = event.value;

        // The interrupt gives us the touch state directly
        bool new_state = event.is_touched;

        // Track when we last saw this pad as touched
        if (new_state) {
          this->last_touch_time_[event.pad] = now;
        }

        // Only publish if state changed
        if (new_state != child->last_state_) {
          child->last_state_ = new_state;
          child->publish_state(new_state);
          ESP_LOGD(TAG, "Touch Pad '%s' state: %s (value: %" PRIu32 ", threshold: %" PRIu32 ")",
                   child->get_name().c_str(), new_state ? "ON" : "OFF", event.value, child->get_threshold());
        }
        break;
      }
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

    // Skip if we just processed an event for this pad
    if ((processed_pads >> pad) & 0x01) {
      continue;
    }

    if (child->last_state_) {
      uint32_t last_time = this->last_touch_time_[pad];
      uint32_t time_diff = now - last_time;

      // Check if we haven't seen this pad recently
      if (last_time == 0 || time_diff > this->release_timeout_ms_) {
        // Haven't seen this pad recently, assume it's released
        child->last_state_ = false;
        child->publish_state(false);
        this->last_touch_time_[pad] = 0;
        ESP_LOGD(TAG, "Touch Pad '%s' state: OFF (timeout)", child->get_name().c_str());
      }
    }
  }
}

void ESP32TouchComponent::on_shutdown() {
  touch_pad_intr_disable();
  touch_pad_isr_deregister(touch_isr_handler, this);
  if (this->touch_queue_) {
    vQueueDelete(this->touch_queue_);
  }

  bool is_wakeup_source = false;

#if !(defined(USE_ESP32_VARIANT_ESP32S2) || defined(USE_ESP32_VARIANT_ESP32S3))
  if (this->iir_filter_enabled_()) {
    touch_pad_filter_stop();
    touch_pad_filter_delete();
  }
#endif

  for (auto *child : this->children_) {
    if (child->get_wakeup_threshold() != 0) {
      if (!is_wakeup_source) {
        is_wakeup_source = true;
        // Touch sensor FSM mode must be 'TOUCH_FSM_MODE_TIMER' to use it to wake-up.
        touch_pad_set_fsm_mode(TOUCH_FSM_MODE_TIMER);
      }

#if !(defined(USE_ESP32_VARIANT_ESP32S2) || defined(USE_ESP32_VARIANT_ESP32S3))
      // No filter available when using as wake-up source.
      touch_pad_config(child->get_touch_pad(), child->get_wakeup_threshold());
#endif
    }
  }

  if (!is_wakeup_source) {
    touch_pad_deinit();
  }
}

void IRAM_ATTR ESP32TouchComponent::touch_isr_handler(void *arg) {
  ESP32TouchComponent *component = static_cast<ESP32TouchComponent *>(arg);

  // Log that ISR was called
  ets_printf("Touch ISR triggered!\n");

  uint32_t pad_status = touch_pad_get_status();
  touch_pad_clear_status();

  // Always log the status
  ets_printf("Touch ISR: raw status=0x%04x\n", pad_status);

  // Process all configured pads to check their current state
  // Send events for ALL pads with valid readings so we catch both touches and releases
  for (auto *child : component->children_) {
    touch_pad_t pad = child->get_touch_pad();

    // Read current value
    uint32_t value = touch_ll_read_raw_data(pad);

    // Skip pads with 0 value - they haven't been measured in this cycle
    if (value == 0) {
      continue;
    }

    // Determine current touch state based on value vs threshold
#if defined(USE_ESP32_VARIANT_ESP32S2) || defined(USE_ESP32_VARIANT_ESP32S3)
    bool is_touched = value > child->get_threshold();
#else
    bool is_touched = value < child->get_threshold();
#endif

    ets_printf("  Pad %d: value=%d, threshold=%d, touched=%d\n", pad, value, child->get_threshold(), is_touched);

    // Always send the current state - the main loop will filter for changes
    TouchPadEvent event;
    event.pad = pad;
    event.value = value;
    event.is_touched = is_touched;

    // Send to queue from ISR
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xQueueSendFromISR(component->touch_queue_, &event, &xHigherPriorityTaskWoken);
    if (xHigherPriorityTaskWoken) {
      portYIELD_FROM_ISR();
    }
  }
}

ESP32TouchBinarySensor::ESP32TouchBinarySensor(touch_pad_t touch_pad, uint32_t threshold, uint32_t wakeup_threshold)
    : touch_pad_(touch_pad), threshold_(threshold), wakeup_threshold_(wakeup_threshold) {}

}  // namespace esp32_touch
}  // namespace esphome

#endif
