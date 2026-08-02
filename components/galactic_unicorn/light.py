import esphome.codegen as cg
from esphome.components import light

from . import galactic_unicorn_ns

GalacticUnicornLight = galactic_unicorn_ns.class_(
    "GalacticUnicornLight", light.LightOutput, cg.Component
)

CONFIG_SCHEMA = light.light_schema(GalacticUnicornLight, light.LightType.RGB)


async def to_code(config):
    var = await light.new_light(config)
    await cg.register_component(var, config)
