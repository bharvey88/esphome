#include <gtest/gtest.h>

#include "esphome/components/wled_fx/wf_engine.h"
#include "esphome/components/wled_fx/wf_registry.h"

namespace esphome::wled_fx::testing {

namespace {

// Geometries an effect has to survive: a plain strip, a square matrix, a matrix
// that is not square, and the degenerate single pixel.
struct Geometry {
  uint16_t width;
  uint16_t height;
};

constexpr Geometry GEOMETRIES[] = {{60, 1}, {16, 16}, {31, 17}, {1, 1}};

// Runs one effect for `frames` frames and returns true if any pixel was lit.
bool run_effect(Engine &engine, size_t index, unsigned frames) {
  EXPECT_TRUE(engine.set_effect_index(index));
  bool lit = false;
  for (unsigned frame = 0; frame < frames; frame++) {
    engine.render(frame * 33 + 1);
    const Canvas &canvas = engine.canvas();
    for (size_t i = 0; i < canvas.size(); i++) {
      if ((canvas.get(i) & 0x00FFFFFFu) != 0) {
        lit = true;
        break;
      }
    }
  }
  return lit;
}

}  // namespace

// Every registered effect must run at every geometry without writing outside the
// framebuffer. The canvas keeps a sentinel word band either side of the pixels,
// so an overrun that a bounds check missed shows up here.
TEST(EffectSmoke, EveryEffectStaysInsideTheFramebuffer) {
  ASSERT_GT(EffectRegistry::count(), 0u);
  for (const Geometry &geom : GEOMETRIES) {
    for (size_t index = 0; index < EffectRegistry::count(); index++) {
      Engine engine;
      ASSERT_TRUE(engine.init(geom.width, geom.height));
      run_effect(engine, index, 120);
      char name[48]{};
      effect_name(*EffectRegistry::at(index), name, sizeof(name));
      EXPECT_TRUE(engine.canvas().guards_intact())
          << name << " overran the framebuffer at " << geom.width << "x" << geom.height;
    }
  }
}

// An effect that renders nothing at all is almost always a porting mistake, so
// the default parameters have to produce output on a normal strip.
TEST(EffectSmoke, EveryEffectLightsSomethingOnAPlainStrip) {
  Engine engine;
  ASSERT_TRUE(engine.init(60, 1));
  for (size_t index = 0; index < EffectRegistry::count(); index++) {
    char name[48]{};
    effect_name(*EffectRegistry::at(index), name, sizeof(name));
    EXPECT_TRUE(run_effect(engine, index, 120)) << name << " rendered nothing";
  }
}

TEST(EffectSmoke, EveryPaletteRunsUnderEveryEffect) {
  Engine engine;
  ASSERT_TRUE(engine.init(16, 16));
  for (size_t index = 0; index < EffectRegistry::count(); index++) {
    ASSERT_TRUE(engine.set_effect_index(index));
    for (size_t pal = 0; pal < palette_count(); pal++) {
      engine.set_palette(static_cast<uint8_t>(pal));
      engine.render(static_cast<uint32_t>(pal) * 33 + 1);
    }
    EXPECT_TRUE(engine.canvas().guards_intact()) << "effect " << index;
  }
}

// Selecting an effect by name is what the YAML `effect:` key does, and an unknown
// name has to leave the running effect alone rather than blanking the light.
TEST(EffectSmoke, UnknownNamesLeaveTheRunningEffectAlone) {
  Engine engine;
  ASSERT_TRUE(engine.init(60, 1));
  ASSERT_TRUE(engine.set_effect("Fire 2012"));
  const size_t before = engine.effect_index();
  EXPECT_FALSE(engine.set_effect("No Such Effect"));
  EXPECT_EQ(engine.effect_index(), before);
  EXPECT_FALSE(engine.set_palette_by_name("No Such Palette"));
}

// A pinned control must survive an effect change; anything not pinned goes back
// to the new effect's own metadata default.
TEST(EffectSmoke, PinnedControlsSurviveAnEffectChange) {
  Engine engine;
  ASSERT_TRUE(engine.init(60, 1));
  ASSERT_TRUE(engine.set_effect("Plasma"));
  engine.set_speed(7);
  ASSERT_TRUE(engine.set_effect("Fire 2012"));
  EXPECT_EQ(engine.segment().speed, 7);
  // Fire 2012's metadata carries sx=64, which is what an unpinned speed picks up.
  engine.clear_override(OVERRIDE_SPEED);
  ASSERT_TRUE(engine.set_effect("Plasma"));
  ASSERT_TRUE(engine.set_effect("Fire 2012"));
  EXPECT_EQ(engine.segment().speed, 64);
}

}  // namespace esphome::wled_fx::testing
