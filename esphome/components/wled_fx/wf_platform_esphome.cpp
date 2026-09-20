/* ESPHome's answers to the engine's platform hooks. The engine ships defaults
 * that work anywhere; these replace them with the hardware RNG, ESPHome's clock
 * and the RAM allocator, which can place the canvas in PSRAM.
 */

#include "wf_platform_esphome.h"

#include <cstring>

#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"

namespace esphome::wled_fx {

namespace {

uint32_t esphome_random_u32() { return random_uint32(); }

uint32_t esphome_millis() { return millis(); }

void *esphome_alloc(size_t size) {
  RAMAllocator<uint8_t> allocator;
  uint8_t *buffer = allocator.allocate(size);
  if (buffer != nullptr)
    memset(buffer, 0, size);
  return buffer;
}

void esphome_free(void *ptr) {
  if (ptr == nullptr)
    return;
  RAMAllocator<uint8_t> allocator;
  allocator.deallocate(static_cast<uint8_t *>(ptr), 0);
}

}  // namespace

void install_platform() {
  const PlatformHooks hooks{esphome_random_u32, esphome_millis, esphome_alloc, esphome_free};
  set_platform(hooks);
}

}  // namespace esphome::wled_fx
