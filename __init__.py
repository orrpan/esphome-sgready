import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome import pins
from esphome.components import sensor

# from esphome.components import switch
from esphome.cpp_helpers import gpio_pin_expression

MULTI_CONF = True
CONF_SGREADY_COMPONENT_ID = "sgready_component_id"

sgready_component_ns = cg.esphome_ns.namespace("sgready_component")
SGReadyComponent = sgready_component_ns.class_("SGReadyComponent", cg.Component)

CONF_PIN_SGREADY_A = "sgready_a"
CONF_PIN_SGREADY_B = "sgready_b"
CONF_BLOCK_ORDERED_MODE = "block_ordered_mode"
CONF_BLOCK_ENCOURAGE_MODE = "block_encourage_mode"
CONF_TEMPERATURE_SENSOR = "temperature_sensor"
CONF_PRICE_LEVEL_SENSOR = "price_level_sensor"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(SGReadyComponent),
        cv.Required(CONF_PIN_SGREADY_A): pins.gpio_output_pin_schema,
        cv.Required(CONF_PIN_SGREADY_B): pins.gpio_output_pin_schema,
        cv.Optional(CONF_TEMPERATURE_SENSOR): cv.use_id(sensor.Sensor),
        cv.Optional(CONF_PRICE_LEVEL_SENSOR): cv.use_id(sensor.Sensor),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])

    pin_a = await gpio_pin_expression(config[CONF_PIN_SGREADY_A])
    pin_b = await gpio_pin_expression(config[CONF_PIN_SGREADY_B])
    cg.add(var.set_output_pin(pin_a, pin_b))

    # if a sensor entity (existing sensor object) is provided, get it and attach to component
    if CONF_TEMPERATURE_SENSOR in config:
        temp_sensor = await cg.get_variable(config[CONF_TEMPERATURE_SENSOR])
        cg.add(var.set_temperature_sensor(temp_sensor))
    if CONF_PRICE_LEVEL_SENSOR in config:
        price_level_sensor = await cg.get_variable(config[CONF_PRICE_LEVEL_SENSOR])
        cg.add(var.set_price_level_sensor(price_level_sensor))

    await cg.register_component(var, config)
