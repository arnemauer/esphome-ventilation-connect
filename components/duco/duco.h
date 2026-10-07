#pragma once
// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 esphome-duco contributors
//
// Deze component is een afgeleid werk van ESPHome, waarvan de C++/runtime onder
// GPLv3 staat. Zie LICENSE.
// ESPHome external component for the DucoBox internal UART protocol.
// Reverse-engineered frame format:
//   AA 55 | LEN | FUNC | ID | DATA... | CRC16(MODBUS, little-endian)
//   LEN   = number of bytes in FUNC+ID+DATA
//   CRC   = CRC-16/MODBUS over LEN+FUNC+ID+DATA
//   byte-stuffing: any 0xAA inside LEN..CRC is followed by a 0x01 filler
//                  (filler is NOT counted in LEN and NOT in the CRC)
// Bus: 57600 8N1, board is master, box replies ACK (FUNC+1) then REPLY (FUNC+2).

#include "esphome/core/component.h"
#include "esphome/components/uart/uart.h"
#include "duco_params.h"     // gegenereerd: volledige 463-parameterlijst + duco_find_param()
#include "duco_verified.h"   // gegenereerd uit duco-parameters-per-module.md: op de
                             // bus GEVERIFIEERDE adressen, op volledig API-pad
#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor.h"
#endif
#ifdef USE_TEXT_SENSOR
#include "esphome/components/text_sensor/text_sensor.h"
#endif
#ifdef USE_SELECT
#include "esphome/components/select/select.h"
#include "esphome/core/helpers.h"
#endif
#ifdef USE_TIME
#include "esphome/components/time/real_time_clock.h"
#endif
#ifdef USE_NUMBER
#include "esphome/components/number/number.h"
#include "esphome/core/helpers.h"
#endif
#ifdef USE_BUTTON
#include "esphome/components/button/button.h"
#include "esphome/core/helpers.h"
#endif
#ifdef USE_SWITCH
#include "esphome/components/switch/switch.h"
#include "esphome/core/helpers.h"
#endif

#include <cmath>
#include <map>
#include <set>
#include <deque>
#include <vector>
#include <string>
#include <array>

namespace esphome {
namespace duco {

// Ventilation modes (HOLDING param xx00 / live-block byte 0)
const char *mode_to_string(uint8_t mode);
uint8_t string_to_mode(const std::string &s);
// Node/component type codes (live 0x0C sub 0x01)
const char *node_type_to_string(uint8_t type);

struct NodeEntities {
#ifdef USE_SENSOR
  sensor::Sensor *mode{nullptr};
  sensor::Sensor *flow{nullptr};            // FlowLvlTgt  (%)
  sensor::Sensor *time_remaining{nullptr};  // TimeStateRemain (s)
  sensor::Sensor *uptime{nullptr};          // UpTime in minuten (0x49 block u16[3])
  sensor::Sensor *co2{nullptr};             // raw CO2 ppm  (0x49 block u16[1])
  sensor::Sensor *rssi{nullptr};            // param 0xE10E via 0x24 (optional)
  sensor::Sensor *temp{nullptr};            // 0x49 block u16[0] x0.1 (degC)
  // node live block (0x0C 02 <node>) diagnostic fields:
  sensor::Sensor *comm_error_ctr{nullptr};  // offset 6  (CommErrorCtr)
  sensor::Sensor *rssi_n2m{nullptr};        // offset 7  (RssiRfN2M, RF nodes)
  sensor::Sensor *hop_rf{nullptr};          // offset 8  (HopRf, RF nodes)
  sensor::Sensor *rssi_n2h{nullptr};        // offset 9  (RssiRfN2H, RF nodes)
  sensor::Sensor *flow_target{nullptr};     // live offset 3 (FlowLvlTgt / requested)
  sensor::Sensor *flow_ovrl{nullptr};       // live offset 4 (FlowLvlOvrl, 255=none)
  // type block (0x0C 01 <node>) identity fields:
  sensor::Sensor *network_type{nullptr};    // [1] 1=WI 2=RF 3=VIRT
  sensor::Sensor *addr{nullptr};            // [2]
  sensor::Sensor *sub_addr{nullptr};        // [3]
  sensor::Sensor *parent{nullptr};          // [4]
  sensor::Sensor *asso{nullptr};            // [5]
  // dedicated node-register reads:
  sensor::Sensor *subtype{nullptr};         // reg 0x15 type01 (u16)
  sensor::Sensor *week_ctr{nullptr};        // reg 0x18 type02 u16[0]
  sensor::Sensor *boot_ctr{nullptr};        // reg 0x18 type02 u16[1]
  sensor::Sensor *product_id{nullptr};      // reg 0x00 type04 u16[0] (ProductId)
#endif
#ifdef USE_SELECT
  select::Select *mode_select{nullptr};   // dropdown; follows the live mode
  // select::Select has no readable `state` in ESPHome 2026.7, so remember what we
  // last pushed and only publish on an actual change.
  uint8_t mode_select_last{0xFF};
#endif
#ifdef USE_TEXT_SENSOR
  text_sensor::TextSensor *mode_text{nullptr};
  text_sensor::TextSensor *type_text{nullptr};
  text_sensor::TextSensor *active_error{nullptr};  // "E56.31.01" or "" (via 0x30/0x32 + 0x2C push)
  text_sensor::TextSensor *sw_version{nullptr};    // reg 0x00 type04 u16[1..3] "maj.min.patch"
#endif
  bool want_co2{false};     // issue the 0x10 sensor-block (0x49) read for this node
  bool want_type{false};    // issue the 0x0C/01 type read for this node
  bool want_errors{false};  // issue the 0x30 03 <node> error-list poll
  bool want_subtype{false}; // issue the reg 0x15 read
  bool want_ctrs{false};    // issue the reg 0x18 (week/boot counters) read
  bool want_nodever{false}; // issue the reg 0x00 (ProductId+SwVersion) read
  bool nodever_done{false}; // ProductId/SwVersion are static: read once
  uint8_t node{0};
};

enum RequestKind : uint8_t {
  REQ_LIVE, REQ_CO2, REQ_TYPE, REQ_GEN, REQ_SETMODE, REQ_SETTIME, REQ_READTIME, REQ_WRITE,
  REQ_STR, REQ_ERRORS, REQ_SUBTYPE, REQ_CTRS, REQ_PROG, REQ_NODEVER, REQ_ZCOUNT, REQ_ZNODES,
  REQ_ERRLOG,  // 0x30 sub 01: read ALL active errors from the box error-log service
  REQ_READTIME_LOCAL,  // 0x24 sub 02: box time with TimeZone+Dst already applied
  REQ_BOXSTR,  // 0x24 sub 00 run of box params holding one string
  REQ_NPARAM,  // svc6 (0x18 sub 00): read one node-config parameter back
  REQ_BOXSEL   // 0x24 sub 00: lees een box-parameter terug voor een keuzelijst
};

#ifdef USE_SELECT
// Een box-parameter met benoemde waarden (enum). De positie in de optielijst is
// de waarde die over de bus gaat. Acties uit /action met ValType Enum zijn
// hetzelfde: de print vertaalt ze naar een gewone 0x24 01 schrijfactie (#61).
struct BoxSelect {
  uint16_t id;
  select::Select *sel;
};
#endif

#if defined(USE_NUMBER) || defined(USE_SELECT)
// Een node-configuratieparameter via service 6. Lezen levert waarde/stap/min/max,
// schrijven zet alleen de waarde. Zie #51 in de overdracht voor het formaat --
// let op dat lezen little-endian is en SCHRIJVEN big-endian.
// Eén van beide wijzers is gezet: een getalsparameter wordt een number, een
// parameter met benoemde waarden een select.
struct NodeParam {
  uint8_t node;
  uint8_t reg;
#ifdef USE_NUMBER
  number::Number *num{nullptr};
#endif
#ifdef USE_SELECT
  select::Select *sel{nullptr};
#endif
};
#endif

// One weekly program (schedule slot), read via 0x24 0C <idx>.
struct ProgramRec {
  bool valid{false};
  uint8_t type{0};      // 1=VENT_SUP, 5=NIGHT_BOOST, 6=VENT_RH_SUMMER
  uint8_t zone{0};      // 0-based on the bus (0xFF = all)
  uint8_t group{0};
  uint8_t valcnt{0};
  uint8_t cnt{0};
  uint8_t npts{0};
  uint32_t pack{0};     // 21-bit: bit0=Enable, bit1=cnt-flag, bits2-8=Days, bits9-20=Months
  uint8_t pts[30];      // npts * (hour, minute, value)
};

struct Request {
  RequestKind kind;
  uint8_t func;
  uint8_t node;
  std::vector<uint8_t> data;
  int gen_index;   // index into gen_reads_ for REQ_GEN (0 otherwise; unused)
};

#ifdef USE_SENSOR
// A generic read: send `func` + `data`, then decode `size` bytes at `offset`
// of the reply DATA as (un)signed little-endian * scale. Covers the 0x24
// parameter service (any of the 463 params) and fully-manual raw reads.
struct GenRead {
  uint8_t func;
  std::vector<uint8_t> data;
  uint8_t offset;
  uint8_t size;
  bool sgn;
  float scale;
  uint8_t node;
  sensor::Sensor *sens;
};

// A passive subscription: publish when the box pushes parameter `param`.
struct PushSub {
  uint8_t param;
  float scale;
  bool sgn;
  sensor::Sensor *sens;
};
#endif

#ifdef USE_TEXT_SENSOR
// A one-shot string read (node serial via 0x10 type 0x10, circuit name via
// 0x24 sub 0x11 group 02). The NUL-padded ASCII starts at `offset` in the reply
// DATA. Retried each poll round until it succeeds once (the strings are static).
// A string stored across consecutive BOX parameters (0x24 sub 00 reads, reply
// `01 <lo> <hi> <4 chars>`). The DucoBox keeps its own serial at 0x0602..0x0606
// and the WiFi-AP key at 0x060A..0x060C, forwards (NOT word-reversed like the
// identity handshake). The real print reads these once, right after ITS boot.
struct BoxStrRead {
  uint16_t first;                  // first parameter id
  uint8_t words;                   // how many 4-byte words to fetch
  uint32_t got;                    // bitmask of words received
  std::vector<uint8_t> buf;
  bool done;
  text_sensor::TextSensor *sens;
};
struct StrRead {
  uint8_t func;
  std::vector<uint8_t> data;
  uint8_t offset;
  uint8_t node;
  bool done;
  text_sensor::TextSensor *sens;
};
#endif

class DucoComponent : public PollingComponent, public uart::UARTDevice {
 public:
  void setup() override;
  void loop() override;
  void update() override;   // schedule a poll round
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

  // registration from the platform *.py generators
  NodeEntities &node_entities(uint8_t node);
  void set_scan_max(uint8_t n) { this->scan_max_ = n; }
#ifdef USE_SENSOR
  void set_node_sensor(uint8_t node, const std::string &field, sensor::Sensor *s);
  // responder health counters (diagnostic; global, no node)
  void set_diag_sensor(const std::string &name, sensor::Sensor *s) {
    this->diag_sensors_[name] = s;
  }
  // Read any of the 463 parameters by name via the 0x24 service.
  bool add_param_sensor(uint8_t node, const std::string &name, float scale, sensor::Sensor *s);
  // Fully-manual raw read: caller supplies the request and how to decode the reply.
  void add_raw_sensor(uint8_t node, uint8_t func, const std::vector<uint8_t> &req,
                      uint8_t offset, uint8_t size, bool sgn, float scale, sensor::Sensor *s);
  // Passive push: subscribe to a parameter that the DucoBox reports unsolicited
  // (FUNC 0x08 / sub 0x02). Published whenever the box pushes that param id.
  void add_pushed_sensor(uint8_t param, float scale, bool sgn, sensor::Sensor *s);
#endif
#ifdef USE_TEXT_SENSOR
  void set_node_text_sensor(uint8_t node, const std::string &field, text_sensor::TextSensor *s);
#ifdef USE_NUMBER
  // Writable box registers also need a way BACK: without this the slider keeps
  // showing 0 until someone moves it, while the sensor beside it reads the real
  // value off the bus. Registered per box-param id so we can push the truth in.
  void set_bus_number(uint16_t bus_id, number::Number *n) { this->bus_numbers_[bus_id] = n; }
  // Node-configuratieparameter (service 6). Zelfde reden als hierboven: zonder
  // terugleesactie staat de schuifregelaar op zijn minimum tot iemand hem beweegt,
  // en dan schrijf je ongemerkt een verkeerde waarde naar de box.
  void add_node_param(uint8_t node, uint8_t reg, number::Number *n) {
    NodeParam np{node, reg};
    np.num = n;
    this->node_params_.push_back(np);
  }
#endif
#ifdef USE_SELECT
  // Zelfde terugleesactie als bij de number-variant, zodat de keuzelijst toont
  // wat de box werkelijk heeft staan.
  void add_box_select(uint16_t id, select::Select *s) {
    this->box_selects_.push_back(BoxSelect{id, s});
  }
  void add_node_param_select(uint8_t node, uint8_t reg, select::Select *s) {
    NodeParam np{node, reg};
    np.sel = s;
    this->node_params_.push_back(np);
  }
  void set_node_mode_select(uint8_t node, select::Select *s) {
    this->nodes_[node].mode_select = s;
  }
#endif
  // global box wall-clock (Unix time), published as an ISO-8601 UTC string
  void set_time_text_sensor(text_sensor::TextSensor *s) { this->time_text_ = s; }
  void set_time_local_text_sensor(text_sensor::TextSensor *s) { this->time_local_text_ = s; }
  void set_tz_text_sensor(text_sensor::TextSensor *s) { this->tz_text_ = s; }
  // install/pairing mode, announced unsolicited by the box (0x2C `0C 01 04 <1|2>`)
  void set_install_text_sensor(text_sensor::TextSensor *s) { this->install_text_ = s; }
  // comm-print identity, decoded from the box<->print 0x20/0x22 handshake
  void set_comm_text(const std::string &field, text_sensor::TextSensor *s) {
    this->comm_text_[field] = s; this->want_comm_ident_ = true;
  }
  // one-shot bus string read (node serial number, circuit name, ...)
  void add_string_sensor(uint8_t node, uint8_t func, const std::vector<uint8_t> &req,
                         uint8_t offset, text_sensor::TextSensor *s);
  void add_box_string(uint16_t first_id, uint8_t words, text_sensor::TextSensor *s);
  // weekly program summary text sensors (idx 0..7) + a combined diagnostic dump
  void set_program_text(uint8_t idx, text_sensor::TextSensor *s);
  void set_programs_dump(text_sensor::TextSensor *s) { this->programs_dump_ = s; this->want_programs_ = true; }
  // global error-log (0x30 sub 01): all active errors as "node 6: E56.31.01, ..."
  void set_errors_text_sensor(text_sensor::TextSensor *s) { this->errors_all_ = s; }
  // human-readable explanation (NL) of the active error(s), from the service manual
  void set_errors_info_text_sensor(text_sensor::TextSensor *s) { this->errors_info_ = s; }
  // group node list "[2,4,6]" via 0x24 13 (count read, then array read)
  void set_group_nodes_text(uint8_t idx, text_sensor::TextSensor *s) { this->group_nodes_text_[idx] = s; }
#endif

  // program writes (require active master; ignored when passive/sniffer).
  void set_program_enable(uint8_t idx, bool en);   // preserve schedule, flip Enable
  void write_program_schedule(uint8_t idx, bool en, uint32_t days, uint32_t months,
                              uint8_t cnt, uint8_t valcnt);
  void write_program_points(uint8_t idx, const std::vector<uint8_t> &pts);  // flat H,M,V,...
#ifdef USE_SWITCH
  void set_program_switch(uint8_t idx, switch_::Switch *s) {
    this->program_sw_[idx] = s; this->want_programs_ = true;
  }
#endif

  // control
  // node 1 = the DucoBox itself; any other node addresses that component.
  void set_mode(uint8_t mode, uint8_t node = 1);
  void set_mode_str(const std::string &s, uint8_t node = 1) {
    this->set_mode(string_to_mode(s), node);
  }

  // writes / actions (require active-master mode; ignored when passive/sniffer).
  // The comm-print refresh/commit that normally follows a node write is replicated.
  void write_box_param(uint16_t id, uint32_t val);                  // 0x24 01 reg mod val32
  void write_node_param18(uint8_t node, uint8_t reg, uint16_t val); // 0x18 01 node reg val16
  void write_node_param10(uint8_t node, uint8_t reg, int16_t val);  // 0x10 02 node 00 reg 01 val16
  void set_identify(uint8_t node, bool on);                         // 0x10 04 node 0/1 (LED)
  void set_install_mode(bool on);                                   // 0x10 00 0/1
  void reset_filter();                                              // write param 0x0A30 = 1
  // node topology / lifecycle actions (confirmed by REST correlation)
  void node_reboot(uint8_t node);                                   // 0x10 08 <node>
  void node_reset_config(uint8_t node);                             // 0x10 0A <node>
  // LET OP: de box moet in INSTALL mode staan, anders weigert de print het
  // commando lokaal (HTTP Code 8) en gaat er niets de bus op. Gemeten 26-08.
  void set_link_mode(uint8_t node, bool on);                        // 0x10 0B <node> <0/1>
  void set_node_parent(uint8_t node, uint8_t parent);               // 0x0C 08 <node> <parent>
  void set_node_asso(uint8_t node, uint8_t asso);                   // 0x0C 09 <node> <asso>
  // zone/circuit config write: 0x24 12 <circuit> <prop> <group> <value u32 LE>
  void write_zone_param(uint8_t circuit, uint8_t prop, uint8_t group, uint32_t value);
  void write_zone_name(uint8_t circuit, const std::string &name);   // group 0x02, ASCII words

  // debug: passive raw-byte sniffer. When on, the component sends nothing and just
  // dumps every received byte (hex + ASCII), frames delimited by an idle gap.
  void set_sniffer(bool b) { this->sniffer_ = b; }

  // passive decode: never transmit; just listen to the frames the DucoBox pushes
  // onto the bus by itself and publish the parameters found in them.
  void set_passive(bool b) { this->passive_ = b; }

#ifdef USE_TIME
  // active time-push: keep the DucoBox clock in sync with an ESPHome time source.
  void set_rtc(time::RealTimeClock *rtc) { this->rtc_ = rtc; }
  void set_time_threshold(uint8_t s) { this->time_threshold_ = s; }
#endif

  // debug: also dump every raw TX/RX byte (hex + ASCII) while polling + decoding
  // normally. Unlike `sniffer`, this does NOT change behaviour, only adds logging.
  void set_debug_raw(bool b) { this->debug_raw_ = b; }

  // debug: log a human-readable description of every received frame (what kind of
  // message it is, decoded), on top of / instead of the raw hex.
  void set_decode_log(bool b) { this->decode_log_ = b; }

  // comm-print RESPONDER: answer the box's 0x20 identity queries with 0x22 replies,
  // so the ESP can stand in for the communication print (experimental, master only).
  void set_comm_responder(bool b) { this->comm_responder_ = b; }
  void set_log_prefix(const std::string &s) { this->log_prefix_ = s; }
  void set_comm_ident_ip(const std::string &s) { this->comm_cfg_ip_ = s; }
  void set_comm_ident_netmask(const std::string &s) { this->comm_cfg_mask_ = s; }
  void set_comm_ident_gateway(const std::string &s) { this->comm_cfg_gw_ = s; }
  void set_comm_ident_mac(const std::string &s) { this->comm_cfg_mac_ = s; }
  void set_comm_ident_hostname(const std::string &s) { this->comm_cfg_host_ = s; }
  void set_comm_ident_serial_board(const std::string &s) { this->comm_cfg_serb_ = s; }
  void set_comm_ident_serial_duco(const std::string &s) { this->comm_cfg_serd_ = s; }
  void set_comm_ident_product_id(uint32_t v) { this->comm_cfg_pid_ = v; }
  void set_comm_ident_version(const std::string &s) { this->comm_cfg_ver_ = s; }

 protected:
  static uint16_t crc16_modbus(const uint8_t *d, size_t n);
  void send_frame_(uint8_t func, const std::vector<uint8_t> &data);
  void send_reply_(uint8_t func, uint8_t id, const std::vector<uint8_t> &data);  // explicit id
  void write_stuffed_(uint8_t b);
  // comm-print identity (0x20/0x22): decode replies + optionally respond as the print
  void decode_comm_reply_(const uint8_t *d, size_t n);   // store a 0x22 reply
  void parse_errlog_(const uint8_t *d, size_t n);        // 0x32 reply to 0x30 sub 01
  void publish_error_info_(uint8_t node, uint8_t a, uint8_t b, uint8_t c);
  void publish_comm_ident_();                            // -> text sensors
  void handle_comm_query_(uint8_t id, uint8_t field, uint8_t group);  // answer a 0x20
  void seed_comm_ident_();                               // build responder table from config
  void auto_fill_network_();                             // fill ip/gw/mask/host/mac from the ESP
  void enqueue_(const Request &r);
  void pump_queue_();
  void handle_frame_(const uint8_t *buf, size_t len);
  void log_decoded_(const uint8_t *buf, size_t len);   // human-readable frame description
  void parse_live_(uint8_t node, const uint8_t *d, size_t n);
  void parse_co2_(uint8_t node, const uint8_t *d, size_t n);
  void parse_type_(uint8_t node, const uint8_t *d, size_t n);
  void parse_errors_(uint8_t node, const uint8_t *d, size_t n);  // 0x32 error list
  void parse_subtype_(uint8_t node, const uint8_t *d, size_t n); // reg 0x15
  void parse_ctrs_(uint8_t node, const uint8_t *d, size_t n);    // reg 0x18
  void parse_nodever_(uint8_t node, const uint8_t *d, size_t n); // reg 0x00 (PID+version)
  void parse_prog_(uint8_t idx, const uint8_t *d, size_t n);     // 0x24 0C program read
  void publish_program_(uint8_t idx);                            // text/switch/dump
  void parse_zcount_(uint8_t idx, const uint8_t *d, size_t n);   // 0x24 13 node count
  void parse_znodes_(uint8_t idx, const uint8_t *d, size_t n);   // 0x24 13 node array
  void parse_gen_(int idx, const uint8_t *d, size_t n);
#ifdef USE_TEXT_SENSOR
  void parse_str_(int idx, const uint8_t *d, size_t n);
#endif
  void sniff_loop_();
  void sniff_flush_();
  void decode_passive_(const uint8_t *buf, size_t len);
  void record_box_time_(uint32_t t);   // store + publish a decoded box wall-clock
  void record_box_local_time_(uint32_t t);        // 0x24 sub 02 reply (local wall-clock)
  void record_box_tz_(int8_t tz, uint8_t dst);    // TimeZone/Dst from either time reply
  void node_refresh_(uint8_t node);    // comm-print's post-write refresh (0x24 06 / 0x30 03)
#ifdef USE_TIME
  void maybe_push_time_();   // read/compare/push the box clock vs the ESP time source
#endif

  std::map<uint8_t, NodeEntities> nodes_;
#ifdef USE_SENSOR
  std::vector<GenRead> gen_reads_;
  std::vector<PushSub> push_subs_;   // passive subscriptions
#endif
#ifdef USE_TEXT_SENSOR
  std::vector<StrRead> str_reads_;   // one-shot string reads
  std::map<uint8_t, text_sensor::TextSensor *> program_text_;  // idx -> summary
  text_sensor::TextSensor *programs_dump_{nullptr};
  text_sensor::TextSensor *errors_all_{nullptr};    // global active-error list (0x30 sub 01)
  text_sensor::TextSensor *errors_info_{nullptr};   // NL explanation of active error(s)
  std::map<uint8_t, text_sensor::TextSensor *> group_nodes_text_;  // group idx -> "[..]"
#endif
#ifdef USE_SWITCH
  std::map<uint8_t, switch_::Switch *> program_sw_;            // idx -> enable switch
#endif
  std::map<uint8_t, ProgramRec> programs_;   // last-read program records (idx 0..7)
  bool want_programs_{false};

  // comm-print identity handshake (0x20/0x22)
  std::map<uint16_t, std::array<uint8_t, 4>> comm_ident_;   // key = (group<<8)|field
  bool want_comm_ident_{false};   // passively decode + publish the print's identity
  bool comm_responder_{false};    // actively answer the box's 0x20/0x1C/0x08 as the print
  // Optional tag printed in every decode/RX line. Lets a two-hub sniffer (one
  // duco: per bus direction) keep the two directions apart in one log.
  std::string log_prefix_;
  // TimeZone (-11..12) and Dst (0/1) as reported by the box in every time reply.
  int8_t box_tz_{0};
  uint8_t box_dst_{0};
  bool box_tz_valid_{false};
  bool comm_seeded_{false};
  // Keys the DucoBox has written to us (0x20 sub 01). seed_comm_ident_() runs on
  // every query and must never clobber these, or the box sees its setting vanish
  // and re-asserts it forever -- which is what it did before this was added.
  std::set<uint16_t> comm_written_;
  uint8_t comm_state_{3};         // connectivity state reported in the 0x1E status block
  // responder health counters
  uint32_t cnt_queries_{0};       // 0x20 identity queries answered
  uint32_t cnt_unknown_{0};       // 0x20 for a group/field we answered "empty"
  uint32_t cnt_status_{0};        // 0x1C status polls answered
  uint32_t cnt_pushes_{0};        // 0x08 / 0x2C pushes acked
  uint32_t cnt_retrans_{0};       // same box frame seen again quickly = our reply rejected
  uint32_t cnt_enroll_{0};        // box restarted the identity handshake (group0/field0)
  uint32_t cnt_ip_shared_{0};     // IP address (identity g2/f0) sent to the box
  uint32_t cnt_mac_shared_{0};    // MAC address (identity g2/f2) sent to the box
  // print-initiated heartbeat (real print sends these ~every 64 s, fixed ids):
  //   0x20 id=0xAA `01 00 <group>` identity-announce  +  0x1C id=0x99 `<status>`
  uint32_t last_announce_ms_{0};
  uint8_t announce_grp_{0};
  uint32_t modbus_addr_{0};       // Modbus address the box pushes (0x20 write g0/f0)
  bool g0f0_seen_{false};
  uint16_t last_box_key_{0xFFFF};
  uint32_t last_box_ms_{0};
  std::array<uint8_t, 3> last_box_head_{{0, 0, 0}};
#ifdef USE_SENSOR
  std::map<std::string, sensor::Sensor *> diag_sensors_;
#endif
  void publish_diag_();
#ifdef USE_TEXT_SENSOR
  std::map<std::string, text_sensor::TextSensor *> comm_text_;
#endif
  std::string comm_cfg_ip_, comm_cfg_mask_, comm_cfg_gw_, comm_cfg_mac_, comm_cfg_host_;
  std::string comm_cfg_serb_, comm_cfg_serd_, comm_cfg_ver_;
  uint32_t comm_cfg_pid_{0};
  std::deque<Request> queue_;

  // RX framing
  enum { WAIT_AA, WAIT_55, BODY } rx_state_{WAIT_AA};
  std::vector<uint8_t> rx_;
  uint32_t rx_last_ms_{0};

  // in-flight request
  bool in_flight_{false};
  uint8_t inflight_id_{0};
  Request inflight_{};
  uint32_t inflight_ms_{0};

  uint8_t next_id_{1};
  uint8_t scan_max_{0};   // if >0, also poll node numbers 1..scan_max_ for discovery
  bool typed_once_{false};

  // debug sniffer
  bool sniffer_{false};
  std::vector<uint8_t> sniff_;
  uint32_t sniff_last_ms_{0};
  uint32_t sniff_probe_ms_{0};

  // passive decode (listen-only)
  bool passive_{false};

  // debug: raw TX/RX byte logging while running normally
  bool debug_raw_{false};
  // debug: log a decoded description of each received frame
  bool decode_log_{false};

  // box wall-clock: last Unix time seen (0x26 report / 0x24 sub 05 set)
  uint32_t last_box_time_{0};
  uint32_t last_box_time_ms_{0};   // millis() when last_box_time_ was captured
#ifdef USE_TEXT_SENSOR
  text_sensor::TextSensor *time_text_{nullptr};
  text_sensor::TextSensor *time_local_text_{nullptr};
  // TimeZone as a readable offset ("UTC+1"). A number entity cannot carry a
  // dynamic sign, so the human-readable form lives in its own text sensor.
  text_sensor::TextSensor *tz_text_{nullptr};
  std::vector<BoxStrRead> box_strings_;
#ifdef USE_NUMBER
  std::map<uint16_t, number::Number *> bus_numbers_;
  void publish_bus_number_(uint16_t bus_id, float v);
#endif
#ifdef USE_SELECT
  std::vector<BoxSelect> box_selects_;
  size_t bsel_idx_{0};
  void parse_boxsel_(int idx, const uint8_t *d, size_t n);
#endif
#if defined(USE_NUMBER) || defined(USE_SELECT)
  std::vector<NodeParam> node_params_;
  size_t nparam_idx_{0};        // welke parameter we deze ronde teruglezen
  void parse_nparam_(int idx, const uint8_t *d, size_t n);
#endif
  void parse_box_str_(const uint8_t *d, size_t n);
  text_sensor::TextSensor *install_text_{nullptr};
#endif
#ifdef USE_TIME
  time::RealTimeClock *rtc_{nullptr};
  uint8_t time_threshold_{2};       // max allowed drift (s) before we push
  uint32_t last_time_push_ms_{0};
  uint32_t last_time_read_ms_{0};   // when we last actively requested the box time
  bool time_bootstrap_read_{false}; // issued the initial box-time read; next round bootstraps
#endif
};

#ifdef USE_SELECT
// Ventilation-mode control (writes FUNC 0x0C, DATA 04 01 <mode>). Experimental.
class DucoModeSelect : public select::Select, public Parented<DucoComponent> {
 public:
  void set_node(uint8_t node) { this->node_ = node; }
 protected:
  void control(const std::string &value) override {
    this->parent_->set_mode_str(value, this->node_);
    this->publish_state(value);
  }
  uint8_t node_{1};
};

// Een box-parameter met benoemde waarden. Schrijft met 0x24 01; de positie in
// de optielijst IS de buswaarde. Zo werken ook de enum-acties uit /action.
class DucoBoxSelect : public select::Select, public Parented<DucoComponent> {
 public:
  void set_bus_id(uint16_t id) { this->id_ = id; }
 protected:
  void control(const std::string &value) override {
    auto idx = this->index_of(value);
    if (!idx.has_value()) return;
    this->parent_->write_box_param(this->id_, (uint32_t) *idx);
    this->publish_state(value);
  }
  uint16_t id_{0};
};

// Een node-parameter met benoemde waarden (service 6). De positie in de
// optielijst IS de waarde die over de bus gaat -- de enums beginnen bij 0, dus
// index 0 = waarde 0. De namen horen bij de Duco Network Tool.
class DucoParamSelect : public select::Select, public Parented<DucoComponent> {
 public:
  void set_param(uint8_t node, uint8_t reg) { this->node_ = node; this->reg_ = reg; }
 protected:
  void control(const std::string &value) override {
    auto idx = this->index_of(value);
    if (!idx.has_value()) return;
    this->parent_->write_node_param18(this->node_, this->reg_, (uint16_t) *idx);
    this->publish_state(value);
  }
  uint8_t node_{1};
  uint8_t reg_{0};
};
#endif

#ifdef USE_NUMBER
// Writable node setpoint. kind 0 = Co2SetPoint (0x18 reg 0x03, u16),
//                         kind 1 = TempOffset  (0x10 reg 0x52, i16 signed).
class DucoNumber : public number::Number, public Parented<DucoComponent> {
 public:
  void set_target(uint8_t kind, uint8_t node) { this->kind_ = kind; this->node_ = node; }
  // node-configuratieparameter via service 6; het register gaat in bus_id_
  void set_node_param_target(uint8_t reg, uint8_t node) {
    this->kind_ = 30; this->bus_id_ = reg; this->node_ = node;
  }
  // generic box-register write (0x24 01): kind 20, value scaled back to the raw u32.
  void set_bus_target(uint16_t id, float scale) {
    this->kind_ = 20; this->bus_id_ = id; this->scale_ = (scale == 0.0f) ? 1.0f : scale;
  }
 protected:
  void control(float value) override {
    switch (this->kind_) {
      case 0: this->parent_->write_node_param18(this->node_, 0x03, (uint16_t) lroundf(value)); break;
      // kind 30 = generieke node-parameter; het register staat in bus_id_
      case 30: this->parent_->write_node_param18(this->node_, (uint8_t) this->bus_id_,
                                                 (uint16_t) lroundf(value)); break;
      case 1: this->parent_->write_node_param10(this->node_, 0x52, (int16_t) value); break;
      case 2: this->parent_->set_node_parent(this->node_, (uint8_t) value); break;
      case 3: this->parent_->set_node_asso(this->node_, (uint8_t) value); break;
      // zone/circuit writes (node_ carries the circuit index)
      case 10: this->parent_->write_zone_param(this->node_, 0x00, 0x05, (uint32_t)(int32_t) value); break;
      case 11: this->parent_->write_zone_param(this->node_, 0x01, 0x06, (uint32_t) value); break;
      // generic box register (0x24 01 <reg> <mod> <u32>)
      // lroundf, not "+0.5f": the latter rounds -10 to -9, which is exactly the
      // kind of value TimeZone (-11..12) takes. Verified against the real print,
      // which writes TimeZone to box-param 0x0802 and Dst to 0x0806.
      case 20: this->parent_->write_box_param(
                   this->bus_id_, (uint32_t)(int32_t) lroundf(value / this->scale_)); break;
      default: break;
    }
    this->publish_state(value);
  }
  uint8_t kind_{0};
  uint8_t node_{0};
  uint16_t bus_id_{0};
  float scale_{1.0f};
};
#endif

#ifdef USE_SWITCH
// Enable/disable a weekly program (index 0..7). Preserves the schedule; flips Enable.
class DucoProgramSwitch : public switch_::Switch, public Parented<DucoComponent> {
 public:
  void set_index(uint8_t idx) { this->idx_ = idx; }
 protected:
  void write_state(bool state) override {
    this->parent_->set_program_enable(this->idx_, state);
    this->publish_state(state);
  }
  uint8_t idx_{0};
};

// Generic 0/1 box register (0x24 01 <reg> <mod> <0|1>). Optimistic state.
class DucoBoxRegSwitch : public switch_::Switch, public Parented<DucoComponent> {
 public:
  void set_bus_id(uint16_t id) { this->bus_id_ = id; }
 protected:
  void write_state(bool state) override {
    this->parent_->write_box_param(this->bus_id_, state ? 1 : 0);
    this->publish_state(state);
  }
  uint16_t bus_id_{0};
};
#endif

#ifdef USE_BUTTON
// Action button. action: 0=filter reset, 1=install mode on, 2=install mode off,
//                        3=identify on, 4=identify off (node-scoped).
class DucoActionButton : public button::Button, public Parented<DucoComponent> {
 public:
  void set_action(uint8_t action, uint8_t node) { this->action_ = action; this->node_ = node; }
 protected:
  void press_action() override {
    switch (this->action_) {
      case 0: this->parent_->reset_filter(); break;
      case 1: this->parent_->set_install_mode(true); break;
      case 2: this->parent_->set_install_mode(false); break;
      case 3: this->parent_->set_identify(this->node_, true); break;
      case 4: this->parent_->set_identify(this->node_, false); break;
      case 5: this->parent_->node_reboot(this->node_); break;
      case 6: this->parent_->node_reset_config(this->node_); break;
      case 7: this->parent_->set_link_mode(this->node_, true); break;
      case 8: this->parent_->set_link_mode(this->node_, false); break;
      default: break;
    }
  }
  uint8_t action_{0};
  uint8_t node_{0};
};
#endif

}  // namespace duco
}  // namespace esphome
