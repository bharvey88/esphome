#pragma once

/* Derived from WLED 16.0.1 wled00/FX.h (class Segment), wled00/FX_fcn.cpp and
 * wled00/FX_2Dfcn.cpp.
 * Copyright (c) 2016 Harm Aldick, Segment class (c) 2022 Blaz Kristan (@blazoncek),
 * (c) 2016-present Christian Schwinne and individual WLED contributors.
 * Licensed under the EUPL v. 1.2 or later, distributed here under GPLv3 per the
 * EUPL Article 5 compatibility clause.
 *
 * This is the whole surface an effect sees. Effect porters should never need to
 * change this file: if something an effect needs is missing, that is a bug in this
 * header, not something to work around in the effect. See PORTING.md.
 */

#include <cstdint>

#include "wf_canvas.h"
#include "wf_color.h"
#include "wf_math.h"
#include "wf_palettes.h"

namespace esphome {
namespace wled_fx {

// How a 1D effect is laid out on a 2D canvas. Same numbering as WLED's
// mapping1D2D_t so the metadata key m12 carries over unchanged.
enum Mapping1D2D : uint8_t {
  M12_PIXELS = 0,
  M12_P_BAR = 1,
  M12_P_ARC = 2,
  M12_P_CORNER = 3,
  M12_S_PINWHEEL = 4,
};

// WLED's nominal frame period at its default WLED_FPS of 42.
inline constexpr uint32_t FRAMETIME = 1000 / 42;

// WLED FX.h's FRAMETIME_FIXED: the frame period at WLED_FPS regardless of the
// rate the strip is managing. Effects that rate limit themselves use this.
inline constexpr uint32_t FRAMETIME_FIXED = 1000 / 42;

/* WLED FX.h:101, MAX_SEGMENT_DATA / MAX_NUM_SEGMENTS on a plain ESP32. Effects
 * use it as the upper bound on how many particles or sparks they allocate. */
inline constexpr unsigned FAIR_DATA_PER_SEG = (64 * 1024) / 32;

// WLED const.h: the number of colour slots a segment carries.
inline constexpr unsigned NUM_COLORS = 3;

class Segment {
 public:
  // --- user controls, names and ranges as in WLED ------------------------------
  uint8_t speed{128};
  uint8_t intensity{128};
  uint8_t custom1{128};
  uint8_t custom2{128};
  uint8_t custom3{16};  // 0 to 31
  bool check1{false};
  bool check2{false};
  bool check3{false};
  uint8_t palette{0};
  uint32_t colors[3]{0xFFAA00, 0x000000, 0x000000};
  uint8_t map1d2d{M12_PIXELS};
  // 0 wrap when moving, 1 always wrap, 2 never wrap, 3 no interpolation
  uint8_t palette_blend{0};

  // --- runtime scratch, zeroed when the effect changes --------------------------
  uint32_t step{0};
  uint32_t call{0};
  uint16_t aux0{0};
  uint16_t aux1{0};
  uint8_t *data{nullptr};

  // Frame timestamp in milliseconds. Every effect in a frame sees the same value.
  uint32_t now{0};
  // The same instant in microseconds, for the handful of upstream effects that
  // call micros() for sub-millisecond pacing. Derived from `now`, so it steps in
  // 1000 microsecond jumps and the host simulator stays reproducible.
  uint32_t now_us{0};

  ~Segment();

  // Binds the canvas and allocates the one row of scratch that move_x / move_y
  // need. Call once at setup, after the canvas is allocated.
  bool set_canvas(Canvas *canvas);
  Canvas *canvas() const { return this->canvas_; }

  // Called once per frame before the effect function. Resolves the palette.
  void begin_draw(const CRGBPalette16 &random_palette);

  // Zeroes the runtime scratch and frees the effect data. Call on effect change.
  void reset();

  // --- geometry -----------------------------------------------------------------
  uint16_t width() const { return this->canvas_ != nullptr ? this->canvas_->width() : 0; }
  uint16_t height() const { return this->canvas_ != nullptr ? this->canvas_->height() : 0; }
  bool is_2d() const { return this->width() > 1 && this->height() > 1; }
  // Raw pixel count of the canvas, what the fade and blur helpers walk.
  size_t raw_length() const { return this->canvas_ != nullptr ? this->canvas_->size() : 0; }
  // Virtual 1D length, which depends on the 1D-to-2D mapping. WLED's SEGLEN.
  unsigned length() const;
  bool is_active() const { return this->canvas_ != nullptr && this->canvas_->is_allocated(); }

  // WLED's nrOfVStrips() / indexToVStrip(). In M12_P_BAR a 1D effect is run once
  // per matrix column, with the column number packed into the high half of the index.
  unsigned nr_of_v_strips() const { return (this->is_2d() && this->map1d2d == M12_P_BAR) ? this->width() : 1; }
  static int index_to_v_strip(int i, int n) { return i | (static_cast<int>(n + 1) << 16); }

  // WLED's getPinwheelLength(). A multiple of 8, which stops the rays overdrawing.
  static int pinwheel_length(int vw, int vh) { return ((vw > vh ? vw : vh) + 15) & ~7; }

  // --- colour and palette --------------------------------------------------------
  uint32_t color(unsigned i) const { return this->colors[i < 3 ? i : 0]; }
  const CRGBPalette16 &palette_ref() const { return this->current_palette_; }
  bool palette_solid_wrap() const { return this->palette_blend == 1 || this->palette_blend == 3; }
  bool palette_moving_wrap() const {
    return !(this->palette_blend == 2 || (this->palette_blend == 0 && this->speed == 0));
  }
  uint32_t color_from_palette(uint16_t i, bool mapping, bool moving, uint8_t mcol, uint8_t pbri = 255) const;
  uint32_t color_wheel(uint8_t pos) const;

  // --- effect scratch data -------------------------------------------------------
  // Allocated on effect start only, never per frame. Zero filled. A repeat call
  // with the same length is a no-op, matching WLED.
  bool allocate_data(size_t len);
  void deallocate_data();
  size_t data_size() const { return this->data_len_; }

  // --- raw pixel access ----------------------------------------------------------
  // No mapping, no bounds check. The fade, blur and move helpers use these.
  void set_pixel_color_raw(unsigned i, uint32_t c) const { this->canvas_->set(i, c); }
  uint32_t get_pixel_color_raw(unsigned i) const { return this->canvas_->get(i); }
  void set_pixel_color_xy_raw(unsigned x, unsigned y, uint32_t c) const {
    this->canvas_->set(x + y * this->width(), c);
  }
  uint32_t get_pixel_color_xy_raw(unsigned x, unsigned y) const { return this->canvas_->get(x + y * this->width()); }

  // --- 1D pixel access -----------------------------------------------------------
  void set_pixel_color(int n, uint32_t c) const;
  void set_pixel_color(unsigned n, uint32_t c) const { this->set_pixel_color(static_cast<int>(n), c); }
  void set_pixel_color(int n, uint8_t r, uint8_t g, uint8_t b, uint8_t w = 0) const {
    this->set_pixel_color(n, RGBW32(r, g, b, w));
  }
  void set_pixel_color(int n, CRGB c) const { this->set_pixel_color(n, RGBW32(c.r, c.g, c.b, 0)); }
  uint32_t get_pixel_color(int i) const;
  void blend_pixel_color(int n, uint32_t color, uint8_t blend) const {
    this->set_pixel_color(n, color_blend(this->get_pixel_color(n), color, blend));
  }
  void blend_pixel_color(int n, CRGB c, uint8_t blend) const {
    this->blend_pixel_color(n, RGBW32(c.r, c.g, c.b, 0), blend);
  }
  void add_pixel_color(int n, uint32_t color, bool preserve_cr = true) const {
    this->set_pixel_color(n, color_add(this->get_pixel_color(n), color, preserve_cr));
  }
  void add_pixel_color(int n, uint8_t r, uint8_t g, uint8_t b, uint8_t w = 0, bool preserve_cr = true) const {
    this->add_pixel_color(n, RGBW32(r, g, b, w), preserve_cr);
  }
  void add_pixel_color(int n, CRGB c, bool preserve_cr = true) const {
    this->add_pixel_color(n, RGBW32(c.r, c.g, c.b, 0), preserve_cr);
  }
  void fade_pixel_color(int n, uint8_t fade) const {
    this->set_pixel_color(n, color_fade(this->get_pixel_color(n), fade, true));
  }

  // --- 2D pixel access -----------------------------------------------------------
  void set_pixel_color_xy(int x, int y, uint32_t c) const;
  void set_pixel_color_xy(unsigned x, unsigned y, uint32_t c) const {
    this->set_pixel_color_xy(static_cast<int>(x), static_cast<int>(y), c);
  }
  void set_pixel_color_xy(int x, int y, uint8_t r, uint8_t g, uint8_t b, uint8_t w = 0) const {
    this->set_pixel_color_xy(x, y, RGBW32(r, g, b, w));
  }
  void set_pixel_color_xy(int x, int y, CRGB c) const { this->set_pixel_color_xy(x, y, RGBW32(c.r, c.g, c.b, 0)); }
  uint32_t get_pixel_color_xy(int x, int y) const;
  void blend_pixel_color_xy(int x, int y, uint32_t color, uint8_t blend) const {
    this->set_pixel_color_xy(x, y, color_blend(this->get_pixel_color_xy(x, y), color, blend));
  }
  void blend_pixel_color_xy(int x, int y, CRGB c, uint8_t blend) const {
    this->blend_pixel_color_xy(x, y, RGBW32(c.r, c.g, c.b, 0), blend);
  }
  void add_pixel_color_xy(int x, int y, uint32_t color, bool preserve_cr = true) const {
    this->set_pixel_color_xy(x, y, color_add(this->get_pixel_color_xy(x, y), color, preserve_cr));
  }
  void add_pixel_color_xy(int x, int y, uint8_t r, uint8_t g, uint8_t b, uint8_t w = 0, bool preserve_cr = true) const {
    this->add_pixel_color_xy(x, y, RGBW32(r, g, b, w), preserve_cr);
  }
  void add_pixel_color_xy(int x, int y, CRGB c, bool preserve_cr = true) const {
    this->add_pixel_color_xy(x, y, RGBW32(c.r, c.g, c.b, 0), preserve_cr);
  }
  void fade_pixel_color_xy(int x, int y, uint8_t fade) const {
    this->set_pixel_color_xy(x, y, color_fade(this->get_pixel_color_xy(x, y), fade, true));
  }

  // --- whole canvas operations -----------------------------------------------------
  void fill(uint32_t c) const;
  void fill_solid(CRGB c) const { this->fill(RGBW32(c.r, c.g, c.b, 0)); }
  void clear() const { this->fill(BLACK); }
  void fade_out(uint8_t rate) const;
  void fade_to_secondary_by(uint8_t fade_by) const;
  void fade_to_black_by(uint8_t fade_by) const;
  void blur(uint8_t blur_amount, bool smear = false) const;
  void blur2d(uint8_t blur_x, uint8_t blur_y, bool smear = false) const;
  void blur_rows(uint8_t blur_amount, bool smear = false) const { this->blur2d(blur_amount, 0, smear); }
  void blur_cols(uint8_t blur_amount, bool smear = false) const { this->blur2d(0, blur_amount, smear); }
  void move_x(int delta, bool wrap = false) const;
  void move_y(int delta, bool wrap = false) const;
  void move(unsigned dir, unsigned delta, bool wrap = false) const;

  // --- 2D drawing ------------------------------------------------------------------
  void draw_circle(int cx, int cy, uint8_t radius, uint32_t c, bool soft = false) const;
  void draw_circle(int cx, int cy, uint8_t radius, CRGB c, bool soft = false) const {
    this->draw_circle(cx, cy, radius, RGBW32(c.r, c.g, c.b, 0), soft);
  }
  void fill_circle(int cx, int cy, uint8_t radius, uint32_t c, bool soft = false) const;
  void fill_circle(int cx, int cy, uint8_t radius, CRGB c, bool soft = false) const {
    this->fill_circle(cx, cy, radius, RGBW32(c.r, c.g, c.b, 0), soft);
  }
  void draw_line(int x0, int y0, int x1, int y1, uint32_t c, bool soft = false) const;
  void draw_line(int x0, int y0, int x1, int y1, CRGB c, bool soft = false) const {
    this->draw_line(x0, y0, x1, y1, RGBW32(c.r, c.g, c.b, 0), soft);
  }
  void wu_pixel(uint32_t x, uint32_t y, CRGB c) const;

 protected:
  // WLED's setPinwheelParameters(). Fills the start point and the sine and cosine
  // steps for two consecutive rays, all in 14 bit fixed point.
  void pinwheel_parameters_(int i, int vw, int vh, int &startx, int &starty, int *cos_val, int *sin_val,
                            bool get_pixel = false) const;

  Canvas *canvas_{nullptr};
  CRGBPalette16 current_palette_{};
  size_t data_len_{0};
  uint32_t *scratch_{nullptr};  // one row or column, for move_x / move_y
  // Bresenham coordinates for the two rays of the pinwheel mapping. Upstream puts
  // these on the stack as variable length arrays, which this port does not allow,
  // so they are allocated once with the canvas: 2 lines x (max(w, h) + 2) points
  // x 2 coordinates.
  uint16_t *pinwheel_coords_{nullptr};
  unsigned pinwheel_max_line_{0};
  // The two previously drawn ray numbers, so adjacent rays do not double draw.
  mutable int prev_rays_[2]{0x7FFFFFFF, 0x7FFFFFFF};
};

}  // namespace wled_fx
}  // namespace esphome
