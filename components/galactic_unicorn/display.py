import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import display
from esphome.const import CONF_ID, CONF_LAMBDA

from . import (
    CONF_GALACTIC_UNICORN_ID,
    GalacticUnicornHub,
    galactic_unicorn_ns,
)

DEPENDENCIES = ["galactic_unicorn"]

GalacticUnicornDisplay = galactic_unicorn_ns.class_(
    "GalacticUnicornDisplay", display.DisplayBuffer
)

CONFIG_SCHEMA = display.FULL_DISPLAY_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(GalacticUnicornDisplay),
        cv.GenerateID(CONF_GALACTIC_UNICORN_ID): cv.use_id(GalacticUnicornHub),
    }
).extend(cv.polling_component_schema("33ms"))


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await display.register_display(var, config)

    hub = await cg.get_variable(config[CONF_GALACTIC_UNICORN_ID])
    cg.add(var.set_hub(hub))

    if CONF_LAMBDA in config:
        lambda_ = await cg.process_lambda(
            config[CONF_LAMBDA], [(display.DisplayRef, "it")], return_type=cg.void
        )
        cg.add(var.set_writer(lambda_))
