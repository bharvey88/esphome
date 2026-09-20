#include <gtest/gtest.h>

#include <cstring>

#include "esphome/components/wled_fx/wf_palettes.h"
#include "esphome/components/wled_fx/wf_registry.h"

namespace esphome::wled_fx {

// Codegen emits this table into the generated main.cpp. The unit test build has no
// codegen, so it names the one group this component currently ships.
extern const EffectGroup EFFECT_GROUP_1D_A;
const EffectGroup *const LINKED_EFFECT_GROUPS[] = {&EFFECT_GROUP_1D_A};
const unsigned LINKED_EFFECT_GROUP_COUNT = 1;

namespace testing {

TEST(EffectMetadata, NameIsEverythingBeforeTheFirstAt) {
  const EffectInfo info{"Fire 2012@Cooling,Spark rate,,2D Blur,Boost;;!;1;pal=35,sx=64", nullptr};
  char buffer[32]{};
  const size_t written = effect_name(info, buffer, sizeof(buffer));
  EXPECT_STREQ(buffer, "Fire 2012");
  EXPECT_EQ(written, 9u);
}

TEST(EffectMetadata, NameWithNoMetadataIsTheWholeString) {
  const EffectInfo info{"Solid", nullptr};
  char buffer[32]{};
  effect_name(info, buffer, sizeof(buffer));
  EXPECT_STREQ(buffer, "Solid");
}

TEST(EffectMetadata, NameIsTruncatedRatherThanOverflowing) {
  const EffectInfo info{"Fire 2012@x", nullptr};
  char buffer[5]{};
  effect_name(info, buffer, sizeof(buffer));
  EXPECT_STREQ(buffer, "Fire");
}

TEST(EffectMetadata, NameComparisonIgnoresCase) {
  const EffectInfo info{"Fire 2012@x", nullptr};
  EXPECT_TRUE(effect_name_equals(info, "fire 2012"));
  EXPECT_TRUE(effect_name_equals(info, "FIRE 2012"));
  EXPECT_FALSE(effect_name_equals(info, "Fire 201"));
  EXPECT_FALSE(effect_name_equals(info, "Fire 20123"));
}

TEST(EffectMetadata, DefaultsAreReadFromTheLastGroup) {
  const EffectInfo info{"X@a;b;c;1;pal=35,sx=64,ix=160,c2=128,m12=1,o1=1", nullptr};
  const EffectDefaults d = effect_defaults(info);
  EXPECT_EQ(d.palette, 35);
  EXPECT_EQ(d.speed, 64);
  EXPECT_EQ(d.intensity, 160);
  EXPECT_EQ(d.custom2, 128);
  EXPECT_EQ(d.map1d2d, 1);
  EXPECT_TRUE(d.check1);
}

TEST(EffectMetadata, MissingKeysKeepTheWledDefaults) {
  const EffectInfo info{"X@a;b;c", nullptr};
  const EffectDefaults d = effect_defaults(info);
  EXPECT_EQ(d.speed, 128);
  EXPECT_EQ(d.intensity, 128);
  EXPECT_EQ(d.custom3, 16);
  EXPECT_FALSE(d.check1);
}

TEST(EffectMetadata, DimensionFlagsComeFromTheFourthGroup) {
  EXPECT_EQ(effect_defaults(EffectInfo{"X@a;b;c;2", nullptr}).flags & EFFECT_FLAG_2D, EFFECT_FLAG_2D);
  EXPECT_EQ(effect_defaults(EffectInfo{"X@a;b;c;1", nullptr}).flags & EFFECT_FLAG_1D, EFFECT_FLAG_1D);
  // An absent group means 1D only, which is what most of WLED's table relies on.
  EXPECT_EQ(effect_defaults(EffectInfo{"X@a;b;c", nullptr}).flags, EFFECT_FLAG_1D);
}

TEST(EffectRegistryTest, ShippedEffectsAreFindableByName) {
  ASSERT_EQ(EffectRegistry::count(), 7u);
  for (const char *name : {"Solid", "Blink", "Rainbow", "Pride 2015", "Fire 2012", "Noise 1", "Plasma"}) {
    const EffectInfo *info = EffectRegistry::find(name);
    ASSERT_NE(info, nullptr) << name;
    EXPECT_NE(info->fn, nullptr) << name;
  }
  EXPECT_EQ(EffectRegistry::find("No Such Effect"), nullptr);
  EXPECT_LT(EffectRegistry::index_of("No Such Effect"), 0);
}

TEST(EffectRegistryTest, IndexOfAgreesWithAt) {
  const int index = EffectRegistry::index_of("Fire 2012");
  ASSERT_GE(index, 0);
  EXPECT_EQ(EffectRegistry::at(static_cast<size_t>(index)), EffectRegistry::find("Fire 2012"));
  EXPECT_EQ(EffectRegistry::at(EffectRegistry::count()), nullptr);
}

TEST(PaletteTest, NamesResolveBothWaysAndAreCaseInsensitive) {
  const int id = palette_id_by_name("Fire");
  ASSERT_GE(id, 0);
  EXPECT_STREQ(PALETTE_NAMES[id], "Fire");
  EXPECT_EQ(palette_id_by_name("fire"), id);
  EXPECT_LT(palette_id_by_name("Not A Palette"), 0);
}

TEST(PaletteTest, EveryPaletteIdLoadsWithoutReadingPastTheTable) {
  const uint32_t colors[3] = {0xFFAA00, 0x0000FF, 0x00FF00};
  const CRGBPalette16 random_palette = generate_random_palette();
  for (uint8_t pal = 0; pal < palette_count(); pal++) {
    CRGBPalette16 target{};
    load_palette(target, pal, colors, random_palette);
    EXPECT_NE(PALETTE_NAMES[pal], nullptr) << "palette " << static_cast<int>(pal);
  }
}

}  // namespace testing
}  // namespace esphome::wled_fx
