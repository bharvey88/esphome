import esphome.codegen as cg
from esphome.components import number
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    DEVICE_CLASS_DISTANCE,
    ENTITY_CATEGORY_CONFIG,
    ICON_TIMELAPSE,
    UNIT_DEGREES,
    UNIT_MILLIMETER,
    UNIT_SECOND,
)

from .. import CONF_LD2450_ID, LD2450Component, ld2450_ns

CONF_PRESENCE_TIMEOUT = "presence_timeout"
CONF_X1 = "x1"
CONF_X2 = "x2"
CONF_Y1 = "y1"
CONF_Y2 = "y2"
ICON_ARROW_BOTTOM_RIGHT = "mdi:arrow-bottom-right"
ICON_ARROW_BOTTOM_RIGHT_BOLD_BOX_OUTLINE = "mdi:arrow-bottom-right-bold-box-outline"
ICON_ARROW_TOP_LEFT = "mdi:arrow-top-left"
ICON_ARROW_TOP_LEFT_BOLD_BOX_OUTLINE = "mdi:arrow-top-left-bold-box-outline"
MAX_ZONES = 3

PresenceTimeoutNumber = ld2450_ns.class_("PresenceTimeoutNumber", number.Number)
ZoneCoordinateNumber = ld2450_ns.class_("ZoneCoordinateNumber", number.Number)
DetectionLimitNumber = ld2450_ns.class_("DetectionLimitNumber", number.Number)

CONF_MIN_DETECTION_ANGLE = "min_detection_angle"
CONF_MAX_DETECTION_ANGLE = "max_detection_angle"
CONF_MIN_DETECTION_DISTANCE = "min_detection_distance"
CONF_MAX_DETECTION_DISTANCE = "max_detection_distance"

# (conf key, C++ DetectionLimit index, min, max, step)
DETECTION_LIMITS = [
    (CONF_MIN_DETECTION_ANGLE, 0, -90, 90, 1),
    (CONF_MAX_DETECTION_ANGLE, 1, -90, 90, 1),
    (CONF_MIN_DETECTION_DISTANCE, 2, 0, 7560, 1),
    (CONF_MAX_DETECTION_DISTANCE, 3, 0, 7560, 1),
]

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_ID): cv.declare_id(cg.EntityBase),
        cv.GenerateID(CONF_LD2450_ID): cv.use_id(LD2450Component),
        cv.Required(CONF_PRESENCE_TIMEOUT): number.number_schema(
            PresenceTimeoutNumber,
            unit_of_measurement=UNIT_SECOND,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon=ICON_TIMELAPSE,
        ),
        cv.Optional(CONF_MAX_DETECTION_ANGLE): number.number_schema(
            DetectionLimitNumber,
            unit_of_measurement=UNIT_DEGREES,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon="mdi:angle-acute",
        ),
        cv.Optional(CONF_MIN_DETECTION_ANGLE): number.number_schema(
            DetectionLimitNumber,
            unit_of_measurement=UNIT_DEGREES,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon="mdi:angle-acute",
        ),
        cv.Optional(CONF_MAX_DETECTION_DISTANCE): number.number_schema(
            DetectionLimitNumber,
            device_class=DEVICE_CLASS_DISTANCE,
            unit_of_measurement=UNIT_MILLIMETER,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon="mdi:map-marker-distance",
        ),
        cv.Optional(CONF_MIN_DETECTION_DISTANCE): number.number_schema(
            DetectionLimitNumber,
            device_class=DEVICE_CLASS_DISTANCE,
            unit_of_measurement=UNIT_MILLIMETER,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon="mdi:map-marker-distance",
        ),
    }
)

CONFIG_SCHEMA = CONFIG_SCHEMA.extend(
    {
        cv.Optional(f"zone_{n + 1}"): cv.Schema(
            {
                cv.Required(CONF_X1): number.number_schema(
                    ZoneCoordinateNumber,
                    device_class=DEVICE_CLASS_DISTANCE,
                    unit_of_measurement=UNIT_MILLIMETER,
                    entity_category=ENTITY_CATEGORY_CONFIG,
                    icon=ICON_ARROW_TOP_LEFT_BOLD_BOX_OUTLINE,
                ),
                cv.Required(CONF_Y1): number.number_schema(
                    ZoneCoordinateNumber,
                    device_class=DEVICE_CLASS_DISTANCE,
                    unit_of_measurement=UNIT_MILLIMETER,
                    entity_category=ENTITY_CATEGORY_CONFIG,
                    icon=ICON_ARROW_TOP_LEFT,
                ),
                cv.Required(CONF_X2): number.number_schema(
                    ZoneCoordinateNumber,
                    device_class=DEVICE_CLASS_DISTANCE,
                    unit_of_measurement=UNIT_MILLIMETER,
                    entity_category=ENTITY_CATEGORY_CONFIG,
                    icon=ICON_ARROW_BOTTOM_RIGHT_BOLD_BOX_OUTLINE,
                ),
                cv.Required(CONF_Y2): number.number_schema(
                    ZoneCoordinateNumber,
                    device_class=DEVICE_CLASS_DISTANCE,
                    unit_of_measurement=UNIT_MILLIMETER,
                    entity_category=ENTITY_CATEGORY_CONFIG,
                    icon=ICON_ARROW_BOTTOM_RIGHT,
                ),
            }
        )
        for n in range(MAX_ZONES)
    }
)


async def to_code(config):
    ld2450_component = await cg.get_variable(config[CONF_LD2450_ID])
    if presence_timeout_config := config.get(CONF_PRESENCE_TIMEOUT):
        n = await number.new_number(
            presence_timeout_config,
            min_value=0,
            max_value=3600,
            step=1,
        )
        await cg.register_parented(n, config[CONF_LD2450_ID])
        cg.add(ld2450_component.set_presence_timeout_number(n))
    for conf_key, index, min_value, max_value, step in DETECTION_LIMITS:
        if (limit_config := config.get(conf_key)) is not None:
            n = cg.new_Pvariable(limit_config[CONF_ID], index)
            await number.register_number(
                n, limit_config, min_value=min_value, max_value=max_value, step=step
            )
            await cg.register_parented(n, config[CONF_LD2450_ID])
            cg.add(ld2450_component.set_detection_limit_number(index, n))
    for zone_num in range(MAX_ZONES):
        if zone_conf := config.get(f"zone_{zone_num + 1}"):
            zone_x1_config = zone_conf.get(CONF_X1)
            x1 = cg.new_Pvariable(zone_x1_config[CONF_ID], zone_num)
            await number.register_number(
                x1, zone_x1_config, min_value=-4860, max_value=4860, step=1
            )
            await cg.register_parented(x1, config[CONF_LD2450_ID])

            zone_y1_config = zone_conf.get(CONF_Y1)
            y1 = cg.new_Pvariable(zone_y1_config[CONF_ID], zone_num)
            await number.register_number(
                y1, zone_y1_config, min_value=0, max_value=7560, step=1
            )
            await cg.register_parented(y1, config[CONF_LD2450_ID])

            zone_x2_config = zone_conf.get(CONF_X2)
            x2 = cg.new_Pvariable(zone_x2_config[CONF_ID], zone_num)
            await number.register_number(
                x2, zone_x2_config, min_value=-4860, max_value=4860, step=1
            )
            await cg.register_parented(x2, config[CONF_LD2450_ID])

            zone_y2_config = zone_conf.get(CONF_Y2)
            y2 = cg.new_Pvariable(zone_y2_config[CONF_ID], zone_num)
            await number.register_number(
                y2, zone_y2_config, min_value=0, max_value=7560, step=1
            )
            await cg.register_parented(y2, config[CONF_LD2450_ID])

            cg.add(ld2450_component.set_zone_numbers(zone_num, x1, y1, x2, y2))
