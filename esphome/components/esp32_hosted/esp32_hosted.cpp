#include "esp32_hosted.h"
#include "esphome/core/log.h"

#ifdef USE_ESP32

#ifdef USE_ESP32_BLE
#include <esp_bluedroid_hci.h>
#include <hosted_hci_bluedroid.h>
#endif

namespace esphome {
namespace esp32_hosted {

static const char *const TAG = "esp32_hosted";

void ESP32Hosted::setup() {
  ESP_LOGCONFIG(TAG, "Setting up ESP32 Hosted...");

#ifdef USE_ESP32_BLE
  // Initialize Bluetooth if esp32_ble component is enabled
  this->init_bluetooth_();
#else
  ESP_LOGD(TAG, "Bluetooth support disabled (esp32_ble not included)");
#endif
}

void ESP32Hosted::dump_config() { ESP_LOGCONFIG(TAG, "ESP32 Hosted:"); }

#ifdef USE_ESP32_BLE
void ESP32Hosted::init_bluetooth_() {
  ESP_LOGD(TAG, "Initializing Bluetooth...");

  // Initialize TRANSPORT first
  hosted_hci_bluedroid_open();

  // Get HCI driver operations
  esp_bluedroid_hci_driver_operations_t operations = {
      .send = hosted_hci_bluedroid_send,
      .check_send_available = hosted_hci_bluedroid_check_send_available,
      .register_host_callback = hosted_hci_bluedroid_register_host_callback,
  };

  // Attach HCI driver
  esp_bluedroid_attach_hci_driver(&operations);

  ESP_LOGD(TAG, "Bluetooth initialized successfully");
}
#endif

}  // namespace esp32_hosted
}  // namespace esphome

#endif  // USE_ESP32