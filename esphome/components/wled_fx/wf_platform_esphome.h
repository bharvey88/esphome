#pragma once

#include "wf_platform.h"

namespace esphome::wled_fx {

// Must run before the first Engine::init(). Calling it twice is harmless.
void install_platform();

}  // namespace esphome::wled_fx
