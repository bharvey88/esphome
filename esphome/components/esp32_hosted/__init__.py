import os

from esphome import pins
import esphome.codegen as cg
from esphome.components import esp32
import esphome.config_validation as cv
from esphome.const import (
    CONF_CLK_PIN,
    CONF_ID,
    CONF_RESET_PIN,
    CONF_VARIANT,
    KEY_CORE,
    KEY_FRAMEWORK_VERSION,
)
from esphome.core import CORE

CODEOWNERS = ["@swoboda1337"]
AUTO_LOAD = ["esp32"]

esp32_hosted_ns = cg.esphome_ns.namespace("esp32_hosted")
ESP32Hosted = esp32_hosted_ns.class_("ESP32Hosted", cg.Component)

CONF_ACTIVE_HIGH = "active_high"
CONF_CMD_PIN = "cmd_pin"
CONF_D0_PIN = "d0_pin"
CONF_D1_PIN = "d1_pin"
CONF_D2_PIN = "d2_pin"
CONF_D3_PIN = "d3_pin"
CONF_SLOT = "slot"

VARIANTS_WITHOUT_WIFI = {
    esp32.VARIANT_ESP32H2,
    esp32.VARIANT_ESP32P4,
}


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(ESP32Hosted),
            cv.Required(CONF_VARIANT): cv.one_of(*esp32.VARIANTS, upper=True),
            cv.Optional(CONF_ACTIVE_HIGH, default=True): cv.boolean,
            cv.Required(CONF_CLK_PIN): pins.internal_gpio_output_pin_number,
            cv.Required(CONF_CMD_PIN): pins.internal_gpio_output_pin_number,
            cv.Required(CONF_D0_PIN): pins.internal_gpio_output_pin_number,
            cv.Required(CONF_D1_PIN): pins.internal_gpio_output_pin_number,
            cv.Required(CONF_D2_PIN): pins.internal_gpio_output_pin_number,
            cv.Required(CONF_D3_PIN): pins.internal_gpio_output_pin_number,
            cv.Required(CONF_RESET_PIN): pins.internal_gpio_output_pin_number,
            cv.Optional(CONF_SLOT, default=1): cv.int_range(min=0, max=1),
        }
    ),
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    framework_ver: cv.Version = CORE.data[KEY_CORE][KEY_FRAMEWORK_VERSION]

    if config[CONF_ACTIVE_HIGH]:
        esp32.add_idf_sdkconfig_option("CONFIG_ESP_HOSTED_SDIO_RESET_ACTIVE_HIGH", True)
    else:
        esp32.add_idf_sdkconfig_option("CONFIG_ESP_HOSTED_SDIO_RESET_ACTIVE_LOW", True)

    esp32.add_idf_sdkconfig_option(
        f"CONFIG_SLAVE_IDF_TARGET_{config[CONF_VARIANT]}",  # NOLINT
        True,
    )

    esp32.add_idf_sdkconfig_option(
        f"CONFIG_ESP_HOSTED_SDIO_SLOT_{config[CONF_SLOT]}", True
    )

    esp32.add_idf_sdkconfig_option(
        f"CONFIG_ESP_HOSTED_PRIV_SDIO_PIN_CLK_SLOT_{config[CONF_SLOT]}",
        config[CONF_CLK_PIN],
    )

    esp32.add_idf_sdkconfig_option(
        f"CONFIG_ESP_HOSTED_PRIV_SDIO_PIN_CMD_SLOT_{config[CONF_SLOT]}",
        config[CONF_CMD_PIN],
    )

    esp32.add_idf_sdkconfig_option(
        f"CONFIG_ESP_HOSTED_PRIV_SDIO_PIN_D0_SLOT_{config[CONF_SLOT]}",
        config[CONF_D0_PIN],
    )

    esp32.add_idf_sdkconfig_option(
        f"CONFIG_ESP_HOSTED_PRIV_SDIO_PIN_D1_4BIT_BUS_SLOT_{config[CONF_SLOT]}",
        config[CONF_D1_PIN],
    )

    esp32.add_idf_sdkconfig_option(
        f"CONFIG_ESP_HOSTED_PRIV_SDIO_PIN_D2_4BIT_BUS_SLOT_{config[CONF_SLOT]}",
        config[CONF_D2_PIN],
    )

    esp32.add_idf_sdkconfig_option(
        f"CONFIG_ESP_HOSTED_PRIV_SDIO_PIN_D3_4BIT_BUS_SLOT_{config[CONF_SLOT]}",
        config[CONF_D3_PIN],
    )

    esp32.add_idf_sdkconfig_option(
        "CONFIG_ESP_HOSTED_SDIO_GPIO_RESET_SLAVE",  # NOLINT
        config[CONF_RESET_PIN],
    )

    os.environ["ESP_IDF_VERSION"] = f"{framework_ver.major}.{framework_ver.minor}"

    if config[CONF_VARIANT] not in VARIANTS_WITHOUT_WIFI:
        esp32.add_idf_component(
            name="esp_wifi_remote",
            repo="https://github.com/espressif/esp-wifi-remote.git",
            path="components/esp_wifi_remote",
            ref="wifi_remote-v0.10.2",
        )

    esp32.add_extra_script(
        "post",
        "esp32_hosted.py",
        os.path.join(os.path.dirname(__file__), "esp32_hosted.py.script"),
    )
