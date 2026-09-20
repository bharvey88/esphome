#include "wf_platform.h"

#include <cstring>

#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace wled_fx {

uint32_t platform_random_u32() { return random_uint32(); }

uint32_t platform_millis() { return millis(); }

void *platform_alloc(size_t size) {
  RAMAllocator<uint8_t> allocator;
  uint8_t *buffer = allocator.allocate(size);
  if (buffer != nullptr) {
    memset(buffer, 0, size);
  }
  return buffer;
}

void platform_free(void *ptr) {
  if (ptr == nullptr)
    return;
  RAMAllocator<uint8_t> allocator;
  allocator.deallocate(static_cast<uint8_t *>(ptr), 0);
}

}  // namespace wled_fx
}  // namespace esphome
