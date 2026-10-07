# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 esphome-duco contributors
#
# Deze component is een afgeleid werk van ESPHome, waarvan de C++/runtime onder
# GPLv3 staat. Zie LICENSE.
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import button

from . import DucoComponent, CONF_DUCO_ID, duco_ns

CONF_NODE = "node"
CONF_ACTION = "action"

DucoActionButton = duco_ns.class_("DucoActionButton", button.Button, cg.Parented)

# action -> (code, needs_node)
ACTIONS = {
    "filter_reset": (0, False),
    "install_mode_on": (1, False),
    "install_mode_off": (2, False),
    "identify_on": (3, True),
    "identify_off": (4, True),
    "reboot": (5, True),         # 0x10 08 <node>
    "reset_config": (6, True),   # 0x10 0A <node>
    # Link mode werkt ALLEEN met de box in install mode. Daarbuiten weigert de
    # print het commando lokaal en gaat er niets de bus op. Gemeten 26-08 op
    # node 6 (UCCO2/RF) en node 2 (EAV/bedraad), beide richtingen.
    "link_mode_on": (7, True),   # 0x10 0B <node> 01
    "link_mode_off": (8, True),  # 0x10 0B <node> 00
}


def _validate(config):
    _code, needs_node = ACTIONS[config[CONF_ACTION]]
    if needs_node and CONF_NODE not in config:
        raise cv.Invalid(f"action '{config[CONF_ACTION]}' requires 'node:'")
    if not needs_node and CONF_NODE in config:
        raise cv.Invalid(f"action '{config[CONF_ACTION]}' takes no 'node:'")
    return config


CONFIG_SCHEMA = cv.All(
    button.button_schema(DucoActionButton).extend(
        {
            cv.GenerateID(CONF_DUCO_ID): cv.use_id(DucoComponent),
            cv.Required(CONF_ACTION): cv.one_of(*ACTIONS, lower=True),
            cv.Optional(CONF_NODE): cv.int_range(min=1, max=143),
        }
    ),
    _validate,
)


async def to_code(config):
    var = await button.new_button(config)
    await cg.register_parented(var, config[CONF_DUCO_ID])
    code, _needs_node = ACTIONS[config[CONF_ACTION]]
    node = config.get(CONF_NODE, 0)
    cg.add(var.set_action(code, node))
