"""Tests for the wled_fx compile-time effect allow-list."""

import pytest
from voluptuous import Invalid

from esphome.components.wled_fx import (
    CONF_EFFECT,
    CONF_EFFECTS,
    _discover_effect_groups,
    _validate_chosen_effect,
    _validate_effect_names,
    effect_macro,
)


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


def test_allow_list_rejects_a_name_no_effect_answers_to() -> None:
    """A typo must fail validation, not silently drop the effect from the build."""
    with pytest.raises(Invalid, match="No effect is called 'Fire 2013'"):
        _validate_effect_names({CONF_EFFECTS: ["Fire 2012", "Fire 2013"]})


def test_allow_list_accepts_shipped_names_case_insensitively() -> None:
    config = {CONF_EFFECTS: ["fire 2012", "PLASMA"]}
    assert _validate_effect_names(config) is config


def test_no_allow_list_is_valid() -> None:
    """An omitted list means every effect, which is the default."""
    config: dict[str, list[str]] = {}
    assert _validate_effect_names(config) is config


def test_chosen_effect_must_exist() -> None:
    """A light effect naming nothing would silently run whatever is registered first."""
    with pytest.raises(Invalid, match="No effect is called 'Firenoise'"):
        _validate_chosen_effect({CONF_EFFECT: "Firenoise"})


def test_chosen_effect_accepts_a_shipped_name() -> None:
    config = {CONF_EFFECT: "Pride 2015"}
    assert _validate_chosen_effect(config) is config


def test_no_chosen_effect_is_valid() -> None:
    """Omitting 'effect:' leaves the engine on the first registered effect."""
    config: dict[str, str] = {}
    assert _validate_chosen_effect(config) is config
