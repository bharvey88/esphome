#include "pixmob_protocol.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::remote_base {

ESPHOME_LOG_TAG(TAG, "remote.pixmob");

// A frame is 90 bits of 500 us each with no carrier: the preamble 0x55 twice, a 0 and a 1 sync bit, then nine line
// coded bytes, every byte least significant bit first. The first and last byte hold the low and high 6 bits of a
// CRC-12 over the seven line coded message bytes between them.
static constexpr uint32_t BIT_US = 500;
static constexpr uint8_t HEADER_BITS = 18;
static constexpr uint32_t HEADER = 0x25555;  // preamble and sync, first bit sent lowest
static constexpr uint8_t MESSAGE_BYTES = 7;
static constexpr uint8_t FRAME_BYTES = MESSAGE_BYTES + 2;
static constexpr uint8_t FRAME_BITS = HEADER_BITS + FRAME_BYTES * 8;
// The line code never sends the same bit more than 4 times in a row, across byte boundaries too
static constexpr uint8_t MAX_RUN_BITS = 4;
static constexpr uint8_t MODE_COLOR = 0x00;
static constexpr uint16_t CRC_POLY = 0x8F3;  // reversed
static constexpr uint16_t CRC_INIT = 0xC69;

// 6b8b line code: the byte sent for each 6 bit value
static constexpr uint8_t LINE_CODE[64] PROGMEM = {
    0x21, 0x35, 0x2C, 0x34, 0x66, 0x26, 0xAC, 0x24, 0x46, 0x56, 0x44, 0x54, 0x64, 0x6D, 0x4C, 0x6C,
    0x92, 0xB2, 0xA6, 0xA2, 0xB4, 0x94, 0x86, 0x96, 0x42, 0x62, 0x2A, 0x6A, 0xB6, 0x36, 0x22, 0x32,
    0x31, 0xB1, 0x95, 0xB5, 0x91, 0x99, 0x85, 0x89, 0xA5, 0xA4, 0x8C, 0x84, 0xA1, 0xA9, 0x8D, 0xAD,
    0x9A, 0x8A, 0x5A, 0x4A, 0x49, 0x59, 0x52, 0x51, 0x25, 0x2D, 0x69, 0x29, 0x4D, 0x45, 0x61, 0x65,
};

static uint8_t line_code(uint8_t value) { return progmem_read_byte(&LINE_CODE[value & 0x3F]); }

// The 6 bit value a byte codes for, or -1 if it is not a line code
static int8_t line_decode(uint8_t code) {
  for (uint8_t value = 0; value < 64; value++) {
    if (line_code(value) == code)
      return value;
  }
  return -1;
}

static uint16_t frame_crc(const uint8_t *frame) { return crc16(frame + 1, MESSAGE_BYTES, CRC_INIT, CRC_POLY); }

// Bit `pos` of the frame in send order
static bool frame_bit(const uint8_t *frame, uint8_t pos) {
  if (pos < HEADER_BITS)
    return (HEADER >> pos) & 1;
  pos -= HEADER_BITS;
  return (frame[pos / 8] >> (pos % 8)) & 1;
}

void PixMobProtocol::encode(RemoteTransmitData *dst, const PixMobData &data) {
  // The band's byte order puts green before red
  const uint8_t message[MESSAGE_BYTES] = {
      MODE_COLOR,
      uint8_t(data.green >> 2),
      uint8_t(data.red >> 2),
      uint8_t(data.blue >> 2),
      uint8_t((data.attack & 0x07) << 3 | (data.random & 0x07)),
      uint8_t((data.release & 0x07) << 3 | (data.hold & 0x07)),
      uint8_t(data.group & 0x1F),
  };
  uint8_t frame[FRAME_BYTES];
  for (uint8_t i = 0; i < MESSAGE_BYTES; i++)
    frame[i + 1] = line_code(message[i]);
  const uint16_t crc = frame_crc(frame);
  frame[0] = line_code(crc);
  frame[FRAME_BYTES - 1] = line_code(crc >> 6);

  dst->set_carrier_frequency(0);
  dst->reserve(FRAME_BITS + 1);
  // Each run of equal bits is one mark or space
  uint32_t run = 1;
  for (uint8_t pos = 1; pos <= FRAME_BITS; pos++) {
    const bool bit = frame_bit(frame, pos - 1);
    if (pos < FRAME_BITS && frame_bit(frame, pos) == bit) {
      run++;
      continue;
    }
    if (bit) {
      dst->mark(run * BIT_US);
    } else {
      dst->space(run * BIT_US);
    }
    run = 1;
  }
  // The line idles low after the frame
  if (frame_bit(frame, FRAME_BITS - 1))
    dst->space(BIT_US);
}

// Decodes a frame that starts at the current timing
static optional<PixMobData> decode_frame(RemoteReceiveData &src) {
  // A noise mark right before the frame merges into its first bit
  if (!src.peek_mark_at_least(BIT_US))
    return {};
  src.advance();

  uint32_t header = 1;
  uint8_t frame[FRAME_BYTES]{};
  uint8_t pos = 1;
  bool mark = false;
  while (pos < FRAME_BITS) {
    const uint8_t left = FRAME_BITS - pos;
    // The line idles low, so the last zero bits can merge into the gap after the frame
    if (!mark && (!src.is_valid() || src.peek_space_at_least(left * BIT_US)))
      break;
    uint8_t run = 1;
    while (run <= MAX_RUN_BITS && !(mark ? src.peek_mark(run * BIT_US) : src.peek_space(run * BIT_US)))
      run++;
    if (run > MAX_RUN_BITS || run > left)
      return {};
    src.advance();
    for (; run > 0; run--, pos++) {
      if (!mark)
        continue;
      if (pos < HEADER_BITS) {
        header |= 1U << pos;
      } else {
        frame[(pos - HEADER_BITS) / 8] |= 1 << ((pos - HEADER_BITS) % 8);
      }
    }
    mark = !mark;
  }
  if (header != HEADER)
    return {};

  uint8_t values[FRAME_BYTES];
  for (uint8_t i = 0; i < FRAME_BYTES; i++) {
    const int8_t value = line_decode(frame[i]);
    if (value < 0)
      return {};
    values[i] = value;
  }
  const uint8_t *message = values + 1;
  if (frame_crc(frame) != (values[FRAME_BYTES - 1] << 6 | values[0]) || message[0] != MODE_COLOR)
    return {};
  return PixMobData{
      .red = uint8_t(message[2] << 2),
      .green = uint8_t(message[1] << 2),
      .blue = uint8_t(message[3] << 2),
      .group = message[6],
      .attack = uint8_t(message[4] >> 3),
      .hold = uint8_t(message[5] & 0x07),
      .release = uint8_t(message[5] >> 3),
      .random = uint8_t(message[4] & 0x07),
  };
}

optional<PixMobData> PixMobProtocol::decode(RemoteReceiveData src) {
  // Half a bit is the widest window that still tells runs of different lengths apart
  src.set_tolerance(BIT_US / 2, TOLERANCE_MODE_TIME);
  // A buffer can hold noise and several frames, so a failed match is retried one timing later
  for (int32_t start = 0; start < src.size(); start++) {
    src.reset();
    src.advance(start);
    auto data = decode_frame(src);
    if (data.has_value())
      return data;
  }
  return {};
}

void PixMobProtocol::dump(const PixMobData &data) {
  ESP_LOGI(TAG, "Received PixMob: red=%u, green=%u, blue=%u, group=%u, attack=%u, hold=%u, release=%u, random=%u",
           data.red, data.green, data.blue, data.group, data.attack, data.hold, data.release, data.random);
}

}  // namespace esphome::remote_base
