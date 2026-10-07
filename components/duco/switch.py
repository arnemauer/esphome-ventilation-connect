import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import switch

from . import DucoComponent, CONF_DUCO_ID, duco_ns

CONF_PROGRAM_ENABLE = "program_enable"
CONF_BUS_ID = "bus_id"

DucoProgramSwitch = duco_ns.class_("DucoProgramSwitch", switch.Switch, cg.Parented)
DucoBoxRegSwitch = duco_ns.class_("DucoBoxRegSwitch", switch.Switch, cg.Parented)

# Two variants, each with its own C++ class:
#  - program_enable: 1..8  -> DucoProgramSwitch (enable a weekly program)
#  - bus_id: 0x<mod><reg>  -> DucoBoxRegSwitch  (generic 0/1 box register, 0x24 01)
CONFIG_SCHEMA = cv.Any(
    switch.switch_schema(DucoProgramSwitch).extend(
        {
            cv.GenerateID(CONF_DUCO_ID): cv.use_id(DucoComponent),
            cv.Required(CONF_PROGRAM_ENABLE): cv.int_range(min=1, max=8),
        }
    ),
    switch.switch_schema(DucoBoxRegSwitch).extend(
        {
            cv.GenerateID(CONF_DUCO_ID): cv.use_id(DucoComponent),
            cv.Required(CONF_BUS_ID): cv.hex_uint16_t,
        }
    ),
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_DUCO_ID])
    var = await switch.new_switch(config)
    await cg.register_parented(var, config[CONF_DUCO_ID])
    if CONF_PROGRAM_ENABLE in config:
        idx = config[CONF_PROGRAM_ENABLE] - 1  # 1-based -> 0-based
        cg.add(var.set_index(idx))
        cg.add(parent.set_program_switch(idx, var))
    else:
        cg.add(var.set_bus_id(config[CONF_BUS_ID]))
