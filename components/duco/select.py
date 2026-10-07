# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 esphome-duco contributors
#
# Deze component is een afgeleid werk van ESPHome, waarvan de C++/runtime onder
# GPLv3 staat. Zie LICENSE.
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import select
from esphome.const import CONF_ID

from . import DucoComponent, CONF_DUCO_ID, duco_ns

DucoModeSelect = duco_ns.class_("DucoModeSelect", select.Select, cg.Parented)
DucoParamSelect = duco_ns.class_("DucoParamSelect", select.Select, cg.Parented)
DucoBoxSelect = duco_ns.class_("DucoBoxSelect", select.Select, cg.Parented)

# The x2/x3 variants ARE settable: the firmware packs a press count into the top
# two bits of the mode byte (value = count << 6 | base), so MAN3x3 goes out as
# 0xC6, not as 16. See duco.cpp.
MODES = [
    "AUTO", "MAN1", "MAN2", "MAN3", "EMPT", "CNT1", "CNT2", "CNT3",
    "MAN1x2", "MAN2x2", "MAN3x2", "MAN1x3", "MAN2x3", "MAN3x3",
]

CONF_NODE = "node"
CONF_PARAM = "param"
CONF_ACTION = "action"

# Box-parameters met benoemde waarden. De positie in de lijst IS de buswaarde;
# de namen en hun volgorde komen letterlijk uit het Enum-veld dat /action van de
# print teruggeeft. Bewezen 26-08 (#61): een /action met ValType Enum wordt door
# de print vertaald naar een gewone box-parameterschrijfactie (0x24 01).
_ZONES = ["IDLE"] + [f"SUP_ZONE_{i}" for i in range(1, 9)] +          [f"ETA_ZONE_{i}" for i in range(1, 15)]
_CALIB_SUB = ["IDLE", "SUP_ZONE_1", "SUP_ZONE_2", "ETA_ZONE_1", "SUP_ZONE_3",
              "SUP_ZONE_4", "SUP_ZONE_5", "SUP_ZONE_6", "SUP_ZONE_7", "SUP_ZONE_8",
              "SUP_ALL_OPEN", "FIND_CALIB_DATA", "BASE_NF", "PEAK_NF", "ETA_ZONE_2",
              "ETA_ZONE_3", "ETA_ZONE_4", "ETA_ZONE_5", "ETA_ZONE_6", "ETA_ZONE_7",
              "ETA_ZONE_8", "INIT_MEAS_LOAD"]
# actie -> (box-parameter, opties)
ACTIONS = {
    "identify_zone":      (0x0410, _ZONES),
    "calibration_state":  (0x0A50, ["STOP", "CONFIG_MAX", "START", "VERIFY_LOW", "VERIFY_HIGH"]),
    "calibration_substate": (0x0A58, _CALIB_SUB),
}

# Node-configuratieparameters met BENOEMDE waarden, via service 6 (func 0x18).
# De positie in de lijst is de waarde die over de bus gaat: index 0 = waarde 0.
# Namen horen bij de Duco Network Tool 5.0.2.0. Let op de valstrik daar: de
# enumleden heten m1/m2/m3 maar hebben impliciete waarden 0/1/2 -- de
# m-nummering loopt een op.
# param -> (register, opties)
PARAMS = {
    "uc_error_mode":   (0x14, ["Disabled", "On input", "Automatic"]),
    "uc_warning_mode": (0x18, ["Disabled", "On input", "Automatic"]),
    "uc_info_mode":    (0x19, ["Disabled", "On input", "Automatic"]),
    # De box meldt voor SwitchMode bereik 1..12, terwijl de tool maar drie
    # waarden kent. "Overrule" (0) valt dus onder het gemelde minimum en wordt
    # mogelijk geweigerd; 3..12 zijn onbekend en staan daarom niet in de lijst.
    "switch_mode":     (0x0A, ["Overrule", "Heatpump", "Presence"]),
    "co2_det_mode":    (0x1A, ["Standard Air Quality detection", "Toilet Detection"]),
}

# Without 'node:' this addresses the DucoBox itself (node 1), which is how this
# select behaved before. With 'node:' it drives that component instead -- the bus
# frame is 0x0C `04 <node> <mode>`, so the node is simply an argument.
def _validate(config):
    if CONF_ACTION in config:
        if CONF_PARAM in config or CONF_NODE in config:
            raise cv.Invalid("'action:' takes no 'param:' or 'node:'")
        config[CONF_ID].type = DucoBoxSelect
        return config
    if CONF_PARAM not in config:
        return config
    if CONF_NODE not in config:
        raise cv.Invalid(f"'{config[CONF_PARAM]}' requires 'node:' (1 = de DucoBox)")
    # Het schema declareert standaard de standen-select; een benoemde parameter
    # is een andere klasse, dus het type van de gegenereerde id omzetten.
    config[CONF_ID].type = DucoParamSelect
    return config


CONFIG_SCHEMA = cv.All(
    select.select_schema(DucoModeSelect).extend(
        {
            cv.GenerateID(CONF_DUCO_ID): cv.use_id(DucoComponent),
            cv.Optional(CONF_NODE): cv.int_range(min=1, max=143),
            cv.Optional(CONF_PARAM): cv.one_of(*PARAMS, lower=True),
            cv.Optional(CONF_ACTION): cv.one_of(*ACTIONS, lower=True),
        }
    ),
    _validate,
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_DUCO_ID])
    if CONF_ACTION in config:
        bus_id, opties = ACTIONS[config[CONF_ACTION]]
        var = cg.new_Pvariable(config[CONF_ID])
        await select.register_select(var, config, options=opties)
        await cg.register_parented(var, parent)
        cg.add(var.set_bus_id(bus_id))
        cg.add(parent.add_box_select(bus_id, var))
        return
    if CONF_PARAM in config:
        reg, opties = PARAMS[config[CONF_PARAM]]
        var = cg.new_Pvariable(config[CONF_ID])
        await select.register_select(var, config, options=opties)
        await cg.register_parented(var, parent)
        cg.add(var.set_param(config[CONF_NODE], reg))
        cg.add(parent.add_node_param_select(config[CONF_NODE], reg, var))
        return
    var = await select.new_select(config, options=MODES)
    await cg.register_parented(var, parent)
    node = config.get(CONF_NODE, 1)
    cg.add(var.set_node(node))
    # let the component push the live mode back into the dropdown
    cg.add(parent.set_node_mode_select(node, var))
