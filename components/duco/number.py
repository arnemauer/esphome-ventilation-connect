import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import number

from . import DucoComponent, CONF_DUCO_ID, duco_ns

CONF_NODE = "node"
CONF_TYPE = "type"
CONF_CIRCUIT = "circuit"
CONF_BUS_ID = "bus_id"
CONF_MIN = "min_value"
CONF_MAX = "max_value"
CONF_STEP = "step"
CONF_SCALE = "scale"
CONF_UNIT = "unit_of_measurement"

DucoNumber = duco_ns.class_("DucoNumber", number.Number, cg.Parented)

# type -> (kind, min, max, step, unit, index_key).  Node-addressed:
#   kind 0 = Co2SetPoint (0x18 reg 0x03), kind 1 = TempOffset (0x10 reg 0x52, signed),
#   kind 2 = Parent (0x0C 08), kind 3 = Asso (0x0C 09).
# Circuit-addressed zone config (0x24 12 write):
#   kind 10 = TempReq (grp05/prop00), kind 11 = TempNodeSrc (grp06/prop01).
TYPES = {
    "co2_setpoint": (0, 0, 2000, 10, "ppm", CONF_NODE),
    "temp_offset": (1, -50, 50, 1, "", CONF_NODE),
    "parent": (2, 0, 143, 1, "", CONF_NODE),
    "asso": (3, 0, 143, 1, "", CONF_NODE),
    "zone_temp_req": (10, -500, 500, 1, "", CONF_CIRCUIT),      # x0.1 degC (raw)
    "zone_temp_node_src": (11, 0, 255, 1, "", CONF_CIRCUIT),
    # Box-global clock settings (0x24 01 box-param write, no node/circuit index).
    # Identified 19-08 by watching the real comm print write them when TimeZone
    # and DST were changed on its web interface.
    "box_timezone": (20, -11, 12, 1, "", None),   # box-param 0x0802
    "box_dst": (20, 0, 1, 1, "", None),           # box-param 0x0806
}

# named types that address a box-global register instead of a node/circuit
BUS_TYPES = {"box_timezone": 0x0802, "box_dst": 0x0806}

# Node-configuratieparameters via service 6 (func 0x18).
#   lezen     0x18  00 <node> <param>                  -> 01 <val> <inc> <min> <max>  (u16 LE)
#   schrijven 0x18  01 <node> <param> <waarde:u16 BE>  -> 01
# Namen, bereiken en stappen komen uit de box zelf: de leesreply levert waarde,
# stap, min en max, en die zijn 24-08 automatisch gekoppeld aan /config/nodes/<n>.
# Zie #48 en #51 in de overdracht.
#
# De grenzen hieronder zijn wat NODE 1-6 en 52 melden; ze verschillen soms per
# nodetype (FlowLvlMan2 heeft min 10 op de ene node en 15 op de andere). Overschrijf
# ze in de yaml met min_value/max_value/step als jouw node iets anders meldt.
# type -> (register, min, max, stap, eenheid)
NODE_PARAMS = {
    "flow_lvl_auto_min":   (0x00,  10,  100,  5, "%"),
    "flow_lvl_auto_max":   (0x01,  30,  100,  5, "%"),
    "flow_max":            (0x02,   0,  750,  5, "m3/h"),
    "flow_lvl_man1":       (0x06,   0,   50,  5, "%"),
    "flow_lvl_man2":       (0x07,  10,  100,  5, "%"),
    "flow_lvl_man3":       (0x08,  50,  100,  5, "%"),
    "time_man":            (0x09,   5,  720,  5, "min"),
    # 0 = Overrule, 1 = Heatpump, 2 = Presence. Namen uit de Duco Network Tool 5.0.2.0.
    # LET OP: de box meldt bereik 1..12, de tool kent maar drie waarden. Wat 3..12
    # doen is onbekend, en of 0 uberhaupt te zetten is ook.
    "switch_mode":         (0x0A,   1,   12,  1, ""),
    "flow_lvl_switch":     (0x0B,   0,  100,  5, "%"),
    "temp_dep_enable":     (0x0E,   0,    1,  1, ""),
    "show_sensor_lvl":     (0x12,   0,  100,  5, "%"),
    "time_overrun_switch": (0x13,   0,   60,  1, "min"),
    # 0 = Disabled, 1 = On input, 2 = Automatic (namen uit de Network Tool; let
    # op dat de leden daar m1/m2/m3 heten maar impliciet 0/1/2 zijn).
    # Bevestigd: bus 2 toonde "Automatic".
    "uc_error_mode":       (0x14,   0,    2,  1, ""),
    "uc_warning_mode":     (0x18,   0,    2,  1, ""),
    "uc_info_mode":        (0x19,   0,    2,  1, ""),
    "co2_det_mode":        (0x1A,   0,    2,  1, ""),
}


def _validate(config):
    # 'bus_id:' = generic writable box register (0x24 01); 'type:' = a named field.
    if CONF_BUS_ID in config:
        if CONF_TYPE in config:
            raise cv.Invalid("use either 'bus_id:' or 'type:', not both")
        if CONF_NODE in config or CONF_CIRCUIT in config:
            raise cv.Invalid("'bus_id:' is not node/circuit-addressed")
        for k in (CONF_MIN, CONF_MAX):
            if k not in config:
                raise cv.Invalid(f"a 'bus_id:' number requires '{k}:'")
        return config
    if CONF_TYPE not in config:
        raise cv.Invalid("specify 'type:' or 'bus_id:'")
    if config[CONF_TYPE] in NODE_PARAMS:
        if CONF_NODE not in config:
            raise cv.Invalid(f"'{config[CONF_TYPE]}' requires 'node:'")
        if CONF_CIRCUIT in config:
            raise cv.Invalid(f"'{config[CONF_TYPE]}' uses 'node:', not 'circuit:'")
        return config
    if config[CONF_TYPE] in BUS_TYPES:
        for k in (CONF_NODE, CONF_CIRCUIT):
            if k in config:
                raise cv.Invalid(f"'{config[CONF_TYPE]}' is box-global, drop '{k}:'")
        return config
    idx_key = TYPES[config[CONF_TYPE]][5]
    if idx_key not in config:
        raise cv.Invalid(f"'{config[CONF_TYPE]}' requires '{idx_key}:'")
    other = CONF_CIRCUIT if idx_key == CONF_NODE else CONF_NODE
    if other in config:
        raise cv.Invalid(f"'{config[CONF_TYPE]}' uses '{idx_key}:', not '{other}:'")
    return config


CONFIG_SCHEMA = cv.All(
    number.number_schema(DucoNumber).extend(
        {
            cv.GenerateID(CONF_DUCO_ID): cv.use_id(DucoComponent),
            cv.Optional(CONF_NODE): cv.int_range(min=1, max=143),
            cv.Optional(CONF_CIRCUIT): cv.int_range(min=0, max=7),
            cv.Optional(CONF_TYPE): cv.one_of(*TYPES, *NODE_PARAMS, lower=True),
            # generic box-register write (0x24 01 <reg> <mod> <u32>)
            cv.Optional(CONF_BUS_ID): cv.hex_uint16_t,
            cv.Optional(CONF_MIN): cv.float_,
            cv.Optional(CONF_MAX): cv.float_,
            cv.Optional(CONF_STEP): cv.float_,
            cv.Optional(CONF_SCALE, default=1.0): cv.float_,
        }
    ),
    _validate,
)


async def to_code(config):
    if CONF_BUS_ID in config:
        var = await number.new_number(
            config, min_value=config[CONF_MIN], max_value=config[CONF_MAX],
            step=config.get(CONF_STEP) or 1.0,
        )
        await cg.register_parented(var, config[CONF_DUCO_ID])
        cg.add(var.set_bus_target(config[CONF_BUS_ID], config[CONF_SCALE]))
        parent = await cg.get_variable(config[CONF_DUCO_ID])
        cg.add(parent.set_bus_number(config[CONF_BUS_ID], var))
        return

    if config[CONF_TYPE] in NODE_PARAMS:
        reg, lo, hi, step, unit = NODE_PARAMS[config[CONF_TYPE]]
        lo = config.get(CONF_MIN, lo)
        hi = config.get(CONF_MAX, hi)
        step = config.get(CONF_STEP) or step
        var = await number.new_number(config, min_value=lo, max_value=hi, step=step)
        await cg.register_parented(var, config[CONF_DUCO_ID])
        cg.add(var.set_node_param_target(reg, config[CONF_NODE]))
        # terugregistreren zodat de component de werkelijke waarde kan inlezen
        parent = await cg.get_variable(config[CONF_DUCO_ID])
        cg.add(parent.add_node_param(config[CONF_NODE], reg, var))
        return   # 'unit_of_measurement:' zelf in de yaml zetten; zie de notitie onderaan

    kind, lo, hi, step, unit, idx_key = TYPES[config[CONF_TYPE]]
    var = await number.new_number(config, min_value=lo, max_value=hi, step=step)
    await cg.register_parented(var, config[CONF_DUCO_ID])
    if config[CONF_TYPE] in BUS_TYPES:
        bus_id = BUS_TYPES[config[CONF_TYPE]]
        cg.add(var.set_bus_target(bus_id, 1.0))
        # register the other way round too, so the component can push the value
        # the box reports into the slider instead of leaving it at 0
        parent = await cg.get_variable(config[CONF_DUCO_ID])
        cg.add(parent.set_bus_number(bus_id, var))
        return
    # both node- and circuit-addressed kinds carry their index in the same target slot
    cg.add(var.set_target(kind, config[idx_key]))
    # (unit_of_measurement is cosmetic; number::Number has no runtime setter in all
    #  ESPHome versions, so set it in YAML with 'unit_of_measurement:' if desired.)
