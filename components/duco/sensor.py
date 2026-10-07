# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 esphome-ventilation-connect contributors
#
# Deze component is een afgeleid werk van ESPHome, waarvan de C++/runtime onder
# GPLv3 staat. Zie LICENSE.
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import (
    CONF_ACCURACY_DECIMALS,
    CONF_DEVICE_CLASS,
    CONF_UNIT_OF_MEASUREMENT,
    DEVICE_CLASS_CARBON_DIOXIDE,
    DEVICE_CLASS_TEMPERATURE,
    STATE_CLASS_MEASUREMENT,
    UNIT_CELSIUS,
    UNIT_PARTS_PER_MILLION,
    UNIT_PERCENT,
    UNIT_SECOND,
)

from . import DucoComponent, CONF_DUCO_ID
from .duco_params_gen import PARAM_NAMES
from .duco_verified_gen import VERIFIED_NAMES

CONF_NODE = "node"
CONF_TYPE = "type"
CONF_PARAM = "param"
CONF_SERVICE = "service"
CONF_REQUEST = "request"
CONF_OFFSET = "offset"
CONF_VALUE_TYPE = "value_type"
CONF_SCALE = "scale"
CONF_PUSHED = "pushed_param"
CONF_SIGNED = "signed"
CONF_BUS_ID = "bus_id"
CONF_CIRCUIT = "circuit"
CONF_GROUP = "group"

# Named zone/group reads via the 0x24 sub-11 (circuit) / sub-13 (group) services.
# type -> (index_key, request template with None=index, reply offset, size, scale, unit)
ZONE_TYPES = {
    # circuit temperatures (0x24 11 <c> <prop> 04), x0.1 degC
    "zone_temp": (CONF_CIRCUIT, [0x11, None, 0x00, 0x04], 4, 2, 0.1, UNIT_CELSIUS),
    "zone_temp_filt": (CONF_CIRCUIT, [0x11, None, 0x01, 0x04], 4, 2, 0.1, UNIT_CELSIUS),
    "zone_temp_req_high": (CONF_CIRCUIT, [0x11, None, 0x06, 0x04], 4, 2, 0.1, UNIT_CELSIUS),
    "zone_temp_req_low": (CONF_CIRCUIT, [0x11, None, 0x07, 0x04], 4, 2, 0.1, UNIT_CELSIUS),
    # TempHandler config read-back (0x24 11): TempReq grp05/prop00, TempNodeSrc grp06/prop01
    "zone_temp_req": (CONF_CIRCUIT, [0x11, None, 0x00, 0x05], 4, 2, 0.1, UNIT_CELSIUS),
    "zone_temp_node_src": (CONF_CIRCUIT, [0x11, None, 0x01, 0x06], 4, 1, 1.0, None),
    # ActiveHouse.DataExt (module 0x80) -- external data you can feed INTO a zone.
    # Index map recovered 23-08 by lining the sub-0x12 writes up against the 21
    # PATCH requests in "chrome log zone.har", which changed one field at a time.
    "zone_ext_temp": (CONF_CIRCUIT, [0x11, None, 0x00, 0x80], 4, 2, 0.1, UNIT_CELSIUS),
    "zone_ext_temp_valid": (CONF_CIRCUIT, [0x11, None, 0x01, 0x80], 4, 1, 1.0, None),
    "zone_ext_temp_status": (CONF_CIRCUIT, [0x11, None, 0x02, 0x80], 4, 1, 1.0, None),
    "zone_ext_temp_req_high": (CONF_CIRCUIT, [0x11, None, 0x04, 0x80], 4, 2, 0.1, UNIT_CELSIUS),
    "zone_ext_temp_req_low": (CONF_CIRCUIT, [0x11, None, 0x08, 0x80], 4, 2, 0.1, UNIT_CELSIUS),
    "zone_ext_temp_req_valid": (CONF_CIRCUIT, [0x11, None, 0x09, 0x80], 4, 1, 1.0, None),
    "zone_ext_temp_req_status": (CONF_CIRCUIT, [0x11, None, 0x0A, 0x80], 4, 1, 1.0, None),
    # device-group (0x24 13): type=grp?/prop00 val02, node-count=prop00 val01,
    # error=prop00 val81 (0 in captures, tentative). The node LIST is a text_sensor.
    "group_type": (CONF_GROUP, [0x13, None, 0x00, 0x00, 0x02], 5, 1, 1.0, None),
    "group_node_count": (CONF_GROUP, [0x13, None, 0x00, 0x00, 0x01], 5, 1, 1.0, None),
    "group_error": (CONF_GROUP, [0x13, None, 0x00, 0x81, 0x01], 5, 1, 1.0, None),
}

# Temperatuurvelden zijn SIGNED; ze werden tot 25-08 unsigned gelezen.
# Bewezen met /info/zones/3: bij TempReq=0 meldt de zone TempReqLow = -20 (0,1 gr C)
# en TempReqHigh = +20 -- een dode band van +/- 2,0 gr C rond het setpoint. De API
# serialiseert dat als u32 (4294967276). Unsigned lezen maakte daar 6551,6 gr C van.
ZONE_SIGNED = {
    "zone_temp", "zone_temp_filt", "zone_temp_req_high", "zone_temp_req_low",
    "zone_temp_req",
    "zone_ext_temp", "zone_ext_temp_req_high", "zone_ext_temp_req_low",
}

# Fixed, byte-exact confirmed fields
TYPES = {
    "mode": (None, None),
    "flow": (UNIT_PERCENT, None),          # live offset 2 (Pos, actual position)
    "flow_target": (UNIT_PERCENT, None),   # live offset 3 (FlowLvlTgt / requested)
    "flow_ovrl": (None, None),             # live offset 4 (FlowLvlOvrl, 255=none)
    "time_remaining": (UNIT_SECOND, None),
    "co2": (UNIT_PARTS_PER_MILLION, DEVICE_CLASS_CARBON_DIOXIDE),  # 0x49 u16[1]
    "temp": (UNIT_CELSIUS, DEVICE_CLASS_TEMPERATURE),             # 0x49 u16[0] x0.1
    # 0x49-blok u16[3]: de uptime van de node in minuten (node 1 = de DucoBox).
    # 'iaq_co2' was een verkeerde naam voor ditzelfde veld en blijft als alias werken.
    "uptime": ("min", None),
    "iaq_co2": ("min", None),
    "rssi": (None, None),
    # node live-block diagnostics (0x0C 02 <node> -> 0x0E, offsets 6/7/8/9)
    "comm_error_ctr": (None, None),  # CommErrorCtr (offset 6)
    "rssi_n2m": (None, None),        # RssiRfN2M   (offset 7, RF nodes)
    "hop_rf": (None, None),          # HopRf       (offset 8, RF nodes)
    "rssi_n2h": (None, None),        # RssiRfN2H   (offset 9, RF nodes)
    # type block (0x0C 01 <node>) identity fields
    "network_type": (None, None),    # [1] 1=WI 2=RF 3=VIRT
    "addr": (None, None),            # [2]
    "sub_addr": (None, None),        # [3]
    "parent": (None, None),          # [4]
    "asso": (None, None),            # [5]
    # dedicated node-register reads
    "subtype": (None, None),         # reg 0x15
    "week_ctr": (None, None),        # reg 0x18 u16[0]
    "boot_ctr": (None, None),        # reg 0x18 u16[1]
    "product_id": (None, None),      # reg 0x00 u16[0] (read once)
}

# global responder-health counters (no node) — diagnostic
DIAG_TYPES = [
    "comm_queries_answered", "comm_unknown_queries", "comm_status_polls",
    "comm_pushes_acked", "comm_box_retransmits", "comm_enrollment_restarts",
    "comm_ip_shared", "comm_mac_shared", "modbus_address",
    # box clock settings, read from the 0x24 sub 02/04 reply (0x26 bytes 9 and 10)
    "box_timezone", "box_dst",
    # g1/f3 = DHCP on/off, g6/f0 = WiFi-AP state (1 = on, 0 = off)
    "static_dhcp",
    # g2/f06 = cloudverbinding. Bewezen 24-08: StopAzureConnection zette hem
    # binnen 6 s op 0, RestartAzureConnection binnen 9 s terug op 1.
    "cloud_connected",
    # g6/f0 network mode: 0=DISABLED 1=AUTOMATIC 2=ETHERNET 3=WIFI_CLIENT 4=WIFI_AP
    # (enum recovered from the ESP32 image's pointer table, 23-08)
    "network_mode",
    # g5/f0 netwerktoestand van de print. Waarden bewezen 25-08 door AP starten/
    # stoppen, DHCP omzetten en de kabel eruit te trekken met de sniffer aan:
    #   1 = verbonden, 2 = GEEN verbinding, 3 = AP actief, 4 = bezig met wijzigen
    # (4 is een overgang, geen toestand: hij gaat aan elke wissel vooraf)
    "network_state",
    "wifi_ap_state",   # deprecated alias for network_mode
]
for _d in DIAG_TYPES:
    TYPES[_d] = (None, None)

# value_type -> (size, signed)
VALUE_TYPES = {
    "u8": (1, False),
    "i8": (1, True),
    "u16": (2, False),
    "i16": (2, True),
    "u32": (4, False),
}


def _validate(config):
    keys = [k for k in (CONF_TYPE, CONF_PARAM, CONF_SERVICE, CONF_PUSHED, CONF_BUS_ID) if k in config]
    if len(keys) != 1:
        raise cv.Invalid(
            "Specify exactly one of 'type:' (fixed field), 'param:' (named parameter), "
            "'service:'+'request:' (active raw read), 'pushed_param:' (passive), or "
            "'bus_id:' (read a box register by (module,register) id)."
        )
    # node-scoped reads need a node; passive pushes, box-register reads and the global
    # diagnostic counters do not
    if CONF_PUSHED in config or CONF_BUS_ID in config:
        if CONF_NODE in config:
            raise cv.Invalid("'pushed_param:' / 'bus_id:' are not node-addressed; remove 'node:'")
    elif CONF_TYPE in config and config[CONF_TYPE] in DIAG_TYPES:
        if CONF_NODE in config:
            raise cv.Invalid("responder-health counters are global; remove 'node:'")
    elif CONF_TYPE in config and config[CONF_TYPE] in ZONE_TYPES:
        idx_key = ZONE_TYPES[config[CONF_TYPE]][0]
        if idx_key not in config:
            raise cv.Invalid(f"'{config[CONF_TYPE]}' requires '{idx_key}:'")
        if CONF_NODE in config:
            raise cv.Invalid(f"'{config[CONF_TYPE]}' is not node-addressed; remove 'node:'")
    elif CONF_PARAM in config:
        # box parameters (0x24 read) target the box itself; default to node 1
        if CONF_NODE not in config:
            config[CONF_NODE] = 1
    else:
        if CONF_NODE not in config:
            raise cv.Invalid("'node:' is required for type/param/service reads")
    if CONF_SERVICE in config:
        for k in (CONF_REQUEST, CONF_OFFSET, CONF_VALUE_TYPE):
            if k not in config:
                raise cv.Invalid(f"raw read requires '{k}:'")
    return config


# state_class is set on the schema so it is validated into the proper enum
# (do NOT inject it into `config` in to_code -- that passes a raw string to C++).
CONFIG_SCHEMA = cv.All(
    sensor.sensor_schema(state_class=STATE_CLASS_MEASUREMENT).extend(
        {
            cv.GenerateID(CONF_DUCO_ID): cv.use_id(DucoComponent),
            cv.Optional(CONF_NODE): cv.int_range(min=1, max=143),
            cv.Optional(CONF_TYPE): cv.one_of(*TYPES, *ZONE_TYPES, lower=True),
            cv.Optional(CONF_CIRCUIT): cv.int_range(min=0, max=7),
            cv.Optional(CONF_GROUP): cv.int_range(min=0, max=15),
            # Geverifieerde API-paden EERST: die winnen in duco_find_verified().
            # De lijst uit duco_params.h blijft geldig voor alles wat we zelf niet
            # op de bus hebben gezien -- maar klopt niet bij module 0x00/0x03/0x0A/0x13.
            cv.Optional(CONF_PARAM): cv.one_of(*VERIFIED_NAMES, *PARAM_NAMES),
            cv.Optional(CONF_SERVICE): cv.hex_uint8_t,
            cv.Optional(CONF_REQUEST): cv.ensure_list(cv.hex_uint8_t),
            cv.Optional(CONF_OFFSET): cv.uint8_t,
            cv.Optional(CONF_VALUE_TYPE): cv.one_of(*VALUE_TYPES, lower=True),
            # passive push: the parameter id the DucoBox reports by itself
            cv.Optional(CONF_PUSHED): cv.hex_uint8_t,
            # box register read by (module<<8 | register) id, via the 0x24 service
            cv.Optional(CONF_BUS_ID): cv.hex_uint16_t,
            cv.Optional(CONF_SIGNED, default=False): cv.boolean,
            cv.Optional(CONF_SCALE, default=0.0): cv.float_,
        }
    ),
    _validate,
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_DUCO_ID])

    # passive push subscription -- no node, published when the box reports the param
    if CONF_PUSHED in config:
        var = await sensor.new_sensor(config)
        cg.add(
            parent.add_pushed_sensor(
                config[CONF_PUSHED], config[CONF_SCALE], config[CONF_SIGNED], var
            )
        )
        return

    # box register read by (module,register) id -- 0x24 00 <reg> <mod>, reply value at offset 3
    if CONF_BUS_ID in config:
        bid = config[CONF_BUS_ID]
        size, sgn = VALUE_TYPES[config.get(CONF_VALUE_TYPE, "u32")]
        if config.get(CONF_SIGNED):
            sgn = True
        # a fractional scale (e.g. 0.1) implies decimals -- default them so temps
        # show "21.5" instead of a rounded "22" (unless the user set it explicitly).
        sc = config.get(CONF_SCALE) or 0.0
        if 0.0 < sc < 1.0 and CONF_ACCURACY_DECIMALS not in config:
            import math
            config[CONF_ACCURACY_DECIMALS] = max(0, int(round(-math.log10(sc))))
        var = await sensor.new_sensor(config)
        req = [0x00, bid & 0xFF, (bid >> 8) & 0xFF]
        cg.add(parent.add_raw_sensor(1, 0x24, req, 3, size, sgn, config[CONF_SCALE] or 1.0, var))
        return

    # global responder-health counters (no node)
    if CONF_TYPE in config and config[CONF_TYPE] in DIAG_TYPES:
        var = await sensor.new_sensor(config)
        cg.add(parent.set_diag_sensor(config[CONF_TYPE], var))
        return

    # named zone/group reads (0x24 sub-11 / sub-13) -- no node, index via circuit/group
    if CONF_TYPE in config and config[CONF_TYPE] in ZONE_TYPES:
        idx_key, tmpl, offset, size, scale, unit = ZONE_TYPES[config[CONF_TYPE]]
        idx = config[idx_key]
        req = [idx if b is None else b for b in tmpl]
        if unit and not config.get(CONF_UNIT_OF_MEASUREMENT):
            config[CONF_UNIT_OF_MEASUREMENT] = unit
        var = await sensor.new_sensor(config)
        # node arg is unused for these box-level reads; pass 1 as a placeholder
        cg.add(parent.add_raw_sensor(1, 0x24, req, offset, size,
                                     config[CONF_TYPE] in ZONE_SIGNED, scale, var))
        return

    node = config[CONF_NODE]

    if CONF_TYPE in config:
        # apply per-type unit/device_class defaults (strings are fine in config),
        # unless the user overrode them in YAML.
        unit, dev_class = TYPES[config[CONF_TYPE]]
        if unit and not config.get(CONF_UNIT_OF_MEASUREMENT):
            config[CONF_UNIT_OF_MEASUREMENT] = unit
        if dev_class and not config.get(CONF_DEVICE_CLASS):
            config[CONF_DEVICE_CLASS] = dev_class
        # temp is x0.1 degC -> show one decimal by default
        if config[CONF_TYPE] == "temp" and CONF_ACCURACY_DECIMALS not in config:
            config[CONF_ACCURACY_DECIMALS] = 1
        var = await sensor.new_sensor(config)
        cg.add(parent.set_node_sensor(node, config[CONF_TYPE], var))
        return

    var = await sensor.new_sensor(config)

    if CONF_PARAM in config:
        cg.add(parent.add_param_sensor(node, config[CONF_PARAM], config[CONF_SCALE], var))
        return

    # raw read
    size, sgn = VALUE_TYPES[config[CONF_VALUE_TYPE]]
    scale = config[CONF_SCALE] or 1.0
    cg.add(
        parent.add_raw_sensor(
            node,
            config[CONF_SERVICE],
            list(config[CONF_REQUEST]),
            config[CONF_OFFSET],
            size,
            sgn,
            scale,
            var,
        )
    )
