"""Tests for the wled_fx compile-time effect allow-list."""

import pytest

from esphome.components.wled_fx import _discover_effect_groups, effect_macro


@pytest.mark.parametrize(
    ("name", "expected"),
    [
        ("Solid", "WLED_FX_FX_SOLID"),
        ("Fire 2012", "WLED_FX_FX_FIRE_2012"),
        ("fire 2012", "WLED_FX_FX_FIRE_2012"),
        ("Pride 2015", "WLED_FX_FX_PRIDE_2015"),
        ("Noise 1", "WLED_FX_FX_NOISE_1"),
        ("Scan Dual", "WLED_FX_FX_SCAN_DUAL"),
        ("Dissolve Rnd", "WLED_FX_FX_DISSOLVE_RND"),
    ],
)
def test_effect_macro_matches_the_cpp_guard(name: str, expected: str) -> None:
    """The macro a YAML effect name derives must be the one the C++ guard tests."""
    assert effect_macro(name) == expected


def test_punctuation_keeps_similar_names_apart() -> None:
    """Sparkle and Sparkle+ must not collapse onto one macro."""
    assert effect_macro("Sparkle") != effect_macro("Sparkle+")
    assert effect_macro("Sparkle+") == "WLED_FX_FX_SPARKLE_PLUS"


def test_leading_and_trailing_punctuation_is_stripped() -> None:
    """A derived macro is always a valid identifier."""
    assert effect_macro(" Two  Dots ") == "WLED_FX_FX_TWO_DOTS"


def test_discovered_groups_match_the_shipped_effect_files() -> None:
    """Every effect the guard names must be derivable from a display name."""
    groups = _discover_effect_groups()
    assert "EFFECT_GROUP_1D_A" in groups
    provided = groups["EFFECT_GROUP_1D_A"]
    expected = {
        effect_macro(name)
        for name in (
            "Solid",
            "Blink",
            "Rainbow",
            "Pride 2015",
            "Fire 2012",
            "Noise 1",
            "Plasma",
        )
    }
    assert provided == expected
