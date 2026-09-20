/* See wf_segment.h for the WLED attribution and licence notice. */

#include "wf_segment.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace esphome {
namespace wled_fx {

namespace {

// Fixed point scaling factor used by the pinwheel mapping, 14 bits of fraction.
constexpr int PINWHEEL_FIXED_SCALE = 16384;

}  // namespace

Segment::~Segment() {
  this->deallocate_data();
  if (this->scratch_ != nullptr)
    platform_free(this->scratch_);
  if (this->pinwheel_coords_ != nullptr)
    platform_free(this->pinwheel_coords_);
}

bool Segment::set_canvas(Canvas *canvas) {
  this->canvas_ = canvas;
  if (this->scratch_ != nullptr) {
    platform_free(this->scratch_);
    this->scratch_ = nullptr;
  }
  if (this->pinwheel_coords_ != nullptr) {
    platform_free(this->pinwheel_coords_);
    this->pinwheel_coords_ = nullptr;
  }
  this->pinwheel_max_line_ = 0;
  if (canvas == nullptr || !canvas->is_allocated())
    return false;
  const size_t words = canvas->width() > canvas->height() ? canvas->width() : canvas->height();
  this->scratch_ = static_cast<uint32_t *>(platform_alloc(words * sizeof(uint32_t)));
  if (this->scratch_ == nullptr)
    return false;
  // Pixels drawn along a ray is always fewer than dx or dy, plus one pair for
  // rounding. Two rays, x and y interleaved.
  this->pinwheel_max_line_ = words + 2;
  this->pinwheel_coords_ = static_cast<uint16_t *>(platform_alloc(4 * this->pinwheel_max_line_ * sizeof(uint16_t)));
  return this->pinwheel_coords_ != nullptr;
}

void Segment::pinwheel_parameters_(int i, int vw, int vh, int &startx, int &starty, int *cos_val, int *sin_val,
                                   bool get_pixel) const {
  const int steps = pinwheel_length(vw, vh);
  const int base_angle = ((0xFFFF + steps / 2) / steps);  // 360 degrees / steps, 16 bit, rounded
  int rotate = 0;
  if (get_pixel)
    rotate = base_angle / 2;  // rotate by half a ray width when reading pixel colour
  for (int k = 0; k < 2; k++) {
    const int angle = (i + k) * base_angle + rotate;
    // Explicit shifts: dividing negative numbers is not equivalent, and the
    // rounding error is acceptable. cos16_t output is -0x7FFF to +0x7FFF.
    cos_val[k] = (cos16_t(angle) * PINWHEEL_FIXED_SCALE) >> 15;
    sin_val[k] = (sin16_t(angle) * PINWHEEL_FIXED_SCALE) >> 15;
  }
  startx = (vw * PINWHEEL_FIXED_SCALE) / 2;
  starty = (vh * PINWHEEL_FIXED_SCALE) / 2;
}

void Segment::begin_draw(const CRGBPalette16 &random_palette) {
  load_palette(this->current_palette_, this->palette, this->colors, random_palette);
}

void Segment::reset() {
  this->step = 0;
  this->call = 0;
  this->aux0 = 0;
  this->aux1 = 0;
  this->prev_rays_[0] = 0x7FFFFFFF;
  this->prev_rays_[1] = 0x7FFFFFFF;
  this->deallocate_data();
  if (this->canvas_ != nullptr)
    this->canvas_->clear();
}

bool Segment::allocate_data(size_t len) {
  if (len == 0) {
    this->deallocate_data();
    return false;
  }
  if (this->data != nullptr && this->data_len_ == len)
    return true;  // already the right size, matches WLED
  this->deallocate_data();
  this->data = static_cast<uint8_t *>(platform_alloc(len));
  if (this->data == nullptr)
    return false;
  this->data_len_ = len;
  return true;
}

void Segment::deallocate_data() {
  if (this->data != nullptr) {
    platform_free(this->data);
    this->data = nullptr;
  }
  this->data_len_ = 0;
}

unsigned Segment::length() const {
  const unsigned w = this->width();
  const unsigned h = this->height();
  if (w == 0)
    return 0;
  if (this->is_2d()) {
    switch (this->map1d2d) {
      case M12_P_BAR:
        return h;
      case M12_P_CORNER:
        return w > h ? w : h;
      case M12_P_ARC:
        return sqrt32_bw(w * w + h * h);
      case M12_S_PINWHEEL:
        return pinwheel_length(w, h);
      default:
        return w * h;
    }
  }
  return w * h;
}

void Segment::set_pixel_color(int n, uint32_t c) const {
  if (!this->is_active() || n < 0)
    return;
  int v_strip = 0;
  const int v_len = static_cast<int>(this->length());
  if (n >= v_len) {
    v_strip = n >> 16;
    n &= 0xFFFF;
    if (n >= v_len)
      return;
  }

  if (this->is_2d()) {
    const int vw = this->width();
    const int vh = this->height();
    switch (this->map1d2d) {
      case M12_P_BAR:
        /* v_strip comes out of the index an effect built with index_to_v_strip(),
         * so a miscounted virtual strip reaches this as a column number that can
         * be past the right hand edge. Use the bounds checked setter. */
        if (v_strip > 0) {
          this->set_pixel_color_xy(v_strip - 1, vh - n - 1, c);
        } else {
          for (int x = 0; x < vw; x++)
            this->set_pixel_color_xy_raw(x, vh - n - 1, c);
        }
        break;
      case M12_P_ARC:
        if (n == 0) {
          this->set_pixel_color_xy_raw(0, 0, c);
        } else {
          const float r = static_cast<float>(n);
          const float step = 1.5707963f / (2.8284f * r + 4);
          for (float rad = 0.0f; rad <= (1.5707963f / 2) + step / 2; rad += step) {
            const int x = static_cast<int>(std::lround(sin_approx(rad) * r));
            const int y = static_cast<int>(std::lround(cos_approx(rad) * r));
            this->set_pixel_color_xy(x, y, c);
            this->set_pixel_color_xy(y, x, c);
          }
        }
        break;
      case M12_P_CORNER:
        for (int x = 0; x <= n; x++)
          this->set_pixel_color_xy(x, n, c);
        for (int y = 0; y < n; y++)
          this->set_pixel_color_xy(n, y, c);
        break;
      case M12_S_PINWHEEL: {
        // Bresenham's algorithm places the coordinates of two rays in arrays, then
        // the block between them is filled.
        if (this->pinwheel_coords_ == nullptr)
          break;
        int start_x, start_y, cos_val[2], sin_val[2];  // fixed point
        this->pinwheel_parameters_(n, vw, vh, start_x, start_y, cos_val, sin_val);

        const unsigned max_line_length = this->pinwheel_max_line_;
        uint16_t *const line_coords[2] = {this->pinwheel_coords_, this->pinwheel_coords_ + 2 * max_line_length};
        int line_length[2] = {0, 0};
        int closest_edge_idx = 0x7FFFFFFF;  // index of the closest edge pixel

        for (int line_nr = 0; line_nr < 2; line_nr++) {
          int x0 = start_x;
          int y0 = start_y;
          const int x1 = (start_x + (cos_val[line_nr] << 9));  // outside the grid
          const int y1 = (start_y + (sin_val[line_nr] << 9));  // outside the grid
          const int dx = x1 > x0 ? x1 - x0 : x0 - x1;
          const int sx = x0 < x1 ? 1 : -1;
          const int dy = -(y1 > y0 ? y1 - y0 : y0 - y1);
          const int sy = y0 < y1 ? 1 : -1;
          uint16_t *coordinates = line_coords[line_nr];
          int *length = &line_length[line_nr];
          x0 /= PINWHEEL_FIXED_SCALE;  // convert to pixel coordinates
          y0 /= PINWHEEL_FIXED_SCALE;

          int idx = 0;
          int err = dx + dy;
          while (true) {
            // Exploits unsigned wraparound to catch negatives in one test.
            if (static_cast<unsigned>(x0) >= static_cast<unsigned>(vw) ||
                static_cast<unsigned>(y0) >= static_cast<unsigned>(vh)) {
              closest_edge_idx = closest_edge_idx < idx - 2 ? closest_edge_idx : idx - 2;
              break;
            }
            if (static_cast<unsigned>(idx) + 2 > 2 * max_line_length)
              break;  // the allocation is sized for the longest possible ray
            coordinates[idx++] = x0;
            coordinates[idx++] = y0;
            (*length)++;
            // The endpoint is off the grid, so there is no endpoint test.
            const int e2 = 2 * err;
            if (e2 >= dy) {
              err += dy;
              x0 += sx;
            }
            if (e2 <= dx) {
              err += dx;
              y0 += sy;
            }
          }
        }

        // Pad the shorter ray with the missing coordinates so the block filling
        // below stays correct and efficient.
        const int diff = line_length[0] - line_length[1];
        const int long_line_idx = (diff > 0) ? 0 : 1;
        const int short_line_idx = long_line_idx ? 0 : 1;
        if (diff != 0 && line_length[short_line_idx] > 0) {
          int idx = (line_length[short_line_idx] - 1) * 2;  // last valid coordinate
          const int last_x = line_coords[short_line_idx][idx++];
          const int last_y = line_coords[short_line_idx][idx++];
          const bool keep_x = last_x == 0 || last_x == vw - 1;
          const int pad = diff > 0 ? diff : -diff;
          for (int d = 0; d < pad; d++) {
            if (static_cast<unsigned>(idx) + 2 > 2 * max_line_length)
              break;
            line_coords[short_line_idx][idx] = keep_x ? last_x : line_coords[long_line_idx][idx];
            idx++;
            line_coords[short_line_idx][idx] = keep_x ? line_coords[long_line_idx][idx] : last_y;
            idx++;
          }
        }

        // Draw and block fill. Block filling is only efficient while the angle
        // between the two rays is small, which it is.
        closest_edge_idx += 2;
        const int max_i = pinwheel_length(vw, vh) - 1;
        // Draw the first ray unless the previous ray was adjacent, wrap included.
        const bool draw_first = !(this->prev_rays_[0] == n - 1 || (n == 0 && this->prev_rays_[0] == max_i));
        const bool draw_last = !(this->prev_rays_[0] == n + 1 || (n == max_i && this->prev_rays_[0] == 0));
        for (int idx = 0; idx < line_length[long_line_idx] * 2;) {
          const int x1 = line_coords[0][idx];
          const int x2 = line_coords[1][idx];
          idx++;
          const int y1 = line_coords[0][idx];
          const int y2 = line_coords[1][idx];
          idx++;
          const int min_x = x1 < x2 ? x1 : x2;
          const int max_x = x1 < x2 ? x2 : x1;
          const int min_y = y1 < y2 ? y1 : y2;
          const int max_y = y1 < y2 ? y2 : y1;

          const bool always_draw = (draw_first && draw_last) ||  // no adjacent rays, draw everything
                                   (idx > closest_edge_idx) ||   // edge pixels on uneven rays always draw
                                   (n == 0 && idx == 2) ||       // centre pixel special case
                                   (n == this->prev_rays_[1]);   // effect drawing twice in one frame
          for (int x = min_x; x <= max_x; x++) {
            for (int y = min_y; y <= max_y; y++) {
              const bool on_line1 = x == x1 && y == y1;
              const bool on_line2 = x == x2 && y == y2;
              if (always_draw || (!on_line1 && (!on_line2 || draw_last)) || (!on_line2 && (!on_line1 || draw_first)))
                this->set_pixel_color_xy(x, y, c);
            }
          }
        }
        this->prev_rays_[1] = this->prev_rays_[0];
        this->prev_rays_[0] = n;
        break;
      }
      case M12_PIXELS:
      default:
        this->set_pixel_color_xy_raw(n % vw, n / vw, c);
        break;
    }
    return;
  }

  this->set_pixel_color_raw(n, c);
}

uint32_t Segment::get_pixel_color(int i) const {
  if (!this->is_active() || i < 0)
    return 0;
  int v_strip = 0;
  const int v_len = static_cast<int>(this->length());
  if (i >= v_len) {
    v_strip = i >> 16;
    i &= 0xFFFF;
    if (i >= v_len)
      return 0;
  }

  if (this->is_2d()) {
    const int vw = this->width();
    const int vh = this->height();
    switch (this->map1d2d) {
      case M12_P_BAR:
        // Bounds checked for the same reason as the setter above.
        return this->get_pixel_color_xy(v_strip > 0 ? v_strip - 1 : 0, vh - i - 1);
      case M12_P_ARC:
        return this->get_pixel_color_xy(i, 0);
      case M12_P_CORNER:
        // Upstream uses the longest dimension.
        return vw > vh ? this->get_pixel_color_xy(i, 0) : this->get_pixel_color_xy(0, i);
      case M12_S_PINWHEEL: {
        // Not exact: returns the pixel at the outer edge of the ray.
        int x, y, cos_val[2], sin_val[2];
        this->pinwheel_parameters_(i, vw, vh, x, y, cos_val, sin_val, true);
        const int max_x = (vw - 1) * PINWHEEL_FIXED_SCALE;
        const int max_y = (vh - 1) * PINWHEEL_FIXED_SCALE;
        // Trace the ray from the centre until it hits an edge. Fixed point keeps
        // the rounding out of the loop condition.
        while ((x < max_x) && (y < max_y) && (x > PINWHEEL_FIXED_SCALE) && (y > PINWHEEL_FIXED_SCALE)) {
          x += cos_val[0];
          y += sin_val[0];
        }
        return this->get_pixel_color_xy(x / PINWHEEL_FIXED_SCALE, y / PINWHEEL_FIXED_SCALE);
      }
      case M12_PIXELS:
      default:
        return this->get_pixel_color_xy_raw(i % vw, i / vw);
    }
  }

  return this->get_pixel_color_raw(i);
}

void Segment::set_pixel_color_xy(int x, int y, uint32_t c) const {
  if (!this->is_active())
    return;
  if (static_cast<unsigned>(x) >= this->width() || static_cast<unsigned>(y) >= this->height())
    return;
  this->set_pixel_color_xy_raw(x, y, c);
}

uint32_t Segment::get_pixel_color_xy(int x, int y) const {
  if (!this->is_active())
    return 0;
  if (static_cast<unsigned>(x) >= this->width() || static_cast<unsigned>(y) >= this->height())
    return 0;
  return this->get_pixel_color_xy_raw(x, y);
}

void Segment::fill(uint32_t c) const {
  if (!this->is_active())
    return;
  const size_t len = this->raw_length();
  for (size_t i = 0; i < len; i++)
    this->set_pixel_color_raw(i, c);
}

void Segment::fade_out(uint8_t rate) const {
  if (!this->is_active())
    return;
  rate = (256 - rate) >> 1;
  const int mapped_rate = 256 / (rate + 1);
  const size_t rlength = this->raw_length();
  for (size_t j = 0; j < rlength; j++) {
    uint32_t color = this->get_pixel_color_raw(j);
    if (color == this->colors[1])
      continue;
    for (int i = 0; i < 32; i += 8) {
      uint8_t c2 = static_cast<uint8_t>(this->colors[1] >> i);
      uint8_t c1 = static_cast<uint8_t>(color >> i);
      int delta = (c2 - c1) * mapped_rate / 256;
      if (delta == 0)
        delta += (c2 == c1) ? 0 : (c2 > c1) ? 1 : -1;
      color &= ~(0xFFu << i);
      color |= static_cast<uint32_t>((c1 + delta) & 0xFF) << i;
    }
    this->set_pixel_color_raw(j, color);
  }
}

void Segment::fade_to_secondary_by(uint8_t fade_by) const {
  if (!this->is_active() || fade_by == 0)
    return;
  const size_t rlength = this->raw_length();
  for (size_t i = 0; i < rlength; i++)
    this->set_pixel_color_raw(i, color_blend(this->get_pixel_color_raw(i), this->colors[1], fade_by));
}

void Segment::fade_to_black_by(uint8_t fade_by) const {
  if (!this->is_active() || fade_by == 0)
    return;
  const size_t rlength = this->raw_length();
  for (size_t i = 0; i < rlength; i++)
    this->set_pixel_color_raw(i, fast_color_scale(this->get_pixel_color_raw(i), 255 - fade_by));
}

void Segment::blur(uint8_t blur_amount, bool smear) const {
  if (!this->is_active() || blur_amount == 0)
    return;
  if (this->is_2d()) {
    this->blur2d(blur_amount, blur_amount, smear);
    return;
  }
  const uint8_t keep = smear ? 255 : 255 - blur_amount;
  const uint8_t seep = blur_amount >> 1;
  const size_t vlength = this->raw_length();
  uint32_t cur = this->get_pixel_color_raw(0);
  uint32_t carryover = fast_color_scale(cur, seep);
  this->set_pixel_color_raw(0, fast_color_scale(cur, keep));
  for (size_t i = 1; i < vlength; i++) {
    cur = this->get_pixel_color_raw(i);
    uint32_t part = fast_color_scale(cur, seep);
    cur = fast_color_scale(cur, keep);
    cur = color_add(cur, carryover);
    this->set_pixel_color_raw(i - 1, color_add(this->get_pixel_color_raw(i - 1), part));
    this->set_pixel_color_raw(i, cur);
    carryover = part;
  }
}

void Segment::blur2d(uint8_t blur_x, uint8_t blur_y, bool smear) const {
  if (!this->is_active())
    return;
  const unsigned cols = this->width();
  const unsigned rows = this->height();
  const auto xy = [&](unsigned x, unsigned y) { return x + y * cols; };
  if (blur_x) {
    const uint8_t keepx = smear ? 255 : 255 - blur_x;
    const uint8_t seepx = blur_x >> 1;
    for (unsigned row = 0; row < rows; row++) {
      uint32_t cur = this->get_pixel_color_raw(xy(0, row));
      uint32_t carryover = fast_color_scale(cur, seepx);
      this->set_pixel_color_raw(xy(0, row), fast_color_scale(cur, keepx));
      for (unsigned x = 1; x < cols; x++) {
        cur = this->get_pixel_color_raw(xy(x, row));
        uint32_t part = fast_color_scale(cur, seepx);
        cur = fast_color_scale(cur, keepx);
        cur = color_add(cur, carryover);
        this->set_pixel_color_raw(xy(x - 1, row), color_add(this->get_pixel_color_raw(xy(x - 1, row)), part));
        this->set_pixel_color_raw(xy(x, row), cur);
        carryover = part;
      }
    }
  }
  if (blur_y) {
    const uint8_t keepy = smear ? 255 : 255 - blur_y;
    const uint8_t seepy = blur_y >> 1;
    for (unsigned col = 0; col < cols; col++) {
      uint32_t cur = this->get_pixel_color_raw(xy(col, 0));
      uint32_t carryover = fast_color_scale(cur, seepy);
      this->set_pixel_color_raw(xy(col, 0), fast_color_scale(cur, keepy));
      for (unsigned y = 1; y < rows; y++) {
        cur = this->get_pixel_color_raw(xy(col, y));
        uint32_t part = fast_color_scale(cur, seepy);
        cur = fast_color_scale(cur, keepy);
        cur = color_add(cur, carryover);
        this->set_pixel_color_raw(xy(col, y - 1), color_add(this->get_pixel_color_raw(xy(col, y - 1)), part));
        this->set_pixel_color_raw(xy(col, y), cur);
        carryover = part;
      }
    }
  }
}

void Segment::move_x(int delta, bool wrap) const {
  if (!this->is_active() || !delta)
    return;
  const int vw = this->width();
  const int vh = this->height();
  const auto xy = [&](int x, int y) { return x + y * vw; };
  const int abs_delta = delta < 0 ? -delta : delta;
  if (abs_delta >= vw)
    return;
  int new_delta;
  int stop = vw;
  int start = 0;
  if (wrap) {
    new_delta = (delta + vw) % vw;
  } else {
    if (delta < 0)
      start = abs_delta;
    stop = vw - abs_delta;
    new_delta = delta > 0 ? delta : 0;
  }
  // WLED uses a stack VLA here. ESPHome forbids those, so the row is copied
  // through the scratch buffer allocated alongside the canvas.
  if (this->scratch_ == nullptr)
    return;
  for (int y = 0; y < vh; y++) {
    for (int x = 0; x < stop; x++) {
      int src_x = x + new_delta;
      if (wrap)
        src_x %= vw;
      this->scratch_[x] = this->get_pixel_color_raw(xy(src_x, y));
    }
    for (int x = 0; x < stop; x++)
      this->set_pixel_color_raw(xy(x + start, y), this->scratch_[x]);
  }
}

void Segment::move_y(int delta, bool wrap) const {
  if (!this->is_active() || !delta)
    return;
  const int vw = this->width();
  const int vh = this->height();
  const auto xy = [&](int x, int y) { return x + y * vw; };
  const int abs_delta = delta < 0 ? -delta : delta;
  if (abs_delta >= vh)
    return;
  int new_delta;
  int stop = vh;
  int start = 0;
  if (wrap) {
    new_delta = (delta + vh) % vh;
  } else {
    if (delta < 0)
      start = abs_delta;
    stop = vh - abs_delta;
    new_delta = delta > 0 ? delta : 0;
  }
  if (this->scratch_ == nullptr)
    return;
  for (int x = 0; x < vw; x++) {
    for (int y = 0; y < stop; y++) {
      int src_y = y + new_delta;
      if (wrap)
        src_y %= vh;
      this->scratch_[y] = this->get_pixel_color_raw(xy(x, src_y));
    }
    for (int y = 0; y < stop; y++)
      this->set_pixel_color_raw(xy(x, y + start), this->scratch_[y]);
  }
}

void Segment::move(unsigned dir, unsigned delta, bool wrap) const {
  if (delta == 0)
    return;
  const int d = static_cast<int>(delta);
  switch (dir) {
    case 0:
      this->move_x(d, wrap);
      break;
    case 1:
      this->move_x(d, wrap);
      this->move_y(d, wrap);
      break;
    case 2:
      this->move_y(d, wrap);
      break;
    case 3:
      this->move_x(-d, wrap);
      this->move_y(d, wrap);
      break;
    case 4:
      this->move_x(-d, wrap);
      break;
    case 5:
      this->move_x(-d, wrap);
      this->move_y(-d, wrap);
      break;
    case 6:
      this->move_y(-d, wrap);
      break;
    case 7:
      this->move_x(d, wrap);
      this->move_y(-d, wrap);
      break;
    default:
      break;
  }
}

void Segment::draw_circle(int cx, int cy, uint8_t radius, uint32_t col, bool soft) const {
  if (!this->is_active() || radius == 0)
    return;
  if (soft) {
    const int rsq = radius * radius;
    int x = 0;
    int y = radius;
    unsigned old_fade = 0;
    while (x < y) {
      float yf = std::sqrt(static_cast<float>(rsq - x * x));
      uint8_t fade = static_cast<uint8_t>(255.0f * (std::ceil(yf) - yf));
      if (old_fade > fade)
        y--;
      old_fade = fade;
      int px, py;
      for (uint8_t i = 0; i < 16; i++) {
        int swaps = (i & 0x4) ? 1 : 0;
        int adj = (i < 8) ? 0 : 1;
        int dx = (i & 1) ? -1 : 1;
        int dy = (i & 2) ? -1 : 1;
        if (swaps) {
          px = cx + (y - adj) * dx;
          py = cy + x * dy;
        } else {
          px = cx + x * dx;
          py = cy + (y - adj) * dy;
        }
        uint32_t pix_col = this->get_pixel_color_xy(px, py);
        this->set_pixel_color_xy(px, py, adj ? color_blend(pix_col, col, fade) : color_blend(col, pix_col, fade));
      }
      x++;
    }
  } else {
    int d = 3 - (2 * radius);
    int y = radius, x = 0;
    while (y >= x) {
      for (int i = 0; i < 4; i++) {
        int dx = (i & 1) ? -x : x;
        int dy = (i & 2) ? -y : y;
        this->set_pixel_color_xy(cx + dx, cy + dy, col);
        this->set_pixel_color_xy(cx + dy, cy + dx, col);
      }
      x++;
      if (d > 0) {
        y--;
        d += 4 * (x - y) + 10;
      } else {
        d += 4 * x + 6;
      }
    }
  }
}

void Segment::fill_circle(int cx, int cy, uint8_t radius, uint32_t col, bool soft) const {
  if (!this->is_active() || radius == 0)
    return;
  const int vw = this->width();
  const int vh = this->height();
  if (soft)
    this->draw_circle(cx, cy, radius, col, soft);
  for (int y = -radius; y <= radius; y++) {
    for (int x = -radius; x <= radius; x++) {
      if (x * x + y * y <= radius * radius && cx + x >= 0 && cy + y >= 0 && cx + x < vw && cy + y < vh)
        this->set_pixel_color_xy(cx + x, cy + y, col);
    }
  }
}

void Segment::draw_line(int x0, int y0, int x1, int y1, uint32_t c, bool soft) const {
  if (!this->is_active())
    return;
  const int vw = this->width();
  const int vh = this->height();
  if (static_cast<unsigned>(x0) >= static_cast<unsigned>(vw) ||
      static_cast<unsigned>(x1) >= static_cast<unsigned>(vw) ||
      static_cast<unsigned>(y0) >= static_cast<unsigned>(vh) || static_cast<unsigned>(y1) >= static_cast<unsigned>(vh))
    return;

  const int dx = x1 > x0 ? x1 - x0 : x0 - x1, sx = x0 < x1 ? 1 : -1;
  const int dy = y1 > y0 ? y1 - y0 : y0 - y1, sy = y0 < y1 ? 1 : -1;

  if (dx + dy == 0) {
    this->set_pixel_color_xy(x0, y0, c);
    return;
  }

  if (soft) {
    const bool steep = dy > dx;
    if (steep) {
      std::swap(x0, y0);
      std::swap(x1, y1);
    }
    if (x0 > x1) {
      std::swap(x0, x1);
      std::swap(y0, y1);
    }
    float gradient = x1 - x0 == 0 ? 1.0f : static_cast<float>(y1 - y0) / static_cast<float>(x1 - x0);
    float intersect_y = y0;
    for (int x = x0; x <= x1; x++) {
      uint8_t keep = static_cast<uint8_t>(255.0f * (intersect_y - static_cast<int>(intersect_y)));
      uint8_t seep = 0xFF - keep;
      int y = static_cast<int>(intersect_y);
      int px = x, py = y;
      if (steep)
        std::swap(px, py);
      this->blend_pixel_color_xy(px, py, c, seep);
      this->blend_pixel_color_xy(px + static_cast<int>(steep), py + static_cast<int>(!steep), c, keep);
      intersect_y += gradient;
    }
  } else {
    int err = (dx > dy ? dx : -dy) / 2;
    for (;;) {
      this->set_pixel_color_xy(x0, y0, c);
      if (x0 == x1 && y0 == y1)
        break;
      int e2 = err;
      if (e2 > -dx) {
        err -= dy;
        x0 += sx;
      }
      if (e2 < dy) {
        err += dx;
        y0 += sy;
      }
    }
  }
}

void Segment::wu_pixel(uint32_t x, uint32_t y, CRGB c) const {
  if (!this->is_active())
    return;
  const auto weight = [](unsigned a, unsigned b) { return static_cast<uint8_t>((a * b + a + b) >> 8); };
  unsigned xx = x & 0xff, yy = y & 0xff, ix = 255 - xx, iy = 255 - yy;
  uint8_t wu[4] = {weight(ix, iy), weight(xx, iy), weight(ix, yy), weight(xx, yy)};
  for (int i = 0; i < 4; i++) {
    int wu_x = (x >> 8) + (i & 1);
    int wu_y = (y >> 8) + ((i >> 1) & 1);
    CRGB led = this->get_pixel_color_xy(wu_x, wu_y);
    CRGB old_led = led;
    led.r = qadd8(led.r, c.r * wu[i] >> 8);
    led.g = qadd8(led.g, c.g * wu[i] >> 8);
    led.b = qadd8(led.b, c.b * wu[i] >> 8);
    if (led != old_led)
      this->set_pixel_color_xy(wu_x, wu_y, led);
  }
}

uint32_t Segment::color_wheel(uint8_t pos) const {
  if (this->palette)
    return this->color_from_palette(pos, false, true, 0);
  CRGBW rgb;
  rgb = CHSV32(static_cast<uint16_t>(pos << 8), 255, 255);
  rgb.w = W(this->color(0));
  return rgb.color32;
}

uint32_t Segment::color_from_palette(uint16_t i, bool mapping, bool moving, uint8_t mcol, uint8_t pbri) const {
  uint32_t color = this->color(mcol);
  if (this->palette == 0 && mcol < 3)
    return color_fade(color, pbri, true);

  unsigned palette_index = i;
  if (mapping) {
    const unsigned len = this->length();
    palette_index = len > 0 ? std::min<unsigned>((i * 255) / len, 255) : 0;
  }
  TBlendType blend = NOBLEND;
  switch (this->palette_blend) {
    case 0:
      blend = moving ? LINEARBLEND : LINEARBLEND_NOWRAP;
      break;
    case 1:
      blend = LINEARBLEND;
      break;
    case 2:
      blend = LINEARBLEND_NOWRAP;
      break;
    default:
      break;
  }
  CRGBW palcol = ColorFromPalette(this->current_palette_, palette_index, pbri, blend);
  palcol.w = W(color);
  return palcol.color32;
}

}  // namespace wled_fx
}  // namespace esphome
