#include <array>
#include <vector>

#include <gtest/gtest.h>

#include "esphome/components/remote_base/pixmob_protocol.h"

namespace esphome::remote_base::testing {

namespace {

using Frame = std::array<uint8_t, 9>;

constexpr PixMobData KNOWN{
    .red = 255, .green = 128, .blue = 64, .group = 17, .attack = 1, .hold = 2, .release = 3, .random = 4};
// The frames below were computed with genframe.py, a Python port of the reference pixmob_cement.cpp
constexpr Frame KNOWN_FRAME{0x56, 0x21, 0x31, 0x65, 0x92, 0x64, 0x2A, 0xB2, 0x84};
const RawTimings KNOWN_TIMINGS{
    500, -500, 500,  -500,  500,  -500,  500, -500,  500,  -500,  500, -500,  500, -500,  500,  -1000,
    500, -500, 1000, -500,  500,  -500,  500, -500,  500,  -2000, 500, -1000, 500, -1500, 1000, -1000,
    500, -500, 500,  -1000, 1000, -1000, 500, -1000, 500,  -1000, 500, -1000, 500, -1000, 1000, -1000,
    500, -500, 500,  -500,  500,  -1500, 500, -1000, 1000, -500,  500, -1000, 500, -2000, 500,  -500,
};
// Black with the default envelope; its last bit is a zero
constexpr PixMobData BLACK{
    .red = 0, .green = 0, .blue = 0, .group = 0, .attack = 0, .hold = 7, .release = 2, .random = 0};
constexpr Frame BLACK_FRAME{0x6A, 0x21, 0x21, 0x21, 0x21, 0x21, 0x96, 0x21, 0x42};
// KNOWN with mode 1, and KNOWN with 0x55 (not a line code) as its blue byte, each with a matching CRC
constexpr Frame MODE_1_FRAME{0x26, 0x35, 0x31, 0x65, 0x92, 0x64, 0x2A, 0xB2, 0xAC};
constexpr Frame NOT_A_LINE_CODE_FRAME{0x64, 0x21, 0x31, 0x55, 0x92, 0x64, 0x2A, 0xB2, 0x4A};

RawTimings encode(const PixMobData &data) {
  RemoteTransmitData tx;
  PixMobProtocol().encode(&tx, data);
  return tx.get_data();
}

optional<PixMobData> decode(const RawTimings &timings) {
  return PixMobProtocol().decode(RemoteReceiveData(timings, 25, TOLERANCE_MODE_PERCENTAGE));
}

// Preamble and sync, the bytes least significant bit first, then a space if the frame ends high
RawTimings frame_timings(const Frame &frame) {
  constexpr uint32_t header = 0x25555;  // preamble 0x55 twice, then sync bits 0 and 1
  std::vector<bool> bits;
  bits.reserve(18 + frame.size() * 8);
  for (int i = 0; i < 18; i++) {
    bits.push_back((header >> i) & 1);
  }
  for (uint8_t byte : frame) {
    for (int i = 0; i < 8; i++) {
      bits.push_back((byte >> i) & 1);
    }
  }
  RawTimings timings;
  for (size_t i = 0; i < bits.size();) {
    size_t run = 1;
    while (i + run < bits.size() && bits[i + run] == bits[i]) {
      run++;
    }
    timings.push_back(bits[i] ? int32_t(run * 500) : -int32_t(run * 500));
    i += run;
  }
  if (bits.back()) {
    timings.push_back(-500);
  }
  return timings;
}

void expect_decodes_to(const RawTimings &timings, const PixMobData &expected) {
  auto decoded = decode(timings);
  ASSERT_TRUE(decoded.has_value());
  if (decoded.has_value()) {
    EXPECT_EQ(decoded->red, expected.red & 0xFC);
    EXPECT_EQ(decoded->green, expected.green & 0xFC);
    EXPECT_EQ(decoded->blue, expected.blue & 0xFC);
    EXPECT_EQ(decoded->group, expected.group);
    EXPECT_EQ(decoded->attack, expected.attack);
    EXPECT_EQ(decoded->hold, expected.hold);
    EXPECT_EQ(decoded->release, expected.release);
    EXPECT_EQ(decoded->random, expected.random);
  }
}

}  // namespace

TEST(PixMobProtocolTest, EncodesKnownFrame) {
  EXPECT_EQ(encode(KNOWN), KNOWN_TIMINGS);
  EXPECT_EQ(frame_timings(KNOWN_FRAME), KNOWN_TIMINGS);
}

TEST(PixMobProtocolTest, EncodesFrameEndingLow) {
  const RawTimings timings = encode(BLACK);
  EXPECT_EQ(timings, frame_timings(BLACK_FRAME));
  EXPECT_EQ(timings.back(), -500);
}

TEST(PixMobProtocolTest, DecodesKnownFrame) { expect_decodes_to(KNOWN_TIMINGS, KNOWN); }

TEST(PixMobProtocolTest, RoundTrips) {
  for (const PixMobData &sent : {
           BLACK,
           PixMobData{
               .red = 255, .green = 255, .blue = 255, .group = 31, .attack = 7, .hold = 7, .release = 7, .random = 7},
           PixMobData{
               .red = 0, .green = 255, .blue = 0, .group = 31, .attack = 0, .hold = 7, .release = 2, .random = 0},
           PixMobData{.red = 3, .green = 7, .blue = 251, .group = 1, .attack = 4, .hold = 0, .release = 5, .random = 6},
       }) {
    expect_decodes_to(encode(sent), sent);
  }
}

TEST(PixMobProtocolTest, MatchesOnTheSixBitsSent) {
  PixMobData configured = KNOWN;
  configured.red = 252;
  EXPECT_TRUE(KNOWN == configured);
  configured.red = 248;
  EXPECT_FALSE(KNOWN == configured);
}

TEST(PixMobProtocolTest, RejectsBadCrc) {
  Frame frame = KNOWN_FRAME;
  frame[0] = 0x35;  // the line code for 1 instead of 9
  EXPECT_FALSE(decode(frame_timings(frame)).has_value());
}

TEST(PixMobProtocolTest, RejectsNonZeroMode) { EXPECT_FALSE(decode(frame_timings(MODE_1_FRAME)).has_value()); }

TEST(PixMobProtocolTest, RejectsByteOutsideLineCode) {
  EXPECT_FALSE(decode(frame_timings(NOT_A_LINE_CODE_FRAME)).has_value());
  Frame frame = KNOWN_FRAME;
  frame[0] = 0x55;
  EXPECT_FALSE(decode(frame_timings(frame)).has_value());
}

TEST(PixMobProtocolTest, DecodesWhenLastZerosMergeIntoTheGap) {
  RawTimings timings = frame_timings(BLACK_FRAME);
  timings.pop_back();
  expect_decodes_to(timings, BLACK);
  timings.push_back(-20000);
  expect_decodes_to(timings, BLACK);
}

TEST(PixMobProtocolTest, SkipsLeadingNoise) {
  RawTimings timings{300, -1200, 150, -90};
  timings.insert(timings.end(), KNOWN_TIMINGS.begin(), KNOWN_TIMINGS.end());
  expect_decodes_to(timings, KNOWN);
}

TEST(PixMobProtocolTest, AcceptsNoiseMergedIntoFirstMark) {
  RawTimings timings = KNOWN_TIMINGS;
  timings[0] = 3100;
  expect_decodes_to(timings, KNOWN);
}

TEST(PixMobProtocolTest, UsesRepeatWhenFirstFrameIsCorrupt) {
  Frame bad = KNOWN_FRAME;
  bad[0] = 0x35;
  RawTimings timings = frame_timings(bad);
  timings.insert(timings.end(), KNOWN_TIMINGS.begin(), KNOWN_TIMINGS.end());
  expect_decodes_to(timings, KNOWN);
}

TEST(PixMobProtocolTest, RejectsTruncatedFrame) {
  RawTimings timings = KNOWN_TIMINGS;
  timings.resize(timings.size() - 6);  // ends on a space
  EXPECT_FALSE(decode(timings).has_value());
  timings.resize(timings.size() - 1);  // ends on a mark
  EXPECT_FALSE(decode(timings).has_value());
}

TEST(PixMobProtocolTest, RejectsRunLongerThanTheLineCodeAllows) {
  RawTimings timings = KNOWN_TIMINGS;
  timings[20] = 2600;  // 5 bits
  EXPECT_FALSE(decode(timings).has_value());
}

TEST(PixMobProtocolTest, RejectsMarkRunningPastTheFrame) {
  RawTimings timings = KNOWN_TIMINGS;
  timings[timings.size() - 2] = 1500;  // the last bit is a one, followed by two more
  EXPECT_FALSE(decode(timings).has_value());
}

TEST(PixMobProtocolTest, RejectsBadPreamble) {
  // Bits 4 to 7 become 1, 1, 0, 0 instead of 1, 0, 1, 0, so only the preamble is wrong
  RawTimings timings = KNOWN_TIMINGS;
  timings[4] = 1000;
  timings[5] = -1000;
  timings.erase(timings.begin() + 6, timings.begin() + 8);
  EXPECT_FALSE(decode(timings).has_value());
}

TEST(PixMobProtocolTest, ToleratesMarksStretchedBy200us) {
  RawTimings timings = KNOWN_TIMINGS;
  for (int32_t &timing : timings) {
    timing += 200;
  }
  expect_decodes_to(timings, KNOWN);
}

TEST(PixMobProtocolTest, RejectsShortPreamble) {
  RawTimings timings = KNOWN_TIMINGS;
  timings.erase(timings.begin(), timings.begin() + 2);
  EXPECT_FALSE(decode(timings).has_value());
}

TEST(PixMobProtocolTest, DumpCompiles) { PixMobProtocol().dump(KNOWN); }

}  // namespace esphome::remote_base::testing
