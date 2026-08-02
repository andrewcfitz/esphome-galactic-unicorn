import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import display
from esphome.components.display import CONF_SHOW_TEST_CARD
from esphome.const import CONF_ID, CONF_LAMBDA, CONF_PAGES

from . import (
    CONF_GALACTIC_UNICORN_ID,
    GalacticUnicornHub,
    galactic_unicorn_ns,
)

DEPENDENCIES = ["galactic_unicorn"]

GalacticUnicornDisplay = galactic_unicorn_ns.class_(
    "GalacticUnicornDisplay", display.DisplayBuffer
)


def _validate_galactic_unicorn_display(config):
    # Upstream's test_card computes image_h = min(get_height() - 20, 255),
    # which is -9 on this panel's 11px height, then loops
    # `for (i = 0; i != image_h; i++)` with a signed/unsigned mismatch that
    # runs for roughly 4 billion iterations. That hangs the board solid, so
    # refuse the option here instead of letting a user find out the hard
    # way.
    if config.get(CONF_SHOW_TEST_CARD):
        raise cv.Invalid(
            f"'{CONF_SHOW_TEST_CARD}' is not supported on the Galactic Unicorn. "
            "The panel is only 11px tall, and upstream's test card requires "
            "a height of at least 21px to render; on this panel it hangs the "
            "board instead."
        )

    # Codegen only sets a writer when `lambda` is present, and this
    # component doesn't auto-generate one (there's no way to guess which
    # text entity you want drawn). Without either `lambda` or `pages`,
    # nothing ever gets drawn and the panel just stays blank with no error,
    # so catch it here instead.
    if CONF_LAMBDA not in config and CONF_PAGES not in config:
        raise cv.Invalid(
            "Either 'lambda' or 'pages' is required, otherwise nothing is "
            "ever drawn to the panel. For a minimal config that just draws "
            "the scrolling text entity, use:\n"
            "  lambda: |-\n"
            "      id(your_text)->draw(it);"
        )

    return config


CONFIG_SCHEMA = cv.All(
    display.FULL_DISPLAY_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(GalacticUnicornDisplay),
            cv.GenerateID(CONF_GALACTIC_UNICORN_ID): cv.use_id(GalacticUnicornHub),
        }
    ).extend(cv.polling_component_schema("33ms")),
    _validate_galactic_unicorn_display,
)


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
