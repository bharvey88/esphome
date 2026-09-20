#pragma once

// Platform shim for the effect engine. The engine, the maths and the effect files
// include this rather than an ESPHome header directly, so the same sources build
// unchanged outside a firmware build.

#include <cstddef>
#include <cstdint>

namespace esphome::wled_fx {

// Uniformly distributed 32 bit value, from the hardware RNG behind random_uint32().
uint32_t platform_random_u32();

// Milliseconds since boot.
uint32_t platform_millis();

// One-shot buffer allocation for the canvas and for effect scratch data. Returns
// nullptr on failure. Must not be called per frame.
void *platform_alloc(size_t size);
void platform_free(void *ptr);

}  // namespace esphome::wled_fx
