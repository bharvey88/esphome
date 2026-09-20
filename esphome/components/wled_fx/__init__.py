"""WLED effect engine for ESPHome.

This file is independent work. It carries no effect names, palette data,
parameter tables or other material taken from WLED: everything derived from WLED
is in the C++ sources, which are GPLv3, while this file is MIT like the rest of
the ESPHome Python tree.
"""

from pathlib import Path
import re

import esphome.codegen as cg
from esphome.components.light.effects import register_addressable_effect
from esphome.components.light.types import AddressableLightEffect
import esphome.config_validation as cv
from esphome.const import (
    CONF_HEIGHT,
    CONF_ID,
    CONF_NAME,
    CONF_UPDATE_INTERVAL,
    CONF_WIDTH,
)
from esphome.types import ConfigType

CODEOWNERS = ["@bharvey88"]
DOMAIN = "wled_fx"

CONF_EFFECTS = "effects"
CONF_EFFECT = "effect"
CONF_PALETTE = "palette"
CONF_SPEED = "speed"
CONF_INTENSITY = "intensity"
CONF_CUSTOM1 = "custom1"
CONF_CUSTOM2 = "custom2"
CONF_CUSTOM3 = "custom3"
CONF_CHECK1 = "check1"
CONF_CHECK2 = "check2"
CONF_CHECK3 = "check3"
CONF_SERPENTINE = "serpentine"
CONF_USE_LIGHT_COLOR = "use_light_color"

wled_fx_ns = cg.esphome_ns.namespace("wled_fx")
WledFxLightEffect = wled_fx_ns.class_("WledFxLightEffect", AddressableLightEffect)

# Punctuation that carries meaning in an effect name and so has to survive into
# the derived identifier. Collapsing all of it to "_" made "Sparkle" and
# "Sparkle+" the same macro, so naming one in YAML pulled in both.
_NAME_TOKENS = {
    "+": "_PLUS",
    "&": "_AND",
    "/": "_SLASH",
    "#": "_HASH",
    "%": "_PCT",
    "*": "_STAR",
}


def effect_macro(name: str) -> str:
    """Turns an effect name from YAML into the macro the C++ guard tests."""
    expanded = "".join(_NAME_TOKENS.get(c, c) for c in name.upper())
    return "WLED_FX_FX_" + re.sub(r"[^A-Z0-9]+", "_", expanded).strip("_")


# Each effect translation unit declares the group it belongs to with two lines,
# which is all this scan reads:
#   #define WLED_FX_GROUP_<ID> (WLED_FX_DEFAULT_ENABLE || WLED_FX_FX_A || ...)
#   const EffectGroup EFFECT_GROUP_<ID>{"<id>", ENTRIES, ...};
_GROUP_GUARD_RE = re.compile(
    r"#define\s+WLED_FX_GROUP_(\w+)\s*((?:[^\n\\]*\\\s*\n)*[^\n]*)"
)
_FX_MACRO_RE = re.compile(r"WLED_FX_FX_\w+")


def _discover_effect_groups() -> dict[str, set[str]]:
    """Maps each EFFECT_GROUP_* symbol to the effect macros it can provide."""
    groups: dict[str, set[str]] = {}
    for path in sorted(Path(__file__).parent.glob("wf_effects_*.cpp")):
        text = path.read_text(encoding="utf-8")
        for name, guard in _GROUP_GUARD_RE.findall(text):
            if f"EFFECT_GROUP_{name}" not in text:
                continue
            groups[f"EFFECT_GROUP_{name}"] = set(_FX_MACRO_RE.findall(guard))
    return groups


CONFIG_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_EFFECTS): cv.ensure_list(cv.string),
    }
)

CONTROL_SCHEMA = {
    cv.Optional(CONF_EFFECT): cv.string,
    cv.Optional(CONF_PALETTE): cv.string,
    cv.Optional(CONF_SPEED): cv.int_range(min=0, max=255),
    cv.Optional(CONF_INTENSITY): cv.int_range(min=0, max=255),
    cv.Optional(CONF_CUSTOM1): cv.int_range(min=0, max=255),
    cv.Optional(CONF_CUSTOM2): cv.int_range(min=0, max=255),
    cv.Optional(CONF_CUSTOM3): cv.int_range(min=0, max=31),
    cv.Optional(CONF_CHECK1): cv.boolean,
    cv.Optional(CONF_CHECK2): cv.boolean,
    cv.Optional(CONF_CHECK3): cv.boolean,
}

LIGHT_EFFECT_SCHEMA = {
    cv.Optional(CONF_ID): cv.declare_id(WledFxLightEffect),
    cv.Optional(CONF_WIDTH): cv.positive_not_null_int,
    cv.Optional(CONF_HEIGHT): cv.positive_not_null_int,
    cv.Optional(CONF_SERPENTINE, default=False): cv.boolean,
    cv.Optional(CONF_USE_LIGHT_COLOR, default=True): cv.boolean,
    cv.Optional(
        CONF_UPDATE_INTERVAL, default="33ms"
    ): cv.positive_time_period_milliseconds,
    **CONTROL_SCHEMA,
}


async def to_code(config: ConfigType) -> None:
    """Emits the compile-time allow-list and the table of linked effect groups.

    ESPHome builds the generated sources into a static library, so a translation
    unit nothing refers to is never pulled out of the archive. Naming the group
    symbols from the generated main.cpp is what keeps them in.
    """
    macros = {effect_macro(name) for name in config.get(CONF_EFFECTS, [])}
    all_effects = not macros

    cg.add_build_flag(f"-DWLED_FX_DEFAULT_ENABLE={1 if all_effects else 0}")
    for macro in sorted(macros):
        cg.add_build_flag(f"-D{macro}=1")

    linked = [
        symbol
        for symbol, provides in sorted(_discover_effect_groups().items())
        if all_effects or (provides & macros)
    ]
    if not linked:
        raise cv.Invalid(
            "The 'effects' list selected no effect that exists. Effect names are "
            "the WLED display names, for example 'Fire 2012'.",
            path=[CONF_EFFECTS],
        )
    declarations = "".join(f"extern const EffectGroup {s};" for s in linked)
    references = ", ".join(f"&{s}" for s in linked)
    cg.add_global(
        cg.RawStatement(
            "namespace esphome { namespace wled_fx { struct EffectGroup; "
            f"{declarations} "
            f"const EffectGroup *const LINKED_EFFECT_GROUPS[] = {{{references}}}; "
            f"const unsigned LINKED_EFFECT_GROUP_COUNT = {len(linked)};"
            " } }"
        )
    )


@register_addressable_effect(
    "wled_fx",
    WledFxLightEffect,
    "WLED FX",
    LIGHT_EFFECT_SCHEMA,
    cv.requires_component(DOMAIN),
)
async def wled_fx_light_effect_to_code(config: ConfigType, effect_id):
    var = cg.new_Pvariable(config.get(CONF_ID, effect_id), config[CONF_NAME])
    cg.add(var.set_dimensions(config.get(CONF_WIDTH, 0), config.get(CONF_HEIGHT, 0)))
    cg.add(var.set_serpentine(config[CONF_SERPENTINE]))
    cg.add(var.set_use_light_color(config[CONF_USE_LIGHT_COLOR]))
    cg.add(var.set_frame_interval(config[CONF_UPDATE_INTERVAL]))

    if (effect := config.get(CONF_EFFECT)) is not None:
        cg.add(var.engine().set_effect(effect))
    if (palette := config.get(CONF_PALETTE)) is not None:
        cg.add(var.engine().set_palette_by_name(palette))
    engine = var.engine()
    if (speed := config.get(CONF_SPEED)) is not None:
        cg.add(engine.set_speed(speed))
    if (intensity := config.get(CONF_INTENSITY)) is not None:
        cg.add(engine.set_intensity(intensity))
    if (custom1 := config.get(CONF_CUSTOM1)) is not None:
        cg.add(engine.set_custom1(custom1))
    if (custom2 := config.get(CONF_CUSTOM2)) is not None:
        cg.add(engine.set_custom2(custom2))
    if (custom3 := config.get(CONF_CUSTOM3)) is not None:
        cg.add(engine.set_custom3(custom3))
    if (check1 := config.get(CONF_CHECK1)) is not None:
        cg.add(engine.set_check1(check1))
    if (check2 := config.get(CONF_CHECK2)) is not None:
        cg.add(engine.set_check2(check2))
    if (check3 := config.get(CONF_CHECK3)) is not None:
        cg.add(engine.set_check3(check3))
    return var
