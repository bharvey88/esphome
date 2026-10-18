#pragma once

#include "remote_base.h"

/* Based on protocol analysis from
 * https://github.com/sueppchen/PixMob_waveband (Pixmob_cement/pixmob_cement.cpp)
 */

namespace esphome::remote_base {

// The band takes 6 bits per color channel, so the low 2 bits of red, green and blue are not sent.
struct PixMobData {
  uint8_t red;
  uint8_t green;
  uint8_t blue;
  uint8_t group;   // 5 bits
  uint8_t attack;  // 3 bits, as are hold, release and random
  uint8_t hold;
  uint8_t release;
  uint8_t random;

  // Colors compare on the 6 bits that are sent
  bool operator==(const PixMobData &rhs) const {
    return (this->red >> 2) == (rhs.red >> 2) && (this->green >> 2) == (rhs.green >> 2) &&
           (this->blue >> 2) == (rhs.blue >> 2) && this->group == rhs.group && this->attack == rhs.attack &&
           this->hold == rhs.hold && this->release == rhs.release && this->random == rhs.random;
  }
};

class PixMobProtocol : public RemoteProtocol<PixMobData> {
 public:
  void encode(RemoteTransmitData *dst, const PixMobData &data);
  optional<PixMobData> decode(RemoteReceiveData src);
  void dump(const PixMobData &data);
};

DECLARE_REMOTE_PROTOCOL(PixMob)

}  // namespace esphome::remote_base
