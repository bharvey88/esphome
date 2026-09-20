#include <gtest/gtest.h>

#include "esphome/components/wled_fx/wf_canvas.h"
#include "esphome/components/wled_fx/wf_segment.h"

namespace esphome::wled_fx::testing {

namespace {

// Canvas plus a Segment bound to it, which is the pairing every effect sees.
struct Frame {
  Frame(uint16_t width, uint16_t height) {
    EXPECT_TRUE(canvas.allocate(width, height));
    EXPECT_TRUE(seg.set_canvas(&canvas));
  }
  Canvas canvas;
  Segment seg;
};

}  // namespace

TEST(CanvasTest, AllocationSizesAndGuards) {
  Canvas canvas;
  ASSERT_TRUE(canvas.allocate(16, 8));
  EXPECT_EQ(canvas.width(), 16);
  EXPECT_EQ(canvas.height(), 8);
  EXPECT_EQ(canvas.size(), 128u);
  EXPECT_TRUE(canvas.is_allocated());
  EXPECT_TRUE(canvas.guards_intact());
}

TEST(CanvasTest, GuardsCatchAWriteOffTheEnd) {
  Canvas canvas;
  ASSERT_TRUE(canvas.allocate(8, 1));
  ASSERT_TRUE(canvas.guards_intact());
  canvas.pixels()[canvas.size()] = 0x123456;
  EXPECT_FALSE(canvas.guards_intact());
}

TEST(CanvasTest, ClearZeroesEveryPixel) {
  Canvas canvas;
  ASSERT_TRUE(canvas.allocate(4, 4));
  for (size_t i = 0; i < canvas.size(); i++)
    canvas.set(i, 0xFFFFFF);
  canvas.clear();
  for (size_t i = 0; i < canvas.size(); i++)
    EXPECT_EQ(canvas.get(i), 0u) << "pixel " << i;
  EXPECT_TRUE(canvas.guards_intact());
}

TEST(SegmentTest, OneDimensionalGeometry) {
  Frame frame(60, 1);
  EXPECT_FALSE(frame.seg.is_2d());
  EXPECT_EQ(frame.seg.length(), 60u);
  EXPECT_EQ(frame.seg.raw_length(), 60u);
  EXPECT_EQ(frame.seg.nr_of_v_strips(), 1u);
}

TEST(SegmentTest, TwoDimensionalGeometry) {
  Frame frame(16, 8);
  EXPECT_TRUE(frame.seg.is_2d());
  EXPECT_EQ(frame.seg.raw_length(), 128u);
  EXPECT_EQ(frame.seg.width(), 16);
  EXPECT_EQ(frame.seg.height(), 8);
}

TEST(SegmentTest, PixelBarMappingRunsTheEffectOncePerColumn) {
  Frame frame(16, 8);
  frame.seg.map1d2d = M12_P_BAR;
  EXPECT_EQ(frame.seg.nr_of_v_strips(), 16u);
  // A 1D effect addressing index i of strip n lands on column n.
  EXPECT_EQ(Segment::index_to_v_strip(3, 2), 3 | (3 << 16));
}

TEST(SegmentTest, PinwheelLengthIsAMultipleOfEight) {
  for (int w = 1; w <= 64; w++) {
    const int len = Segment::pinwheel_length(w, 16);
    EXPECT_EQ(len % 8, 0) << "width " << w;
    EXPECT_GE(len, w > 16 ? w : 16);
  }
}

TEST(SegmentTest, WritesStayInsideTheCanvas) {
  Frame frame(8, 4);
  frame.seg.set_pixel_color_xy(-1, -1, 0xFFFFFFu);
  frame.seg.set_pixel_color_xy(99, 99, 0xFFFFFFu);
  frame.seg.set_pixel_color(-5, 0xFFFFFFu);
  frame.seg.set_pixel_color(5000, 0xFFFFFFu);
  EXPECT_TRUE(frame.canvas.guards_intact());
  for (size_t i = 0; i < frame.canvas.size(); i++)
    EXPECT_EQ(frame.canvas.get(i), 0u) << "pixel " << i;
}

TEST(SegmentTest, ReadBackIsLossless) {
  Frame frame(8, 4);
  frame.seg.set_pixel_color_xy(3, 2, 0x123456u);
  EXPECT_EQ(frame.seg.get_pixel_color_xy(3, 2), 0x123456u);
}

TEST(SegmentTest, FillPaintsEveryPixel) {
  Frame frame(8, 4);
  frame.seg.fill(0x00FF00u);
  for (size_t i = 0; i < frame.canvas.size(); i++)
    EXPECT_EQ(frame.canvas.get(i) & 0xFFFFFFu, 0x00FF00u) << "pixel " << i;
  EXPECT_TRUE(frame.canvas.guards_intact());
}

TEST(SegmentTest, FadeToBlackReachesBlackAndStaysThere) {
  Frame frame(8, 1);
  frame.seg.fill(0xFFFFFFu);
  for (int i = 0; i < 200; i++)
    frame.seg.fade_to_black_by(64);
  for (size_t i = 0; i < frame.canvas.size(); i++)
    EXPECT_EQ(frame.canvas.get(i) & 0xFFFFFFu, 0u) << "pixel " << i;
}

TEST(SegmentTest, EffectDataIsAllocatedOnceAndZeroed) {
  Frame frame(8, 1);
  ASSERT_TRUE(frame.seg.allocate_data(64));
  ASSERT_NE(frame.seg.data, nullptr);
  EXPECT_EQ(frame.seg.data_size(), 64u);
  for (size_t i = 0; i < 64; i++)
    EXPECT_EQ(frame.seg.data[i], 0) << "byte " << i;

  frame.seg.data[0] = 0xAB;
  const uint8_t *before = frame.seg.data;
  // A repeat request for the same size must not reallocate or clear, which is what
  // lets effects call it every frame the way WLED does.
  ASSERT_TRUE(frame.seg.allocate_data(64));
  EXPECT_EQ(frame.seg.data, before);
  EXPECT_EQ(frame.seg.data[0], 0xAB);

  frame.seg.deallocate_data();
  EXPECT_EQ(frame.seg.data, nullptr);
  EXPECT_EQ(frame.seg.data_size(), 0u);
}

TEST(SegmentTest, ResetClearsScratchAndFreesEffectData) {
  Frame frame(8, 1);
  ASSERT_TRUE(frame.seg.allocate_data(32));
  frame.seg.step = 1234;
  frame.seg.aux0 = 7;
  frame.seg.aux1 = 9;
  frame.seg.call = 42;
  frame.seg.reset();
  EXPECT_EQ(frame.seg.data, nullptr);
  EXPECT_EQ(frame.seg.step, 0u);
  EXPECT_EQ(frame.seg.aux0, 0);
  EXPECT_EQ(frame.seg.aux1, 0);
  EXPECT_EQ(frame.seg.call, 0u);
}

}  // namespace esphome::wled_fx::testing
