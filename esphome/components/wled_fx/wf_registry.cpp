/* See wf_registry.h for the WLED attribution and licence notice. */

#include "wf_registry.h"

#include <cstdlib>
#include <cstring>

namespace esphome {
namespace wled_fx {

namespace {

char lower(char c) { return (c >= 'A' && c <= 'Z') ? static_cast<char>(c + 32) : c; }

// Returns a pointer to the start of metadata group `index` (0 based, groups are
// separated by ';'), or nullptr when there are fewer groups than that.
const char *group_at(const char *metadata, int index) {
  const char *p = metadata;
  int current = 0;
  while (current < index) {
    p = strchr(p, ';');
    if (p == nullptr)
      return nullptr;
    p++;
    current++;
  }
  return p;
}

}  // namespace

size_t effect_name(const EffectInfo &info, char *dest, size_t dest_size) {
  if (dest_size == 0)
    return 0;
  size_t j = 0;
  const char *src = info.metadata;
  while (j + 1 < dest_size && src[j] != '\0' && src[j] != '@') {
    dest[j] = src[j];
    j++;
  }
  dest[j] = '\0';
  return j;
}

bool effect_name_equals(const EffectInfo &info, const char *name) {
  if (name == nullptr)
    return false;
  const char *a = info.metadata;
  const char *b = name;
  while (*a != '\0' && *a != '@' && *b != '\0') {
    if (lower(*a) != lower(*b))
      return false;
    a++;
    b++;
  }
  return (*a == '\0' || *a == '@') && *b == '\0';
}

EffectDefaults effect_defaults(const EffectInfo &info) {
  EffectDefaults out;

  // Group 3 is the palette section: a leading digit is the default palette ID.
  const char *palette_group = group_at(info.metadata, 2);
  if (palette_group != nullptr && *palette_group >= '0' && *palette_group <= '9')
    out.palette = static_cast<uint8_t>(atoi(palette_group));

  // Group 4 is the dimensionality and audio flag set.
  const char *flag_group = group_at(info.metadata, 3);
  uint8_t flags = 0;
  if (flag_group != nullptr) {
    for (const char *p = flag_group; *p != '\0' && *p != ';'; p++) {
      switch (*p) {
        case '0':
          flags |= EFFECT_FLAG_0D;
          break;
        case '1':
          flags |= EFFECT_FLAG_1D;
          break;
        case '2':
          flags |= EFFECT_FLAG_2D;
          break;
        case 'v':
          flags |= EFFECT_FLAG_VOLUME;
          break;
        case 'f':
          flags |= EFFECT_FLAG_FFT;
          break;
        default:
          break;
      }
    }
  }
  out.flags = flags == 0 ? static_cast<uint8_t>(EFFECT_FLAG_1D) : flags;

  // Defaults live in the LAST ';' group, which is how WLED reads them too. Keys
  // are sx, ix, c1, c2, c3, o1, o2, o3, pal, m12 and si.
  const char *last = strrchr(info.metadata, ';');
  if (last == nullptr)
    return out;
  last++;

  struct KeyTarget {
    const char *key;
    uint8_t *target;
  };
  const KeyTarget numeric[] = {
      {"sx", &out.speed},   {"ix", &out.intensity}, {"c1", &out.custom1},  {"c2", &out.custom2},
      {"c3", &out.custom3}, {"pal", &out.palette},  {"m12", &out.map1d2d},
  };
  bool *const checks[] = {&out.check1, &out.check2, &out.check3};
  const char *const check_keys[] = {"o1", "o2", "o3"};

  const char *p = last;
  while (*p != '\0') {
    // Split on commas. Each item is key=value.
    const char *eq = strchr(p, '=');
    const char *comma = strchr(p, ',');
    if (eq == nullptr || (comma != nullptr && eq > comma)) {
      if (comma == nullptr)
        break;
      p = comma + 1;
      continue;
    }
    const size_t key_len = static_cast<size_t>(eq - p);
    const int value = atoi(eq + 1);
    for (const auto &kt : numeric) {
      if (strlen(kt.key) == key_len && strncmp(p, kt.key, key_len) == 0) {
        *kt.target = static_cast<uint8_t>(value);
        break;
      }
    }
    for (size_t i = 0; i < 3; i++) {
      if (strlen(check_keys[i]) == key_len && strncmp(p, check_keys[i], key_len) == 0)
        *checks[i] = value != 0;
    }
    if (comma == nullptr)
      break;
    p = comma + 1;
  }

  return out;
}

size_t EffectRegistry::count() {
  size_t total = 0;
  for (unsigned g = 0; g < LINKED_EFFECT_GROUP_COUNT; g++)
    total += LINKED_EFFECT_GROUPS[g]->count;
  return total;
}

const EffectInfo *EffectRegistry::at(size_t index) {
  for (unsigned g = 0; g < LINKED_EFFECT_GROUP_COUNT; g++) {
    const EffectGroup &group = *LINKED_EFFECT_GROUPS[g];
    if (index < group.count)
      return &group.entries[index];
    index -= group.count;
  }
  return nullptr;
}

const EffectInfo *EffectRegistry::find(const char *name) {
  const int index = EffectRegistry::index_of(name);
  return index < 0 ? nullptr : EffectRegistry::at(static_cast<size_t>(index));
}

int EffectRegistry::index_of(const char *name) {
  int index = 0;
  for (unsigned g = 0; g < LINKED_EFFECT_GROUP_COUNT; g++) {
    const EffectGroup &group = *LINKED_EFFECT_GROUPS[g];
    for (size_t i = 0; i < group.count; i++, index++) {
      if (effect_name_equals(group.entries[i], name))
        return index;
    }
  }
  return -1;
}

}  // namespace wled_fx
}  // namespace esphome
