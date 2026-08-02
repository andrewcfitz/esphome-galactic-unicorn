import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import font, light, text
from esphome.const import CONF_COLOR, CONF_FONT, CONF_LIGHT_ID, CONF_PLATFORM
import esphome.final_validate as fv

from . import galactic_unicorn_ns

DEPENDENCIES = ["galactic_unicorn", "display"]

CONF_SCROLL_SPEED = "scroll_speed"
CONF_SCROLL_GAP = "scroll_gap"

GalacticUnicornText = galactic_unicorn_ns.class_(
    "GalacticUnicornText", text.Text, cg.Component
)


def _color(value):
    value = cv.ensure_list(cv.int_range(min=0, max=255))(value)
    if len(value) != 3:
        raise cv.Invalid("color must be a list of exactly three values: [r, g, b]")
    return value


# text.text_schema() Requires a `mode` key unless a default is supplied here;
# the brief's original schema omitted this and would fail config validation
# with "required key not provided @ data['mode']".
CONFIG_SCHEMA = (
    text.text_schema(GalacticUnicornText, mode="TEXT")
    .extend(
        {
            cv.Required(CONF_FONT): cv.use_id(font.Font),
            cv.Optional(CONF_COLOR, default=[255, 255, 255]): _color,
            cv.Optional(CONF_SCROLL_SPEED, default=20.0): cv.float_range(min=0.0, max=500.0),
            cv.Optional(CONF_SCROLL_GAP, default=12): cv.int_range(min=0, max=200),
            # Optional: when set, the light drives the text colour (and the
            # `color:` option above is ignored). Omitting it keeps today's
            # behaviour untouched, so existing configs need no changes.
            # This references the light entity's own id, which resolves to
            # its LightState wrapper (not to GalacticUnicornLight directly,
            # which only exists internally as that wrapper's output).
            # cv.use_id(light.LightState) only proves the target is *some*
            # light; it says nothing about which platform generated it.
            # gu_text.h static_casts the resolved LightOutput* straight to
            # GalacticUnicornLight*, so _validate_light_id below (run via
            # FINAL_VALIDATE_SCHEMA, once the whole config is resolved) is
            # what actually guarantees that cast is safe.
            cv.Optional(CONF_LIGHT_ID): cv.use_id(light.LightState),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
)


def _validate_light_id(config):
    if (light_id := config.get(CONF_LIGHT_ID)) is None:
        return config

    full_config = fv.full_config.get()
    light_path = full_config.get_path_for_id(light_id)[:-1]
    light_conf = full_config.get_config_for_path(light_path)
    platform = light_conf.get(CONF_PLATFORM)
    if platform != "galactic_unicorn":
        raise cv.Invalid(
            f"light_id must point at a light: entry using platform: "
            f"galactic_unicorn, but '{light_id}' uses platform: {platform}",
            path=[CONF_LIGHT_ID],
        )
    return config


FINAL_VALIDATE_SCHEMA = _validate_light_id


async def to_code(config):
    # text.register_text() (called by text.new_text()) only queues the
    # entity for the "text" platform; unlike display.register_display() it
    # does NOT also register the Component base, so we still need
    # cg.register_component() ourselves to get setup()/dump_config() called.
    var = await text.new_text(config)
    await cg.register_component(var, config)

    fnt = await cg.get_variable(config[CONF_FONT])
    cg.add(var.set_font(fnt))

    r, g, b = config[CONF_COLOR]
    cg.add(var.set_color(cg.RawExpression(f"esphome::Color({r}, {g}, {b})")))
    cg.add(var.set_scroll_speed(config[CONF_SCROLL_SPEED]))
    cg.add(var.set_scroll_gap(config[CONF_SCROLL_GAP]))

    if (light_id := config.get(CONF_LIGHT_ID)) is not None:
        light_state = await cg.get_variable(light_id)
        cg.add(var.set_light(light_state.get_output()))
