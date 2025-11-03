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


# import esphome.codegen as cg
# import esphome.config_validation as cv
# from esphome.components import switch
# # from esphome.const import CONF_ID

# from . import (
#     SGReadyComponent,
#     CONF_SGREADY_COMPONENT_ID,
#     CONF_BLOCK_ENCOURAGE_MODE,
#     CONF_BLOCK_ORDERED_MODE,
# )

# # CONF_SECOND_ID = "second_id"  # add new key name

# DEPENDENCIES = ["sgready_component"]

# CONFIG_SCHEMA = (
#     switch.switch_schema(SGReadyComponent)
#     .extend({cv.GenerateID(CONF_SGREADY_COMPONENT_ID): cv.use_id(SGReadyComponent)})
#     .extend({cv.Optional(CONF_BLOCK_ENCOURAGE_MODE): cv.declare_id(switch.Switch)})
#     .extend({cv.Optional(CONF_BLOCK_ORDERED_MODE): cv.declare_id(switch.Switch)})
#     .extend(cv.COMPONENT_SCHEMA)
# )


# async def to_code(config):
#     parent = await cg.get_variable(config[CONF_SGREADY_COMPONENT_ID])

#     var1 = cg.new_Pvariable(config[CONF_BLOCK_ENCOURAGE_MODE])
#     await switch.register_switch(var1, config)
#     cg.add(parent.register_switch(var1))

#     var2 = cg.new_Pvariable(config[CONF_BLOCK_ORDERED_MODE])
#     await switch.register_switch(var2, config)
#     cg.add(parent.register_switch(var2))
