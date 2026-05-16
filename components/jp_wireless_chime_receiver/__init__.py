import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins

from esphome.const import CONF_ID, CONF_PIN

AUTO_LOAD = ["api"]

jp_ns = cg.esphome_ns.namespace("jp_wireless_chime_receiver")

JPWirelessChimeReceiver = jp_ns.class_(
    "JPWirelessChimeReceiver",
    cg.Component,
)

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(JPWirelessChimeReceiver),
    cv.Required(CONF_PIN): pins.gpio_input_pin_schema,
}).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    pin = await cg.gpio_pin_expression(config[CONF_PIN])
    cg.add(var.set_pin(pin))

    pin_number = config[CONF_PIN]["number"]
    cg.add(var.set_pin_number(pin_number))
