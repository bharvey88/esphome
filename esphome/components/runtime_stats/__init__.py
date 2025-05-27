"""
Runtime statistics component for ESPHome.
"""

import esphome.codegen as cg
import esphome.config_validation as cv

DEPENDENCIES = []

CONF_ENABLED = "enabled"
CONF_LOG_INTERVAL = "log_interval"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_ENABLED, default=True): cv.boolean,
        cv.Optional(
            CONF_LOG_INTERVAL, default=60000
        ): cv.positive_time_period_milliseconds,
    }
)


async def to_code(config):
    """Generate code for the runtime statistics component."""
    cg.add(cg.App.set_runtime_stats_enabled(config[CONF_ENABLED]))
    cg.add(cg.App.set_runtime_stats_log_interval(config[CONF_LOG_INTERVAL]))
