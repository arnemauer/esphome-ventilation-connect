# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 esphome-ventilation-connect contributors
#
# Deze component is een afgeleid werk van ESPHome, waarvan de C++/runtime onder
# GPLv3 staat. Zie LICENSE.
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import uart, time as time_
from esphome.const import CONF_ID, CONF_TIME_ID

CODEOWNERS = ["@you"]
DEPENDENCIES = ["uart"]
# `network` is optional but lets the responder auto-fill its IP from the ESP.
AUTO_LOAD = ["network"]
MULTI_CONF = True

duco_ns = cg.esphome_ns.namespace("duco")
DucoComponent = duco_ns.class_(
    "DucoComponent", cg.PollingComponent, uart.UARTDevice
)

CONF_DUCO_ID = "duco_id"
CONF_SCAN_MAX = "scan_max"
CONF_SNIFFER = "sniffer"
CONF_PASSIVE = "passive"
CONF_DEBUG_RAW = "debug_raw"
CONF_DECODE_LOG = "decode_log"
CONF_TIME_THRESHOLD = "time_threshold"
CONF_COMM_RESPONDER = "comm_responder"
CONF_COMM_IDENTITY = "comm_identity"
CONF_LOG_PREFIX = "log_prefix"
CONF_CONFIG_PARAM_INTERVAL = "config_param_interval"

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(DucoComponent),
            cv.Optional(CONF_SCAN_MAX, default=0): cv.int_range(min=0, max=143),
            # debug: passive raw-byte sniffer (sends nothing, dumps all RX bytes)
            cv.Optional(CONF_SNIFFER, default=False): cv.boolean,
            # passive decode: listen only, publish parameters the box pushes itself
            cv.Optional(CONF_PASSIVE, default=False): cv.boolean,
            # debug: dump every raw TX/RX byte while polling + decoding normally
            cv.Optional(CONF_DEBUG_RAW, default=False): cv.boolean,
            # debug: log a human-readable description of every received frame
            cv.Optional(CONF_DECODE_LOG, default=False): cv.boolean,
            # active time-push: keep the DucoBox clock synced to this time source.
            # Only pushes when ESP time is valid AND the box clock drifts > threshold.
            cv.Optional(CONF_TIME_ID): cv.use_id(time_.RealTimeClock),
            cv.Optional(CONF_TIME_THRESHOLD, default="2s"): cv.All(
                cv.positive_time_period_seconds,
                cv.Range(max=cv.TimePeriod(seconds=255)),
            ),
            # comm-print responder: answer the box's 0x20 identity queries as the print
            cv.Optional(CONF_COMM_RESPONDER, default=False): cv.boolean,
            # Tag printed in every decode/RX line. Use it when you run one duco:
            # hub per bus direction (sniffer between box and comm print).
            cv.Optional(CONF_LOG_PREFIX, default=""): cv.string,
            # Hoe vaak param:/bus_id:-sensoren voor box-INSTELLINGEN worden gelezen als
            # ze geen eigen update_interval hebben. Het gaat om de 92 IDs die de echte
            # communicatieprint maar elke 15 minuten opvraagt (duco_poll_classes.h).
            # 0s = elke ronde, zoals voorheen.
            cv.Optional(CONF_CONFIG_PARAM_INTERVAL, default="15min"): cv.positive_time_period_milliseconds,
            cv.Optional(CONF_COMM_IDENTITY): cv.Schema(
                {
                    cv.Optional("ip"): cv.string,          # "192.168.1.50"
                    cv.Optional("netmask"): cv.string,     # "255.255.255.0"
                    cv.Optional("gateway"): cv.string,
                    cv.Optional("mac"): cv.string,         # "b8:d6:1a:00:12:34"
                    # Strings are packed into fixed 4-byte identity words. Going
                    # over the limit would spill into the next field (g2/f6 resp.
                    # g3/f8), which must stay 1, so cap them here.
                    cv.Optional("hostname"): cv.All(cv.string, cv.Length(max=12)),
                    cv.Optional("serial_board"): cv.All(cv.string, cv.Length(max=12)),
                    cv.Optional("serial_duco"): cv.All(cv.string, cv.Length(max=20)),
                    cv.Optional("product_id"): cv.uint32_t,
                    cv.Optional("version"): cv.string,     # "8.3.0"
                }
            ),
        }
    )
    .extend(cv.polling_component_schema("10s"))
    .extend(uart.UART_DEVICE_SCHEMA)
)

# The DucoBox internal bus is normally 57600 8N1, but for sniffing you may want to
# try other baud rates, so we only require tx/rx here (not a fixed baud).
# In listen-only modes (passive / sniffer) the component never transmits, so a
# tx_pin is not just unnecessary but undesirable: leaving it out makes it
# physically impossible to disturb a bus that the real comm print is driving.
def _final_validate(config):
    listen_only = config.get(CONF_PASSIVE) or config.get(CONF_SNIFFER)
    return uart.final_validate_device_schema(
        "duco", require_rx=True, require_tx=not listen_only
    )(config)


FINAL_VALIDATE_SCHEMA = _final_validate


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)
    cg.add(var.set_scan_max(config[CONF_SCAN_MAX]))
    cg.add(var.set_sniffer(config[CONF_SNIFFER]))
    cg.add(var.set_passive(config[CONF_PASSIVE]))
    cg.add(var.set_debug_raw(config[CONF_DEBUG_RAW]))
    cg.add(var.set_decode_log(config[CONF_DECODE_LOG]))
    if CONF_TIME_ID in config:
        rtc = await cg.get_variable(config[CONF_TIME_ID])
        cg.add(var.set_rtc(rtc))
        cg.add(var.set_time_threshold(int(config[CONF_TIME_THRESHOLD].total_seconds)))
    cg.add(var.set_comm_responder(config[CONF_COMM_RESPONDER]))
    cg.add(var.set_log_prefix(config[CONF_LOG_PREFIX]))
    cg.add(var.set_config_param_interval(config[CONF_CONFIG_PARAM_INTERVAL].total_milliseconds))
    if CONF_COMM_IDENTITY in config:
        ci = config[CONF_COMM_IDENTITY]
        if "ip" in ci:
            cg.add(var.set_comm_ident_ip(ci["ip"]))
        if "netmask" in ci:
            cg.add(var.set_comm_ident_netmask(ci["netmask"]))
        if "gateway" in ci:
            cg.add(var.set_comm_ident_gateway(ci["gateway"]))
        if "mac" in ci:
            cg.add(var.set_comm_ident_mac(ci["mac"]))
        if "hostname" in ci:
            cg.add(var.set_comm_ident_hostname(ci["hostname"]))
        if "serial_board" in ci:
            cg.add(var.set_comm_ident_serial_board(ci["serial_board"]))
        if "serial_duco" in ci:
            cg.add(var.set_comm_ident_serial_duco(ci["serial_duco"]))
        if "product_id" in ci:
            cg.add(var.set_comm_ident_product_id(ci["product_id"]))
        if "version" in ci:
            cg.add(var.set_comm_ident_version(ci["version"]))
