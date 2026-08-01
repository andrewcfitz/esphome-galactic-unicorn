import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID

CODEOWNERS = ["@andrewcfitz"]

CONF_GALACTIC_UNICORN_ID = "galactic_unicorn_id"
CONF_BRIGHTNESS = "brightness"

galactic_unicorn_ns = cg.esphome_ns.namespace("galactic_unicorn")
GalacticUnicornHub = galactic_unicorn_ns.class_("GalacticUnicornHub", cg.Component)

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(GalacticUnicornHub),
            cv.Optional(CONF_BRIGHTNESS, default=0.5): cv.float_range(min=0.0, max=1.0),
        }
    ).extend(cv.COMPONENT_SCHEMA),
    cv.only_on(["rp2040"]),
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    cg.add(var.set_initial_brightness(config[CONF_BRIGHTNESS]))
