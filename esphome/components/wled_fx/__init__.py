"""WLED effect engine for ESPHome.

The engine itself is an external package. This file is the ESPHome side of it:
it pulls the package in, turns the YAML into the engine's compile-time effect
selection, emits the table of effect groups the linker has to keep, and
registers the addressable light effect.

Nothing here carries an effect name, palette name, parameter table or default
taken from WLED. Effect names arrive from the user's YAML and are turned into
macros by a pure transform, so this file stays MIT like the rest of the ESPHome
Python tree while the engine stays GPL-3.0-or-later.
"""

from pathlib import Path
import re

import esphome.codegen as cg
from esphome.components.light.effects import register_addressable_effect
from esphome.components.light.types import AddressableLightEffect
import esphome.config_validation as cv
from esphome.const import (
    CONF_EFFECT,
    CONF_EFFECTS,
    CONF_HEIGHT,
    CONF_ID,
    CONF_INTENSITY,
    CONF_NAME,
    CONF_PATH,
    CONF_REF,
    CONF_SPEED,
    CONF_UPDATE_INTERVAL,
    CONF_WIDTH,
)
from esphome.core import CORE
from esphome.types import ConfigType

CODEOWNERS = ["@bharvey88"]
DEPENDENCIES = ["light"]
DOMAIN = "wled_fx"

CONF_ENGINE = "engine"
CONF_REPOSITORY = "repository"
CONF_PALETTE = "palette"
CONF_CUSTOM1 = "custom1"
CONF_CUSTOM2 = "custom2"
CONF_CUSTOM3 = "custom3"
CONF_CHECK1 = "check1"
CONF_CHECK2 = "check2"
CONF_CHECK3 = "check3"
CONF_SERPENTINE = "serpentine"
CONF_USE_LIGHT_COLOR = "use_light_color"

# The engine package. Pinned by tag; `engine:` in the YAML overrides it, which is
# how a local checkout is tested.
ENGINE_NAME = "wled-fx-engine"
ENGINE_REPOSITORY = "https://github.com/esphome/wled-fx-engine"
ENGINE_REF = "0.1.0"
ENGINE_IDF_NAME = "esphome/wled-fx-engine"

# The engine's own partitioning of its effect translation units. Every group
# symbol is defined whether or not anything in it was selected, so this table is
# structural and needs no knowledge of which effect lives where.
ENGINE_GROUPS = (
    "1D2D",
    "1D_A",
    "1D_B",
    "1D_C",
    "1D_D",
    "1D_E",
    "2D_A",
    "2D_B",
    "AUDIO_FFT",
    "AUDIO_PARTICLE",
    "AUDIO_VOL",
    "MM",
    "PARTICLE_1D",
    "PARTICLE_2D",
)

# WLED's FRAMETIME at its default WLED_FPS of 42, which is the frame period the
# effect bodies were written against.
FRAMETIME = "23ms"

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
    """Turns an effect name from YAML into the macro the engine's guard tests."""
    expanded = "".join(_NAME_TOKENS.get(c, c) for c in name.upper())
    return "WLED_FX_FX_" + re.sub(r"[^A-Z0-9]+", "_", expanded).strip("_")


ENGINE_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_REPOSITORY, default=ENGINE_REPOSITORY): cv.string,
        cv.Optional(CONF_REF, default=ENGINE_REF): cv.string,
        cv.Optional(CONF_PATH): cv.directory,
    }
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_EFFECTS): cv.ensure_list(cv.string),
        cv.Optional(CONF_ENGINE, default={}): ENGINE_SCHEMA,
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
        CONF_UPDATE_INTERVAL, default=FRAMETIME
    ): cv.positive_time_period_milliseconds,
    **CONTROL_SCHEMA,
}


def _add_engine(engine: ConfigType) -> None:
    """Pulls the engine package in by whichever route the target platform takes."""
    if (path := engine.get(CONF_PATH)) is not None:
        if CORE.using_toolchain_esp_idf:
            from esphome.components.esp32 import add_idf_component

            # ESP-IDF names a local component after its directory, and the
            # build's REQUIRES has to match that name, so the dependency key
            # must be the directory's own name rather than the registry one.
            add_idf_component(name=Path(path).name, path=path)
        else:
            cg.add_library(ENGINE_NAME, None, f"symlink://{path}")
        return

    if CORE.using_toolchain_esp_idf:
        from esphome.components.esp32 import add_idf_component

        add_idf_component(
            name=ENGINE_IDF_NAME,
            repo=engine[CONF_REPOSITORY],
            ref=engine[CONF_REF],
        )
        return

    cg.add_library(
        ENGINE_NAME, None, f"{engine[CONF_REPOSITORY]}.git#{engine[CONF_REF]}"
    )


async def to_code(config: ConfigType) -> None:
    """Emits the compile-time effect selection and the table of linked groups.

    The engine compiles into a static library, so a translation unit nothing
    refers to is never pulled out of the archive. Naming the group symbols from
    the generated main.cpp is what keeps them in.
    """
    _add_engine(config[CONF_ENGINE])

    if macros := {effect_macro(name) for name in config.get(CONF_EFFECTS, [])}:
        for macro in sorted(macros):
            cg.add_build_flag(f"-D{macro}=1")
    else:
        cg.add_build_flag("-DWLED_FX_ALL_EFFECTS")

    declarations = "".join(
        f"extern const EffectGroup EFFECT_GROUP_{g};" for g in ENGINE_GROUPS
    )
    references = ", ".join(f"&EFFECT_GROUP_{g}" for g in ENGINE_GROUPS)
    cg.add_global(
        cg.RawStatement(
            "namespace esphome { namespace wled_fx { struct EffectGroup; "
            f"{declarations} "
            f"const EffectGroup *const LINKED_EFFECT_GROUPS[] = {{{references}}}; "
            f"const unsigned LINKED_EFFECT_GROUP_COUNT = {len(ENGINE_GROUPS)};"
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

    engine = var.engine()
    if (effect := config.get(CONF_EFFECT)) is not None:
        cg.add(engine.set_effect(effect))
    if (palette := config.get(CONF_PALETTE)) is not None:
        cg.add(engine.set_palette_by_name(palette))
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
