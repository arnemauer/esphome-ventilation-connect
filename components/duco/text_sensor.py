# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 esphome-ventilation-connect contributors
#
# Deze component is een afgeleid werk van ESPHome, waarvan de C++/runtime onder
# GPLv3 staat. Zie LICENSE.
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import text_sensor

from . import DucoComponent, CONF_DUCO_ID

CONF_NODE = "node"
CONF_TYPE = "type"
CONF_CIRCUIT = "circuit"
CONF_PROGRAM = "program"
CONF_GROUP = "group"

# "mode" = mode name, "type" = node/component type (both node-scoped);
# "box_time" = the DucoBox wall-clock as an ISO-8601 UTC string (global, no node);
# "serial" = node serial number string (0x10 read, reg 0x1A, type 0x10) -- dit is
#            SerialDucoBox, bv. "P100001-250101-001";
# "serial_board" = het PRINTPLAAT-nummer van de node (reg 0x08), bv. "RS2440001234".
#            Vastgesteld 26-08 door in reboot.log op de ruwe bytes te zoeken: de
#            string zit in een func 0x12-antwoord op `01 <node> 00 08 06`;
# "circuit_name" = ventilation-circuit name (0x24 sub 0x11 group 02), needs 'circuit:';
# "install_mode" = ON/OFF, announced unsolicited by the box (passive; global, no node);
# "active_error" = node's active error code(s) e.g. "E56.31.01" (0x30/0x32 poll + 0x2C push);
# "sw_version" = node firmware version "maj.min.patch" (reg 0x00, read once);
# "programs_dump" = one combined line with all defined weekly programs (global, no node);
# "comm_*" = communication-print identity, decoded from the box<->print 0x20/0x22
#            handshake (global; appears after a print (re)boot).
COMM_TYPES = [
    "comm_ip", "comm_gateway", "comm_netmask", "comm_hostname",
    "comm_serial_board", "comm_serial_duco", "comm_product_id", "comm_sw_version",
    # Group 1 = the STATIC network config (what is configured), group 2 = the
    # values actually in use. Both directions write here: the box display sends
    # a 0x20 identity WRITE, the print web UI is picked up via its 0x22 replies.
    "comm_static_ip", "comm_static_netmask", "comm_static_gateway", "comm_static_dns",
]
TYPES = [
    "mode", "type", "box_time", "box_time_local", "serial", "circuit_name", "install_mode",
    # strings the DucoBox keeps in consecutive box params (0x24 sub 00), read by
    # the real print right after ITS OWN boot -- that is why they never showed up
    # in captures where the print was already running.
    "box_serial", "box_wifi_ap_key", "box_timezone_utc", "serial_board",
    "active_error", "sw_version", "programs_dump", "zone_nodes",
    "errors", "errors_info",
] + COMM_TYPES
GLOBAL_TYPES = ("box_time", "box_time_local", "box_serial", "box_wifi_ap_key",
                "box_timezone_utc",
                "install_mode", "programs_dump", "errors", "errors_info") + tuple(COMM_TYPES)


def _validate(config):
    # 'program:' is an alternative to 'type:' (a per-program summary text sensor)
    if CONF_PROGRAM in config:
        if CONF_TYPE in config:
            raise cv.Invalid("use either 'program:' or 'type:', not both")
        if CONF_NODE in config or CONF_CIRCUIT in config:
            raise cv.Invalid("'program:' takes no 'node:'/'circuit:'")
        return config
    if CONF_TYPE not in config:
        raise cv.Invalid("specify 'type:' or 'program:'")
    t = config[CONF_TYPE]
    if t in GLOBAL_TYPES:
        if CONF_NODE in config or CONF_CIRCUIT in config:
            raise cv.Invalid(f"'{t}' is global and takes no 'node:'/'circuit:'")
    elif t == "circuit_name":
        if CONF_CIRCUIT not in config:
            raise cv.Invalid("'circuit_name' requires 'circuit:' (0 = ETA, 1 = SUP, 2/3 = zonal)")
        if CONF_NODE in config:
            raise cv.Invalid("'circuit_name' is not node-addressed; remove 'node:'")
    elif t == "zone_nodes":
        if CONF_GROUP not in config:
            raise cv.Invalid("'zone_nodes' requires 'group:' (0-based device-group index)")
        if CONF_NODE in config:
            raise cv.Invalid("'zone_nodes' is not node-addressed; remove 'node:'")
    else:
        if CONF_NODE not in config:
            raise cv.Invalid(f"'node:' is required for '{t}'")
        if CONF_CIRCUIT in config:
            raise cv.Invalid("'circuit:' is only valid for 'circuit_name'")
    return config


CONFIG_SCHEMA = cv.All(
    text_sensor.text_sensor_schema().extend(
        {
            cv.GenerateID(CONF_DUCO_ID): cv.use_id(DucoComponent),
            cv.Optional(CONF_NODE): cv.int_range(min=1, max=143),
            cv.Optional(CONF_CIRCUIT): cv.int_range(min=0, max=7),
            cv.Optional(CONF_GROUP): cv.int_range(min=0, max=15),
            cv.Optional(CONF_TYPE): cv.one_of(*TYPES, lower=True),
            cv.Optional(CONF_PROGRAM): cv.int_range(min=1, max=8),  # program 1..8
        }
    ),
    _validate,
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_DUCO_ID])
    var = await text_sensor.new_text_sensor(config)
    if CONF_PROGRAM in config:
        cg.add(parent.set_program_text(config[CONF_PROGRAM] - 1, var))  # 1-based -> 0-based
        return
    t = config[CONF_TYPE]
    if t == "box_time":
        cg.add(parent.set_time_text_sensor(var))
        return
    if t == "install_mode":
        cg.add(parent.set_install_text_sensor(var))
        return
    if t == "box_serial":
        cg.add(parent.add_box_string(0x0602, 5, var))     # "P100001-250101-001"
        return
    if t == "box_wifi_ap_key":
        cg.add(parent.add_box_string(0x060A, 3, var))     # "aBC123456"
        return
    if t == "box_timezone_utc":
        cg.add(parent.set_tz_text_sensor(var))
        return
    if t == "box_time_local":
        cg.add(parent.set_time_local_text_sensor(var))
        return
    if t == "programs_dump":
        cg.add(parent.set_programs_dump(var))
        return
    if t == "errors":
        cg.add(parent.set_errors_text_sensor(var))
        return
    if t == "errors_info":
        cg.add(parent.set_errors_info_text_sensor(var))
        return
    if t == "zone_nodes":
        cg.add(parent.set_group_nodes_text(config[CONF_GROUP], var))
        return
    if t in COMM_TYPES:
        cg.add(parent.set_comm_text(t, var))
        return
    if t == "serial_board":
        # 0x10 01 <node> 00 08 06 -> reply 01 06 <12-byte string>; string op offset 2
        node = config[CONF_NODE]
        cg.add(parent.add_string_sensor(node, 0x10, [0x01, node, 0x00, 0x08, 0x06], 2, var))
        return
    if t == "serial":
        # 0x10 01 <node> 00 1A 10 -> reply 01 10 <32-byte string>; string at offset 2
        node = config[CONF_NODE]
        cg.add(parent.add_string_sensor(node, 0x10, [0x01, node, 0x00, 0x1A, 0x10], 2, var))
        return
    if t == "circuit_name":
        # 0x24 11 <circuit> 01 02 06 -> reply 01 <c> 01 02 <24-byte string>; offset 4
        c = config[CONF_CIRCUIT]
        cg.add(parent.add_string_sensor(1, 0x24, [0x11, c, 0x01, 0x02, 0x06], 4, var))
        return
    cg.add(parent.set_node_text_sensor(config[CONF_NODE], config[CONF_TYPE], var))
