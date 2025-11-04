import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import switch
from esphome.const import CONF_ID

from . import (
    SGReadyComponent,
    CONF_SGREADY_COMPONENT_ID,
)

DEPENDENCIES = ["sgready_component"]

# Use the switch platform schema; require the parent component id
CONFIG_SCHEMA = (
    switch.switch_schema(SGReadyComponent)
    .extend({cv.GenerateID(CONF_SGREADY_COMPONENT_ID): cv.use_id(SGReadyComponent)})
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_SGREADY_COMPONENT_ID])

    # create switch instance for this platform entry
    var = cg.new_Pvariable(config[CONF_ID])

    # register the switch entity and notify parent component
    await switch.register_switch(var, config)
    cg.add(parent.register_switch(var))
