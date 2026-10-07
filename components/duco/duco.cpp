// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 esphome-ventilation-connect contributors
//
// Deze component is een afgeleid werk van ESPHome, waarvan de C++/runtime onder
// GPLv3 staat. Zie LICENSE.
#include "duco.h"
#include "duco_error_info.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"
#include "esphome/core/helpers.h"
#include <ctime>
#ifdef USE_NETWORK
#include "esphome/components/network/util.h"
#endif

namespace esphome {
namespace duco {

static const char *const TAG = "duco";

// ----- constants ---------------------------------------------------------

// Ventilation states.
//
// Wire-encoding of the set-mode frame, observed on the bus. The frame is
// `0x0C 04 <node> <value>`, and the value packs TWO fields:
//
//     value = (press_count << 6) | base_mode
//
// base_mode: 0 AUTO, 1..3 AUT1..AUT3, 4..6 MAN1..MAN3, 7 EMPT, 8..10 CNT1..CNT3
// press_count: 0 = normal, 2 = "x2", 3 = "x3"
//
// MAN3x3 therefore does NOT travel as 16: the firmware turns the API value 16
// into base 6 with count 3, i.e. (3 << 6) | 6 = 0xC6. Sending 16 as-is is why
// the box ignored it -- it rejects anything above 16, and 11..16 are API-level
// names that never appear on the wire.
//
// Same "press count" the RF protocol carries (01=1x, 10=2x, 11=3x); the wired
// bus packs it into the top bits of the mode byte.
static uint8_t mode_pack(uint8_t base, uint8_t count) {
  return (uint8_t)((count << 6) | (base & 0x3F));
}

const char *mode_to_string(uint8_t m) {
  static char buf[12];
  const uint8_t count = m >> 6;
  const uint8_t base = m & 0x3F;
  const char *name;
  switch (base) {
    case 0: name = "AUTO"; break;
    case 1: name = "AUT1"; break;   case 2: name = "AUT2"; break;
    case 3: name = "AUT3"; break;
    case 4: name = "MAN1"; break;   case 5: name = "MAN2"; break;
    case 6: name = "MAN3"; break;
    case 7: name = "EMPT"; break;
    case 8: name = "CNT1"; break;   case 9: name = "CNT2"; break;
    case 10: name = "CNT3"; break;
    // API-level values, should the box ever report them unpacked
    case 11: return "MAN1x2";  case 12: return "MAN2x2";  case 13: return "MAN3x2";
    case 14: return "MAN1x3";  case 15: return "MAN2x3";  case 16: return "MAN3x3";
    default: return "UNKNOWN";
  }
  if (count == 0) return name;
  snprintf(buf, sizeof(buf), "%sx%u", name, (unsigned) count);
  return buf;
}

uint8_t string_to_mode(const std::string &s) {
  static const struct { const char *n; uint8_t base; uint8_t count; } kModes[] = {
    {"AUTO", 0, 0},  {"AUT1", 1, 0},  {"AUT2", 2, 0},  {"AUT3", 3, 0},
    {"MAN1", 4, 0},  {"MAN2", 5, 0},  {"MAN3", 6, 0},  {"EMPT", 7, 0},
    {"CNT1", 8, 0},  {"CNT2", 9, 0},  {"CNT3", 10, 0},
    {"MAN1x2", 4, 2}, {"MAN2x2", 5, 2}, {"MAN3x2", 6, 2},
    {"MAN1x3", 4, 3}, {"MAN2x3", 5, 3}, {"MAN3x3", 6, 3},
    // legacy names from before the parameter tables were mapped
    {"AWAY", 7, 0}, {"PERM1", 8, 0}, {"PERM2", 9, 0}, {"PERM3", 10, 0},
  };
  for (auto &e : kModes)
    if (s == e.n) return mode_pack(e.base, e.count);
  return 0;
}


const char *node_type_to_string(uint8_t t) {
  switch (t) {
    case 7: return "Window ventilator"; case 8: return "UC RF/BAT"; case 9: return "UC RF/Wired";
    case 10: return "RH room sensor";   case 12: return "CO2 room sensor";
    case 13: return "Valve";            case 14: return "RH valve"; case 16: return "CO2 valve";
    case 17: return "DucoBox";          case 18: return "Switch";
    case 22: return "iAV valve";        case 23: return "iAV RH";   case 25: return "iAV CO2";
    case 27: return "Control unit";     case 28: return "CO2/RH valve";
    case 29: return "UC Sun Control";   case 30: return "UC Ventilative Cooling";
    case 31: return "Ext multi-zone valve";
    case 35: return "RH box sensor";    case 37: return "CO2 box sensor";
    case 38: return "Motor relay";      case 39: return "Weather station";
    case 40: return "Modbus motor";     case 41: return "Digital input"; case 42: return "Digital output";
    case 44: return "Modbus relay";     case 45: return "Perilex";       case 46: return "Relay output";
    default: return "Unknown";
  }
}

// A box wall-clock frame has a distinctive shape, unlike a normal 0x26 parameter
// reply (`01 <reg> <module> <val:4>`, 7 bytes). Require the exact signature so a
// parameter whose value happens to exceed 1.6e9 (serials, resistances, ...) is not
// mistaken for a timestamp.
static bool extract_box_time_(uint8_t func, const uint8_t *d, size_t dlen, uint32_t &t,
                             int8_t *tz = nullptr, uint8_t *dst = nullptr) {
  // report: 0x26  01 <unixtime:4> 00 00 00 00 <TimeZone i8> <Dst u8>   (11 bytes)
  // The last two bytes are NOT constants -- they carry TimeZone (-11..12) and
  // Dst (0/1). Requiring them to be 01 01 silently dropped every reply from a
  // box with any other timezone (proven 19-08 with tz=-10).
  if (func == 0x26 && dlen >= 11 && d[0] == 0x01 &&
      d[5] == 0 && d[6] == 0 && d[7] == 0 && d[8] == 0) {
    t = d[1] | (d[2] << 8) | (d[3] << 16) | ((uint32_t) d[4] << 24);
    if (tz != nullptr) *tz = (int8_t) d[9];
    if (dst != nullptr) *dst = d[10];
    return t > 1600000000UL;
  }
  // set:    0x24  05 <unixtime:4> 00 00 00 00           (9 bytes)
  if (func == 0x24 && dlen >= 9 && d[0] == 0x05 &&
      d[5] == 0 && d[6] == 0 && d[7] == 0 && d[8] == 0) {
    t = d[1] | (d[2] << 8) | (d[3] << 16) | ((uint32_t) d[4] << 24);
    return t > 1600000000UL;
  }
  return false;
}

// ----- CRC / framing -----------------------------------------------------

uint16_t DucoComponent::crc16_modbus(const uint8_t *d, size_t n) {
  uint16_t c = 0xFFFF;
  for (size_t i = 0; i < n; i++) {
    c ^= d[i];
    for (int b = 0; b < 8; b++) c = (c & 1) ? (c >> 1) ^ 0xA001 : (c >> 1);
  }
  return c;
}

void DucoComponent::write_stuffed_(uint8_t b) {
  this->write_byte(b);
  if (b == 0xAA) this->write_byte(0x01);  // byte-stuffing
}

// Send a frame with an explicit id (used to answer box-initiated queries as the print).
void DucoComponent::send_reply_(uint8_t func, uint8_t id, const std::vector<uint8_t> &data) {
  std::vector<uint8_t> msg;
  msg.push_back((uint8_t)(2 + data.size()));
  msg.push_back(func);
  msg.push_back(id);
  for (auto b : data) msg.push_back(b);
  uint16_t crc = crc16_modbus(msg.data(), msg.size());
  std::vector<uint8_t> wire{0xAA, 0x55};
  auto stuff = [&](uint8_t b) { wire.push_back(b); if (b == 0xAA) wire.push_back(0x01); };
  for (auto b : msg) stuff(b);
  stuff(crc & 0xFF);
  stuff((crc >> 8) & 0xFF);
  for (auto b : wire) this->write_byte(b);
  this->flush();
  if (this->sniffer_ || this->debug_raw_) {
    char hex[3 * 48 + 1];
    size_t nn = wire.size() > 48 ? 48 : wire.size(), hp = 0;
    for (size_t i = 0; i < nn; i++) hp += snprintf(hex + hp, sizeof(hex) - hp, "%02X ", wire[i]);
    ESP_LOGI(TAG, "TX*  [%2u] %s (reply func=0x%02X id=%u)", (unsigned) wire.size(), hex, func, id);
  }
}

void DucoComponent::send_frame_(uint8_t func, const std::vector<uint8_t> &data) {
  std::vector<uint8_t> msg;                 // LEN FUNC ID DATA (CRC input)
  msg.push_back((uint8_t)(2 + data.size()));  // LEN = func+id+data
  msg.push_back(func);
  msg.push_back(this->next_id_);
  for (auto b : data) msg.push_back(b);
  uint16_t crc = crc16_modbus(msg.data(), msg.size());

  // build the exact on-wire byte stream (start bytes + byte-stuffing)
  std::vector<uint8_t> wire;
  wire.push_back(0xAA);
  wire.push_back(0x55);
  auto stuff = [&](uint8_t b) { wire.push_back(b); if (b == 0xAA) wire.push_back(0x01); };
  for (auto b : msg) stuff(b);
  stuff(crc & 0xFF);
  stuff((crc >> 8) & 0xFF);

  for (auto b : wire) this->write_byte(b);
  this->flush();

  if (this->sniffer_ || this->debug_raw_) {
    char hex[3 * 48 + 1];
    size_t n = wire.size() > 48 ? 48 : wire.size();
    size_t hp = 0;
    for (size_t i = 0; i < n; i++) hp += snprintf(hex + hp, sizeof(hex) - hp, "%02X ", wire[i]);
    ESP_LOGI(TAG, "TX   [%2u] %s", (unsigned) wire.size(), hex);
  } else {
    ESP_LOGV(TAG, "TX func=0x%02X id=%u len=%u", func, this->next_id_, (unsigned) data.size());
  }
}

// ----- lifecycle ---------------------------------------------------------

void DucoComponent::setup() { ESP_LOGCONFIG(TAG, "DucoBox UART component starting"); }

NodeEntities &DucoComponent::node_entities(uint8_t node) {
  auto &e = this->nodes_[node];
  e.node = node;
  return e;
}

void DucoComponent::enqueue_(const Request &r) { this->queue_.push_back(r); }

#ifdef USE_SENSOR
void DucoComponent::set_node_sensor(uint8_t node, const std::string &field, sensor::Sensor *s) {
  NodeEntities &e = this->node_entities(node);
  if (field == "mode") e.mode = s;
  else if (field == "flow") e.flow = s;
  else if (field == "time_remaining") e.time_remaining = s;
  else if (field == "co2") { e.co2 = s; e.want_co2 = true; }
  else if (field == "temp") { e.temp = s; e.want_co2 = true; }
  // 0x49-blok u16[3] (offset 8). Heette 'iaq_co2', maar dat was fout: de waarde
  // is de UPTIME van de node in minuten. Bewezen 24-08 -- node 4 meldde op de bus
  // 386 terwijl de API IaqCo2=100 en UpTime=393 gaf, en node 1 (de box) gaf 18 op
  // het moment dat /info/nodes/1 UpTime ook 18 was. De oude naam blijft werken.
  else if (field == "uptime" || field == "iaq_co2") { e.uptime = s; e.want_co2 = true; }
  else if (field == "rssi") e.rssi = s;
  else if (field == "comm_error_ctr") e.comm_error_ctr = s;
  else if (field == "rssi_n2m") e.rssi_n2m = s;
  else if (field == "hop_rf") e.hop_rf = s;
  else if (field == "rssi_n2h") e.rssi_n2h = s;
  else if (field == "flow_target") e.flow_target = s;
  else if (field == "flow_ovrl") e.flow_ovrl = s;
  else if (field == "network_type") { e.network_type = s; e.want_type = true; }
  else if (field == "addr") { e.addr = s; e.want_type = true; }
  else if (field == "sub_addr") { e.sub_addr = s; e.want_type = true; }
  else if (field == "parent") { e.parent = s; e.want_type = true; }
  else if (field == "asso") { e.asso = s; e.want_type = true; }
  else if (field == "subtype") { e.subtype = s; e.want_subtype = true; }
  else if (field == "week_ctr") { e.week_ctr = s; e.want_ctrs = true; }
  else if (field == "boot_ctr") { e.boot_ctr = s; e.want_ctrs = true; }
  else if (field == "product_id") { e.product_id = s; e.want_nodever = true; }
}
#endif
#ifdef USE_TEXT_SENSOR
void DucoComponent::set_node_text_sensor(uint8_t node, const std::string &field,
                                         text_sensor::TextSensor *s) {
  NodeEntities &e = this->node_entities(node);
  if (field == "mode") e.mode_text = s;
  else if (field == "type") { e.type_text = s; e.want_type = true; }
  else if (field == "active_error") { e.active_error = s; e.want_errors = true; }
  else if (field == "sw_version") { e.sw_version = s; e.want_nodever = true; }
}
#endif

#ifdef USE_SENSOR
static uint8_t vtype_size(uint8_t vt) {
  switch (vt) { case VT_ENUM8: case VT_U8: return 1; case VT_U32: return 4; default: return 2; }
}
static bool vtype_signed(uint8_t vt) { return vt == VT_I16 || vt == VT_MEAS; }

bool DucoComponent::add_param_sensor(uint8_t node, const std::string &name, float scale,
                                     sensor::Sensor *s) {
  // Eerst de GEVERIFIEERDE tabel. duco_params.h is een generieke parameterlijst en
  // klopt voor deze box niet bij module 0x00, 0x03, 0x0A en 0x13 -- daar zou een
  // naamzoektocht het verkeerde register opleveren.
  const DucoVerifiedDef *v = duco_find_verified(name.c_str());
  if (v != nullptr) {
    float sc = (scale != 0.0f) ? scale : v->scale;
    std::vector<uint8_t> req = {0x00, (uint8_t)(v->id & 0xFF), (uint8_t)(v->id >> 8)};
    GenRead g{0x24, req, /*offset*/ 3, 4, v->sgn, sc, node, s};
    this->gen_reads_.push_back(g);
    ESP_LOGCONFIG(TAG, "param '%s' -> 0x%04X (geverifieerd) scale=%.3f", name.c_str(), v->id, sc);
    return true;
  }
  const DucoParamDef *p = duco_find_param(name.c_str());
  if (p == nullptr) { ESP_LOGE(TAG, "unknown parameter '%s'", name.c_str()); return false; }
  float sc = (scale != 0.0f) ? scale : p->scale;
  // 0x24 read: DATA = 00 <id_lo> <id_hi>; reply 0x26 DATA = 01 <id_lo> <id_hi> <val:4>
  std::vector<uint8_t> req = {0x00, (uint8_t)(p->id & 0xFF), (uint8_t)(p->id >> 8)};
  GenRead g{0x24, req, /*offset*/ 3, vtype_size(p->vtype), vtype_signed(p->vtype), sc, node, s};
  this->gen_reads_.push_back(g);
  ESP_LOGCONFIG(TAG, "param '%s' id=0x%04X vt=%u size=%u scale=%.3f (node %u)",
                name.c_str(), p->id, p->vtype, g.size, sc, node);
  return true;
}

void DucoComponent::add_raw_sensor(uint8_t node, uint8_t func, const std::vector<uint8_t> &req,
                                   uint8_t offset, uint8_t size, bool sgn, float scale,
                                   sensor::Sensor *s) {
  GenRead g{func, req, offset, size, sgn, scale == 0.0f ? 1.0f : scale, node, s};
  this->gen_reads_.push_back(g);
}
#endif

void DucoComponent::update() {
  if (this->sniffer_ || this->passive_) return;   // listen-only modes never poll
#ifdef USE_TEXT_SENSOR
  // Refresh the identity-derived text sensors once per round: as the comm print
  // ourselves we hold the table, so nothing else would trigger a publish.
  if (this->want_comm_ident_ && this->comm_responder_) this->publish_comm_ident_();
  // The UTC read (sub 04) is driven by the clock-sync logic and only runs when the
  // box time is unknown or stale. The LOCAL read (sub 02) has no such trigger, so
  // poll it here or the entity would stay empty in normal operation.
  if (this->time_local_text_ != nullptr)
    this->enqueue_({REQ_READTIME_LOCAL, 0x24, 1, {0x02}});
#endif
  // one poll round: type discovery (once), then live data + CO2 for each node
  if (!this->typed_once_) {
    for (auto &kv : this->nodes_)
      if (kv.second.want_type) this->enqueue_({REQ_TYPE, 0x0C, kv.first, {0x01, kv.first}});
    // optional wider discovery scan
    for (uint8_t n = 1; n <= this->scan_max_; n++)
      this->enqueue_({REQ_TYPE, 0x0C, n, {0x01, n}});
    this->typed_once_ = true;
  }
  for (auto &kv : this->nodes_) {
    uint8_t n = kv.first;
    this->enqueue_({REQ_LIVE, 0x0C, n, {0x02, n}});
    if (kv.second.want_co2)
      this->enqueue_({REQ_CO2, 0x10, n, {0x01, n, 0x00, 0x49, 0x04}});
    if (kv.second.want_errors)
      this->enqueue_({REQ_ERRORS, 0x30, n, {0x03, n}});   // 0x30 03 <node> -> 0x32 error list
    if (kv.second.want_subtype)
      this->enqueue_({REQ_SUBTYPE, 0x10, n, {0x01, n, 0x00, 0x15, 0x01}});  // reg 0x15 (SubType)
    if (kv.second.want_ctrs)
      this->enqueue_({REQ_CTRS, 0x10, n, {0x01, n, 0x00, 0x18, 0x02}});     // reg 0x18 (Week/Boot)
    if (kv.second.want_nodever && !kv.second.nodever_done)
      this->enqueue_({REQ_NODEVER, 0x10, n, {0x01, n, 0x00, 0x00, 0x04}});  // reg 0x00 (PID+ver)
  }
#ifdef USE_SENSOR
  for (size_t i = 0; i < this->gen_reads_.size(); i++) {
    GenRead &g = this->gen_reads_[i];
    Request r{REQ_GEN, g.func, g.node, g.data, (int) i};
    this->enqueue_(r);
  }
#endif
#ifdef USE_SELECT
  // Box-keuzelijsten teruglezen, ook een per ronde.
  if (!this->box_selects_.empty()) {
    if (this->bsel_idx_ >= this->box_selects_.size()) this->bsel_idx_ = 0;
    const BoxSelect &bs = this->box_selects_[this->bsel_idx_];
    this->enqueue_({REQ_BOXSEL, 0x24, 1,
                    {0x00, (uint8_t)(bs.id & 0xFF), (uint8_t)(bs.id >> 8)},
                    (int) this->bsel_idx_});
    this->bsel_idx_++;
  }
#endif
#if defined(USE_NUMBER) || defined(USE_SELECT)
  // Node-parameters teruglezen -- maar EEN per ronde. Ze veranderen alleen als
  // iemand ze wijzigt, en de bus is al druk genoeg met live-waarden; alles elke
  // ronde opvragen zou de poll onnodig verlengen.
  if (!this->node_params_.empty()) {
    if (this->nparam_idx_ >= this->node_params_.size()) this->nparam_idx_ = 0;
    const NodeParam &np = this->node_params_[this->nparam_idx_];
    this->enqueue_({REQ_NPARAM, 0x18, np.node, {0x00, np.node, np.reg},
                    (int) this->nparam_idx_});
    this->nparam_idx_++;
  }
#endif
#ifdef USE_TEXT_SENSOR
  // strings spread over consecutive box params (serial, WiFi-AP key): ask for
  // every word until the whole run has come in, then stop.
  for (auto &bs : this->box_strings_) {
    if (bs.done) continue;
    for (uint8_t w = 0; w < bs.words; w++) {
      if (bs.got & (1u << w)) continue;
      uint16_t id = bs.first + w;
      this->enqueue_({REQ_BOXSTR, 0x24, 1, {0x00, (uint8_t)(id & 0xFF), (uint8_t)(id >> 8)}});
    }
  }
  // one-shot string reads: retry every round until each has succeeded once
  for (size_t i = 0; i < this->str_reads_.size(); i++) {
    StrRead &sr = this->str_reads_[i];
    if (sr.done) continue;
    Request r{REQ_STR, sr.func, sr.node, sr.data, (int) i};
    this->enqueue_(r);
  }
#endif
  // weekly programs: read the slots we have an entity for (or all 8 for a dump)
  if (this->want_programs_) {
    bool all = false;
#ifdef USE_TEXT_SENSOR
    all = (this->programs_dump_ != nullptr);
#endif
    for (uint8_t idx = 0; idx < 8; idx++) {
      bool want = all;
#ifdef USE_TEXT_SENSOR
      if (this->program_text_.count(idx)) want = true;
#endif
#ifdef USE_SWITCH
      if (this->program_sw_.count(idx)) want = true;
#endif
      if (want) this->enqueue_({REQ_PROG, 0x24, idx, {0x0C, idx}});
    }
  }
#ifdef USE_TEXT_SENSOR
  // group node lists: read the count (0x24 13 <idx> 00 00 01), then the array
  for (auto &kv : this->group_nodes_text_)
    this->enqueue_({REQ_ZCOUNT, 0x24, kv.first, {0x13, kv.first, 0x00, 0x00, 0x01}});
  // global error-log: 0x30 sub 01 -> all active errors in one reply
  if (this->errors_all_ != nullptr || this->errors_info_ != nullptr)
    this->enqueue_({REQ_ERRLOG, 0x30, 0, {0x01}});
#endif
#ifdef USE_TIME
  this->maybe_push_time_();
#endif
  // Print-initiated heartbeat, mirrored from the real comm print (~every 64 s,
  // fixed ids): identity-announce `0x20 id=0xAA 01 00 <group>` + status `0x1C
  // id=0x99 00`. After a group-2 announce the box re-reads IP/GW/MAC and shows
  // them under LAN-INSTELLINGEN on the display.
  if (this->comm_responder_) {
    const uint32_t now = millis();
    if (now - this->last_announce_ms_ >= 60000UL || this->last_announce_ms_ == 0) {
      this->last_announce_ms_ = now;
      static const uint8_t GRPS[] = {0x00, 0x02, 0x05, 0x01, 0x03, 0x04, 0x06};
      const uint8_t grp = GRPS[this->announce_grp_ % sizeof(GRPS)];
      this->announce_grp_++;
      this->seed_comm_ident_();
      this->send_reply_(0x20, 0xAA, {0x01, 0x00, grp});
      this->send_reply_(0x1C, 0x99, {0x00});
      ESP_LOGD(TAG, "print heartbeat: identity-announce group %u + status", grp);
    }
  }
  this->publish_diag_();   // responder health counters (if configured)
}

#ifdef USE_TIME
void DucoComponent::maybe_push_time_() {
  if (this->rtc_ == nullptr) return;
  auto now = this->rtc_->now();
  if (!now.is_valid()) return;                 // only act when ESP has a valid clock
  const uint32_t now_ms = millis();
  const uint32_t esp_t = (uint32_t) now.timestamp;

  if (this->last_box_time_ == 0) {
    // The box clock is unknown (fresh box, or the ESP has replaced the comm print and
    // nothing has set it yet). Ask for it ONE poll round; if it still hasn't reported a
    // valid time by the next round, BOOTSTRAP by pushing our clock unconditionally.
    // (Attempt-based, not time-based, so it works regardless of update_interval.)
    if (!this->time_bootstrap_read_) {
      this->time_bootstrap_read_ = true;
      this->enqueue_({REQ_READTIME, 0x24, 1, {0x04}});          // UTC
      this->enqueue_({REQ_READTIME_LOCAL, 0x24, 1, {0x02}});    // local wall clock
      ESP_LOGD(TAG, "box clock unknown -> requesting it (0x24 04)");
      return;   // this round reads; next round bootstraps if still unknown
    }
    ESP_LOGI(TAG, "box clock still unknown -> bootstrapping it");
    // fall through to the push below
  } else {
    // Known reference. If it went stale (> 2 min), re-read; otherwise push only on drift.
    if ((now_ms - this->last_box_time_ms_) > 120000) {
      if (this->last_time_read_ms_ == 0 || (now_ms - this->last_time_read_ms_) > 10000) {
        this->last_time_read_ms_ = now_ms;
        this->enqueue_({REQ_READTIME, 0x24, 1, {0x04}});          // UTC
        this->enqueue_({REQ_READTIME_LOCAL, 0x24, 1, {0x02}});    // local wall clock
      }
      return;
    }
    const uint32_t est_box = this->last_box_time_ + (now_ms - this->last_box_time_ms_) / 1000;
    int32_t diff = (int32_t) (esp_t - est_box);
    if (diff < 0) diff = -diff;
    if ((uint32_t) diff <= this->time_threshold_) return;   // within tolerance -> do nothing
  }
  if (this->last_time_push_ms_ != 0 && (now_ms - this->last_time_push_ms_) < 30000) return;
  this->last_time_push_ms_ = now_ms;
  std::vector<uint8_t> data = {0x05,
                               (uint8_t) (esp_t & 0xFF), (uint8_t) ((esp_t >> 8) & 0xFF),
                               (uint8_t) ((esp_t >> 16) & 0xFF), (uint8_t) ((esp_t >> 24) & 0xFF),
                               0, 0, 0, 0};
  this->enqueue_({REQ_SETTIME, 0x24, 1, data});
  ESP_LOGI(TAG, "pushing ESP time %u to box", (unsigned) esp_t);
  this->record_box_time_(esp_t);   // assume applied; avoids an immediate re-push
}
#endif

void DucoComponent::pump_queue_() {
  if (this->in_flight_ || this->queue_.empty()) return;
  this->inflight_ = this->queue_.front();
  this->queue_.pop_front();
  this->inflight_id_ = this->next_id_;
  this->send_frame_(this->inflight_.func, this->inflight_.data);
  this->next_id_++;
  this->in_flight_ = true;
  this->inflight_ms_ = millis();
}

void DucoComponent::sniff_loop_() {
  const uint32_t now = millis();

  // dump anything received, raw -- even bytes that don't parse as a frame
  while (this->available()) {
    uint8_t b;
    this->read_byte(&b);
    this->sniff_.push_back(b);
    this->sniff_last_ms_ = now;
    if (this->sniff_.size() >= 32) this->sniff_flush_();
  }
  // flush a partial frame once the line has been idle for a bit (frame boundary)
  if (!this->sniff_.empty() && (now - this->sniff_last_ms_) > 15) this->sniff_flush_();

  // actively probe once a second: the box is a slave and only answers when polled.
  // (read node 1 = the DucoBox itself, func 0x0C sub 0x01)
  if (now - this->sniff_probe_ms_ > 1000) {
    this->sniff_probe_ms_ = now;
    this->send_frame_(0x0C, {0x01, 0x01});
    this->next_id_++;
  }
}

void DucoComponent::sniff_flush_() {
  char hex[3 * 40 + 1];
  char asc[40 + 1];
  size_t n = this->sniff_.size();
  if (n > 40) n = 40;
  size_t hp = 0;
  for (size_t i = 0; i < n; i++) {
    hp += snprintf(hex + hp, sizeof(hex) - hp, "%02X ", this->sniff_[i]);
    uint8_t c = this->sniff_[i];
    asc[i] = (c >= 0x20 && c < 0x7F) ? (char) c : '.';
  }
  asc[n] = '\0';
  ESP_LOGI(TAG, "RX %s [%2u] %s| %s", this->log_prefix_.c_str(), (unsigned) this->sniff_.size(),
           hex, asc);
  this->sniff_.clear();
}

void DucoComponent::loop() {
  if (this->sniffer_) { this->sniff_loop_(); return; }  // debug: passive listen only

  const uint32_t now = millis();

  // read UART, reassemble frames
  while (this->available()) {
    uint8_t b;
    this->read_byte(&b);
    this->rx_last_ms_ = now;
    if (this->debug_raw_) {           // dump raw RX bytes alongside normal parsing
      this->sniff_.push_back(b);
      if (this->sniff_.size() >= 32) this->sniff_flush_();
    }
    switch (this->rx_state_) {
      case WAIT_AA: if (b == 0xAA) this->rx_state_ = WAIT_55; break;
      case WAIT_55:
        if (b == 0x55) { this->rx_state_ = BODY; this->rx_.clear(); this->rx_esc_ = false; }
        else if (b != 0xAA) this->rx_state_ = WAIT_AA;
        break;
      case BODY:
        // unstuff: the box sends a literal 0xAA as `AA 01`. Track that with a flag,
        // not with rx_.back(): after the filler back() is still 0xAA, so a real 0x01
        // right behind it (e.g. id 0xAA + data `01 ..`) used to be dropped as well.
        if (this->rx_esc_) {
          this->rx_esc_ = false;
          if (b == 0x01) break;
          if (b == 0x55) { this->rx_.clear(); break; }   // `AA 55`: new frame, last one was cut short
        }
        if (b == 0xAA) this->rx_esc_ = true;
        this->rx_.push_back(b);
        if (this->rx_.size() >= 1) {
          size_t need = 1 + (size_t) this->rx_[0] + 2;  // LEN + payload + CRC
          if (this->rx_.size() == need) {
            uint16_t crc = crc16_modbus(this->rx_.data(), this->rx_.size() - 2);
            uint16_t got = this->rx_[need - 2] | (this->rx_[need - 1] << 8);
            if (crc == got) {
              if (this->decode_log_) this->log_decoded_(this->rx_.data(), this->rx_.size());
              if (this->passive_) this->decode_passive_(this->rx_.data(), this->rx_.size());
              else this->handle_frame_(this->rx_.data(), this->rx_.size());
            } else {
              ESP_LOGW(TAG, "CRC fail %s (calc %04X got %04X)", this->log_prefix_.c_str(), crc, got);
            }
            this->rx_state_ = WAIT_AA;
          }
          if (this->rx_.size() > 64) this->rx_state_ = WAIT_AA;  // guard
        }
        break;
    }
  }

  // resync if a partial frame stalls
  if (this->rx_state_ == BODY && (now - this->rx_last_ms_) > 100) this->rx_state_ = WAIT_AA;

  // flush the raw-debug buffer on an idle gap (frame boundary)
  if (this->debug_raw_ && !this->sniff_.empty() && (now - this->rx_last_ms_) > 15)
    this->sniff_flush_();

  if (this->passive_) return;   // listen-only: never transmit / no timeouts

  // request timeout
  if (this->in_flight_ && (now - this->inflight_ms_) > 400) {
    ESP_LOGD(TAG, "timeout id=%u func=0x%02X", this->inflight_id_, this->inflight_.func);
    this->in_flight_ = false;
  }
  this->pump_queue_();
}

// ----- decoded frame logging --------------------------------------------

void DucoComponent::log_decoded_(const uint8_t *buf, size_t len) {
  const uint8_t func = buf[1], id = buf[2];
  const uint8_t *d = buf + 3;
  const int n = (int) buf[0] - 2;   // data length
  auto b = [&](int i) -> int { return (i >= 0 && i < n) ? d[i] : 0; };
  char desc[112];
  switch (func) {
    case 0x0C:
      // One-byte requests carry no node: the real print polls `0C 00` and `0C 0B`
      // every ~1.5 s. `0B` returns a 32-byte bitmap of the registered nodes (bit n =
      // node n; here 7E .. 10 = nodes 1-6 + 52). `00` returns one byte, 0x66 = 102 here,
      // probably the highest node address (after a print restart it scans `0C 0A`
      // for nodes 0..101). Sniffed 03..06-10.
      if (n == 1 && d[0] == 0x0B)
        snprintf(desc, sizeof(desc), "node-list bitmap req");
      else if (n == 1)
        snprintf(desc, sizeof(desc), "box-read req sub 0x%02X", d[0]);
      // `0A <node>` -> `01 <node> <active> ? <e0> <e1>`: the node's active error
      // (`01 06 01 01 38 1F` = node 6, E56.31), matching the 0x2C pushes.
      else if (n >= 2 && d[0] == 0x0A)
        snprintf(desc, sizeof(desc), "node error-status req node %d", b(1));
      else
        snprintf(desc, sizeof(desc), "node-read req %s node %d",
                 b(0) == 0x02 ? "live" : b(0) == 0x01 ? "type" : b(0) == 0x04 ? "SET-MODE"
                 : b(0) == 0x08 ? "SET-PARENT" : b(0) == 0x09 ? "SET-ASSO" : "sub", b(1));
      break;
    case 0x0D: snprintf(desc, sizeof(desc), "node-read ACK"); break;
    case 0x0E: snprintf(desc, sizeof(desc), "node-read reply (%dB: mode=%d flow=%d)", n, b(0), b(2)); break;
    case 0x10: snprintf(desc, sizeof(desc), "multi-func req sub=0x%02X node %d", b(0), b(1)); break;
    case 0x11: snprintf(desc, sizeof(desc), "multi-func ACK"); break;
    case 0x12: snprintf(desc, sizeof(desc), "multi-func reply (%dB)", n); break;
    case 0x13: snprintf(desc, sizeof(desc), "multi-func done (no more data)"); break;
    case 0x04: snprintf(desc, sizeof(desc), "boot announce (bus reset)"); break;
    case 0x05: snprintf(desc, sizeof(desc), "boot ACK"); break;
    case 0x14: snprintf(desc, sizeof(desc), "boot-const req"); break;
    case 0x15: snprintf(desc, sizeof(desc), "boot-const ACK"); break;
    case 0x16: snprintf(desc, sizeof(desc), "boot-const reply"); break;
    case 0x19: snprintf(desc, sizeof(desc), "node-param ACK"); break;
    case 0x18: snprintf(desc, sizeof(desc), "node-param %s node %d reg 0x%02X",
                        b(0) == 0x01 ? "WRITE" : "read", b(1), b(2)); break;
    case 0x1A: snprintf(desc, sizeof(desc), "node-param reply (val=%d min=%d max=%d)",
                        b(1) | (b(2) << 8), b(5) | (b(6) << 8), b(7) | (b(8) << 8)); break;
    case 0x08:
      if (n >= 6 && d[0] == 0x02)
        snprintf(desc, sizeof(desc), "PUSH param 0x%02X = %u", b(1),
                 (unsigned)(b(2) | (b(3) << 8) | (b(4) << 16) | ((uint32_t) b(5) << 24)));
      else
        snprintf(desc, sizeof(desc), "PUSH change-notify (0x%02X)", b(0));
      break;
    case 0x0B: snprintf(desc, sizeof(desc), "PUSH ack"); break;
    case 0x1C: snprintf(desc, sizeof(desc), "box->print STATUS poll"); break;
    case 0x1D: snprintf(desc, sizeof(desc), "status ACK"); break;
    case 0x1E: snprintf(desc, sizeof(desc), "print STATUS reply (state=%d)", b(0)); break;
    // The func triplet runs symmetrically: when WE poll/announce towards the box
    // (0x1C resp. 0x20), the box acknowledges with 0x1F resp. 0x23. Observed in
    // duco-log-2026-08-19_12-12-24.txt, always paired with our own heartbeat id.
    case 0x1F: snprintf(desc, sizeof(desc), "box ACK on print STATUS poll"); break;
    case 0x20:
      if (b(0) == 0x01)
        snprintf(desc, sizeof(desc), "box->print IDENTITY WRITE field %d group %d = %u", b(1), b(2),
                 (unsigned)(b(3) | (b(4) << 8) | (b(5) << 16) | ((uint32_t) b(6) << 24)));
      else
        snprintf(desc, sizeof(desc), "box->print IDENTITY query field %d group %d", b(1), b(2));
      break;
    case 0x21: snprintf(desc, sizeof(desc), "identity ACK"); break;
    case 0x22: snprintf(desc, sizeof(desc), "print IDENTITY reply field %d group %d", b(1), b(2)); break;
    case 0x23: snprintf(desc, sizeof(desc), "box ACK on print IDENTITY announce"); break;
    case 0x24:
      switch (b(0)) {
        case 0x00: snprintf(desc, sizeof(desc), "box-param READ id 0x%02X%02X", b(2), b(1)); break;
        case 0x01: {
          // Some box params are signed: TimeZone (0x0802) runs -11..12 and the
          // real print sends it sign-extended (verified 19-08: -10 = F6 FF FF FF).
          uint32_t v = b(3) | (b(4) << 8) | (b(5) << 16) | ((uint32_t) b(6) << 24);
          if (v >= 0xFFFF0000u)
            snprintf(desc, sizeof(desc), "box-param WRITE id 0x%02X%02X = %d", b(2), b(1),
                     (int) (int32_t) v);
          else
            snprintf(desc, sizeof(desc), "box-param WRITE id 0x%02X%02X = %u", b(2), b(1),
                     (unsigned) v);
          break;
        }
        // Two distinct reads, both answered with the same 11-byte 0x26 payload
        // `01 <unix u32 LE> 00 00 00 00 <TimeZone i8> <Dst u8>`:
        //   sub 02 -> LOCAL time (TimeZone+Dst already applied)
        //   sub 04 -> UTC
        // Verified 19-08 against duco-sniff-2026-08-19_18-31-22.txt.
        case 0x02: snprintf(desc, sizeof(desc), "box TIME read (local)"); break;
        case 0x04: snprintf(desc, sizeof(desc), "box TIME read (UTC)"); break;
        case 0x05: snprintf(desc, sizeof(desc), "box TIME set"); break;
        case 0x06: snprintf(desc, sizeof(desc), "node refresh node %d", b(1)); break;
        case 0x0B: snprintf(desc, sizeof(desc), "PROGRAM list read"); break;
        case 0x0C: snprintf(desc, sizeof(desc), "PROGRAM read idx %d", b(1)); break;
        case 0x0F: snprintf(desc, sizeof(desc), "PROGRAM write header idx %d", b(1)); break;
        case 0x11: snprintf(desc, sizeof(desc), "zone/circuit READ c%d prop 0x%02X grp 0x%02X", b(1), b(2), b(3)); break;
        case 0x12: snprintf(desc, sizeof(desc), "zone/circuit WRITE c%d prop 0x%02X grp 0x%02X", b(1), b(2), b(3)); break;
        case 0x13: snprintf(desc, sizeof(desc), "group/zone array read idx %d", b(1)); break;
        case 0x15: snprintf(desc, sizeof(desc), "PROGRAM write points idx %d", b(1)); break;
        default: snprintf(desc, sizeof(desc), "param-svc sub 0x%02X", b(0)); break;
      }
      break;
    case 0x25: snprintf(desc, sizeof(desc), "param-svc ACK"); break;
    case 0x26: snprintf(desc, sizeof(desc), "param-svc reply (%dB)", n); break;
    case 0x2C:
      if (n == 6 && d[0] == 0x11) snprintf(desc, sizeof(desc), "PUSH error node %d E%d.%02d.%02d", b(1), b(3), b(4), b(5));
      else if (n == 4 && d[0] == 0x0C) snprintf(desc, sizeof(desc), "PUSH install-mode %s", b(3) == 1 ? "ON" : "OFF");
      else snprintf(desc, sizeof(desc), "PUSH presence/event (0x%02X node %d)", b(0), b(1));
      break;
    case 0x2D: snprintf(desc, sizeof(desc), "presence ACK"); break;
    // 0x30..0x33 = error-log service. Verified: the 0x2C error-push bytes
    // (e.g. 38 1F 01 = E56.31.01, node 6) reappear literally in 0x32 replies.
    case 0x30:
      switch (b(0)) {
        case 0x00: snprintf(desc, sizeof(desc), "error-log status check"); break;
        case 0x01: snprintf(desc, sizeof(desc), "error-log read ALL active"); break;
        case 0x02: snprintf(desc, sizeof(desc), "error-log history index"); break;
        case 0x03: snprintf(desc, sizeof(desc), "error-log read node %d", b(1)); break;
        default: snprintf(desc, sizeof(desc), "error-log req sub 0x%02X", b(0)); break;
      }
      break;
    case 0x31: snprintf(desc, sizeof(desc), "error-log ACK"); break;
    case 0x32:
      // `cnt [node e0 e1 e2 flag]... 00`; print the first record when present
      if (n >= 6 && b(0) >= 1)
        snprintf(desc, sizeof(desc), "error-log reply: %d error(s), first: node %d E%u.%02u.%02u",
                 b(0), b(1), b(2), b(3), b(4));
      else
        snprintf(desc, sizeof(desc), "error-log reply: no errors");
      break;
    case 0x33: snprintf(desc, sizeof(desc), "error-log empty (no errors)"); break;
    default: snprintf(desc, sizeof(desc), "func 0x%02X (%dB)", func, n); break;
  }
  ESP_LOGI(TAG, "DEC %s 0x%02X id=%3u  %s", this->log_prefix_.c_str(), func, id, desc);
}

// ----- dispatch ----------------------------------------------------------

void DucoComponent::handle_frame_(const uint8_t *buf, size_t len) {
  uint8_t func = buf[1];
  uint8_t id = buf[2];
  const uint8_t *data = buf + 3;
  size_t dlen = (size_t) buf[0] - 2;  // LEN - (func+id)

  // ACK frames (FUNC odd: 0x0D/0x11/0x25/0x31) just confirm receipt
  if (func == 0x0D || func == 0x11 || func == 0x25 || func == 0x31) return;

  // Any frame that is NOT the reply to our own in-flight request is something the
  // DucoBox pushed onto the bus by itself -> always decode it (active + passive
  // happen at the same time; no mode to choose).
  if (!this->in_flight_ || id != this->inflight_id_) {
    this->decode_passive_(buf, len);
    return;
  }
  Request &q = this->inflight_;
  switch (q.kind) {
    case REQ_LIVE: this->parse_live_(q.node, data, dlen); break;
    case REQ_CO2:  this->parse_co2_(q.node, data, dlen);  break;
    case REQ_TYPE: this->parse_type_(q.node, data, dlen); break;
    case REQ_ERRORS: this->parse_errors_(q.node, data, dlen); break;
    case REQ_SUBTYPE: this->parse_subtype_(q.node, data, dlen); break;
    case REQ_CTRS: this->parse_ctrs_(q.node, data, dlen); break;
    case REQ_NODEVER: this->parse_nodever_(q.node, data, dlen); break;
    case REQ_PROG: this->parse_prog_(q.node, data, dlen); break;
#ifdef USE_TEXT_SENSOR
    case REQ_ZCOUNT: this->parse_zcount_(q.node, data, dlen); break;
    case REQ_ZNODES: this->parse_znodes_(q.node, data, dlen); break;
    case REQ_ERRLOG: this->parse_errlog_(data, dlen); break;
#endif
    case REQ_GEN:  this->parse_gen_(q.gen_index, data, dlen); break;
#if defined(USE_NUMBER) || defined(USE_SELECT)
    case REQ_NPARAM: this->parse_nparam_(q.gen_index, data, dlen); break;
#endif
#ifdef USE_SELECT
    case REQ_BOXSEL: this->parse_boxsel_(q.gen_index, data, dlen); break;
#endif
#ifdef USE_TEXT_SENSOR
    case REQ_STR:  this->parse_str_(q.gen_index, data, dlen); break;
    case REQ_BOXSTR: this->parse_box_str_(data, dlen); break;
#endif
    case REQ_READTIME: {
      // reply to our UTC read (0x24 sub 04)
      uint32_t t; int8_t tz = 0; uint8_t dst = 0;
      if (extract_box_time_(0x26, data, dlen, t, &tz, &dst)) {
        this->record_box_time_(t);
        this->record_box_tz_(tz, dst);
      }
      break;
    }
    case REQ_READTIME_LOCAL: {
      // reply to our LOCAL read (0x24 sub 02): same payload, but the timestamp
      // already has TimeZone+Dst applied. Verified 19-08: with tz=-10/dst=1 the
      // two reads differed by exactly 9 hours.
      uint32_t t; int8_t tz = 0; uint8_t dst = 0;
      if (extract_box_time_(0x26, data, dlen, t, &tz, &dst)) {
        this->record_box_tz_(tz, dst);        // first, so the suffix below is current
        this->record_box_local_time_(t);
      }
      break;
    }
    default: break;
  }
  this->in_flight_ = false;
  this->pump_queue_();
}

void DucoComponent::parse_live_(uint8_t node, const uint8_t *d, size_t n) {
  // Confirmed 20-byte live block (0x0E reply to 0x0C `02 <node>`):
  //   [0]=mode  [2]=flow actual %  [3]=flow target %  [4]=0xFF marker
  //   [16..19]=manual-until (absolute Unix time; 0 = AUTO / no timer)
  if (n < 4) return;
  auto it = this->nodes_.find(node);
  if (it == this->nodes_.end()) return;   // discovered but not configured
  NodeEntities &e = it->second;
  uint8_t mode = d[0];
  uint8_t flow = d[2];  // actual flow %
  uint32_t expiry =
      (n >= 20) ? (d[16] | (d[17] << 8) | (d[18] << 16) | ((uint32_t) d[19] << 24)) : 0;
  uint32_t remain =
      (this->last_box_time_ != 0 && expiry > this->last_box_time_) ? (expiry - this->last_box_time_) : 0;
#ifdef USE_SENSOR
  if (e.mode) e.mode->publish_state(mode);
  if (e.flow) e.flow->publish_state(flow);
  if (e.time_remaining) e.time_remaining->publish_state(remain);
  // diagnostic fields in the live block (only published if the user opted in):
  //   [6]=CommErrorCtr, [7]=RssiRfN2M, [8]=HopRf, [9]=RssiRfN2H (RF nodes)
  if (n >= 7 && e.comm_error_ctr) e.comm_error_ctr->publish_state(d[6]);
  if (n >= 10 && e.rssi_n2m) e.rssi_n2m->publish_state(d[7]);
  if (n >= 10 && e.hop_rf) e.hop_rf->publish_state(d[8]);
  if (n >= 10 && e.rssi_n2h) e.rssi_n2h->publish_state(d[9]);
  if (n >= 4 && e.flow_target) e.flow_target->publish_state(d[3]);  // FlowLvlTgt / requested
  if (n >= 5 && e.flow_ovrl) e.flow_ovrl->publish_state(d[4]);      // FlowLvlOvrl (255=none)
#endif
#ifdef USE_TEXT_SENSOR
  if (e.mode_text) e.mode_text->publish_state(mode_to_string(mode));
#ifdef USE_SELECT
  // Keep the dropdown in sync with what the bus reports, so it shows the real
  // state instead of whatever was last picked in the UI.
  if (e.mode_select != nullptr && e.mode_select_last != mode) {
    e.mode_select_last = mode;
    e.mode_select->publish_state(mode_to_string(mode));
  }
#endif
#endif
  ESP_LOGD(TAG, "node %u  mode=%s flow=%u%% remain=%us", node, mode_to_string(mode), flow,
           (unsigned) remain);
}

#ifdef USE_SELECT
void DucoComponent::parse_boxsel_(int idx, const uint8_t *d, size_t n) {
  // antwoord 0x26: 01 <lo> <hi> <waarde:u32 little-endian>
  if (idx < 0 || (size_t) idx >= this->box_selects_.size()) return;
  if (n < 7 || d[0] != 0x01) return;
  const BoxSelect &bs = this->box_selects_[idx];
  uint32_t v = d[3] | (d[4] << 8) | (d[5] << 16) | ((uint32_t) d[6] << 24);
  if (bs.sel == nullptr) return;
  auto opt = bs.sel->at(v);
  if (opt.has_value()) bs.sel->publish_state(*opt);
  else ESP_LOGW(TAG, "box-param 0x%04X: waarde %u heeft geen naam in de keuzelijst",
                bs.id, (unsigned) v);
}
#endif

#if defined(USE_NUMBER) || defined(USE_SELECT)
void DucoComponent::parse_nparam_(int idx, const uint8_t *d, size_t n) {
  // svc6 leesreply: 01 <val:u16> <inc:u16> <min:u16> <max:u16>, alles LITTLE-endian
  // (schrijven gaat big-endian -- zie write_node_param18).
  if (idx < 0 || (size_t) idx >= this->node_params_.size()) return;
  if (n < 9 || d[0] != 0x01) return;
  const NodeParam &np = this->node_params_[idx];
  uint16_t val = d[1] | (d[2] << 8);
  uint16_t lo = d[5] | (d[6] << 8), hi = d[7] | (d[8] << 8);
  // Een node die de parameter niet kent antwoordt zonder foutmelding met een
  // terugvalwaarde (waargenomen 24-08 bij een offline node). Buiten het gemelde
  // bereik is dus reden om het antwoord te laten liggen.
  if (val < lo || val > hi) {
    ESP_LOGW(TAG, "node %u reg 0x%02X: %u ligt buiten %u..%u, genegeerd",
             np.node, np.reg, (unsigned) val, (unsigned) lo, (unsigned) hi);
    return;
  }
#ifdef USE_NUMBER
  if (np.num != nullptr) np.num->publish_state(val);
#endif
#ifdef USE_SELECT
  if (np.sel != nullptr) {
    // de positie in de optielijst is de buswaarde
    auto opt = np.sel->at(val);
    if (opt.has_value()) np.sel->publish_state(*opt);
    else ESP_LOGW(TAG, "node %u reg 0x%02X: waarde %u heeft geen naam in de keuzelijst",
                  np.node, np.reg, (unsigned) val);
  }
#endif
  ESP_LOGD(TAG, "node %u reg 0x%02X = %u (%u..%u)", np.node, np.reg,
           (unsigned) val, (unsigned) lo, (unsigned) hi);
}
#endif

void DucoComponent::parse_co2_(uint8_t node, const uint8_t *d, size_t n) {
  // 0x49 sensor block reply DATA = 01/00 04 <u16[0]=Temp x0.1> <u16[1]=CO2 ppm>
  //                                  <u16[2]=RH?> <u16[3]=UpTime in minuten>
  if (n < 6) return;
  auto it = this->nodes_.find(node);
  if (it == this->nodes_.end()) return;
  NodeEntities &e = it->second;
  uint16_t temp = d[2] | (d[3] << 8);   // u16[0]
  uint16_t ppm = d[4] | (d[5] << 8);    // u16[1]
#ifdef USE_SENSOR
  if (e.co2) e.co2->publish_state(ppm);
  if (e.temp) e.temp->publish_state(temp * 0.1f);
  if (n >= 10 && e.uptime) e.uptime->publish_state(d[8] | (d[9] << 8));  // u16[3] = uptime (min)
#endif
  ESP_LOGD(TAG, "node %u  CO2=%u ppm temp=%.1f", node, ppm, temp * 0.1f);
}

void DucoComponent::parse_type_(uint8_t node, const uint8_t *d, size_t n) {
  if (n < 1) return;
  uint8_t type = d[0];
  if (type == 0) return;  // empty / no such node
  auto it = this->nodes_.find(node);
  if (it == this->nodes_.end()) return;
  NodeEntities &e = it->second;
#ifdef USE_TEXT_SENSOR
  if (e.type_text) e.type_text->publish_state(node_type_to_string(type));
#endif
#ifdef USE_SENSOR
  // type block: [1]=NetworkType [2]=Addr [3]=SubAddr [4]=Parent [5]=Asso
  if (n >= 2 && e.network_type) e.network_type->publish_state(d[1]);
  if (n >= 3 && e.addr) e.addr->publish_state(d[2]);
  if (n >= 4 && e.sub_addr) e.sub_addr->publish_state(d[3]);
  if (n >= 5 && e.parent) e.parent->publish_state(d[4]);
  if (n >= 6 && e.asso) e.asso->publish_state(d[5]);
#endif
  ESP_LOGI(TAG, "node %u  type=%u (%s)", node, type, node_type_to_string(type));
}

void DucoComponent::parse_subtype_(uint8_t node, const uint8_t *d, size_t n) {
#ifdef USE_SENSOR
  // reg 0x15 reply DATA = 01 01 <u16 SubType>
  if (n < 4) return;
  auto it = this->nodes_.find(node);
  if (it == this->nodes_.end() || it->second.subtype == nullptr) return;
  it->second.subtype->publish_state(d[2] | (d[3] << 8));
#endif
}

void DucoComponent::parse_ctrs_(uint8_t node, const uint8_t *d, size_t n) {
#ifdef USE_SENSOR
  // reg 0x18 reply DATA = 01 02 <u16 WeekCtr> <u16 BootCtr>
  if (n < 6) return;
  auto it = this->nodes_.find(node);
  if (it == this->nodes_.end()) return;
  NodeEntities &e = it->second;
  if (e.week_ctr) e.week_ctr->publish_state(d[2] | (d[3] << 8));
  if (e.boot_ctr) e.boot_ctr->publish_state(d[4] | (d[5] << 8));
#endif
}

void DucoComponent::parse_nodever_(uint8_t node, const uint8_t *d, size_t n) {
  // reg 0x00 reply DATA = 01 04 <ProductId u16> <major u16> <minor u16> <patch u16>
  if (n < 10 || d[0] != 0x01) return;
  auto it = this->nodes_.find(node);
  if (it == this->nodes_.end()) return;
  NodeEntities &e = it->second;
  uint16_t pid = d[2] | (d[3] << 8);
  uint16_t maj = d[4] | (d[5] << 8), mi = d[6] | (d[7] << 8), pa = d[8] | (d[9] << 8);
#ifdef USE_SENSOR
  if (e.product_id) e.product_id->publish_state(pid);
#endif
#ifdef USE_TEXT_SENSOR
  if (e.sw_version) {
    char b[24];
    snprintf(b, sizeof(b), "%u.%u.%u", maj, mi, pa);
    e.sw_version->publish_state(b);
  }
#endif
  e.nodever_done = true;  // static: read once
  ESP_LOGI(TAG, "node %u  ProductId=%u SwVersion=%u.%u.%u", node, pid, maj, mi, pa);
}

// ----- weekly programs (0x24 0C read / 0x24 0F+15 write) -----------------

static const char *program_type_to_string(uint8_t t) {
  switch (t) {
    case 1: return "VENT_SUP";
    case 5: return "NIGHT_BOOST";
    case 6: return "VENT_RH_SUMMER";
    default: return "TYPE?";
  }
}

#ifdef USE_TEXT_SENSOR
void DucoComponent::set_program_text(uint8_t idx, text_sensor::TextSensor *s) {
  this->program_text_[idx] = s;
  this->want_programs_ = true;
}

// Build a compact human-readable summary of one program record.
static std::string program_summary(uint8_t idx, const ProgramRec &p) {
  const uint8_t days = (p.pack >> 2) & 0x7F;
  const uint16_t months = (p.pack >> 9) & 0xFFF;
  const bool en = p.pack & 1;
  static const char *DOW = "MTWTFSS";
  std::string ds;
  for (int i = 0; i < 7; i++) ds += (days & (1 << i)) ? DOW[i] : '-';
  char head[96];
  snprintf(head, sizeof(head), "P%u %s zone=%u grp=%u %s days=%s months=0x%03X",
           idx + 1, program_type_to_string(p.type), p.zone, p.group,
           en ? "ON" : "OFF", ds.c_str(), months);
  std::string out(head);
  if (p.valcnt == 0xFF && p.npts == 0) {
    out += " cont";
  } else {
    for (uint8_t i = 0; i < p.npts && (i * 3 + 2) < sizeof(p.pts); i++) {
      char pt[16];
      snprintf(pt, sizeof(pt), " %02u:%02u=%u", p.pts[i * 3], p.pts[i * 3 + 1], p.pts[i * 3 + 2]);
      out += pt;
    }
  }
  return out;
}
#endif

void DucoComponent::parse_prog_(uint8_t idx, const uint8_t *d, size_t n) {
  // 0x26 reply: 01 <idx> <type> <zone0> <grp> <pack:3 LE> 00 <valcnt> <cnt> <npts> <pts 3B..>
  if (n < 12 || d[0] != 0x01) return;
  ProgramRec &p = this->programs_[idx];
  p.valid = true;
  p.type = d[2];
  p.zone = d[3];
  p.group = d[4];
  p.pack = (uint32_t) d[5] | ((uint32_t) d[6] << 8) | ((uint32_t) d[7] << 16);
  p.valcnt = d[9];
  p.cnt = d[10];
  p.npts = d[11];
  size_t o = 12, k = 0;
  while (o + 2 < n && k + 3 <= sizeof(p.pts) && (k / 3) < p.npts) {
    p.pts[k++] = d[o++]; p.pts[k++] = d[o++]; p.pts[k++] = d[o++];
  }
  this->publish_program_(idx);
}

void DucoComponent::publish_program_(uint8_t idx) {
  auto pit = this->programs_.find(idx);
  if (pit == this->programs_.end()) return;
  const ProgramRec &p = pit->second;
#ifdef USE_SWITCH
  auto sit = this->program_sw_.find(idx);
  if (sit != this->program_sw_.end() && sit->second != nullptr)
    sit->second->publish_state(p.pack & 1);
#endif
#ifdef USE_TEXT_SENSOR
  auto tit = this->program_text_.find(idx);
  if (tit != this->program_text_.end() && tit->second != nullptr)
    tit->second->publish_state(program_summary(idx, p));
  if (this->programs_dump_ != nullptr) {
    std::string dump;
    for (auto &kv : this->programs_) {
      if (!kv.second.valid) continue;
      if (!dump.empty()) dump += " | ";
      dump += program_summary(kv.first, kv.second);
    }
    this->programs_dump_->publish_state(dump);
  }
#endif
}

#ifdef USE_TEXT_SENSOR
void DucoComponent::parse_zcount_(uint8_t idx, const uint8_t *d, size_t n) {
  // reply 01 <idx> 00 00 01 <count>  -> then read the array (or publish "[]" if empty)
  if (n < 6 || d[0] != 0x01) return;
  uint8_t count = d[5];
  if (count == 0) {
    auto it = this->group_nodes_text_.find(idx);
    if (it != this->group_nodes_text_.end() && it->second != nullptr) it->second->publish_state("[]");
    return;
  }
  if (count > 16) count = 16;
  this->enqueue_({REQ_ZNODES, 0x24, idx, {0x13, idx, 0x00, 0x01, 0x01, count}});
}

void DucoComponent::parse_znodes_(uint8_t idx, const uint8_t *d, size_t n) {
  // reply 01 <idx> 00 01 01 <node u32 LE> x count  (nodes start at offset 5)
  if (n < 6 || d[0] != 0x01) return;
  auto it = this->group_nodes_text_.find(idx);
  if (it == this->group_nodes_text_.end() || it->second == nullptr) return;
  std::string out = "[";
  for (size_t o = 5; o + 3 < n; o += 4) {
    uint32_t node = d[o] | (d[o + 1] << 8) | (d[o + 2] << 16) | ((uint32_t) d[o + 3] << 24);
    if (out.size() > 1) out += ",";
    out += std::to_string(node);
  }
  out += "]";
  it->second->publish_state(out);
}
#endif

void DucoComponent::write_program_schedule(uint8_t idx, bool en, uint32_t days,
                                           uint32_t months, uint8_t cnt, uint8_t valcnt) {
  if (this->passive_ || this->sniffer_) return;
  uint32_t pack = (en ? 1u : 0u) | ((cnt ? 1u : 0u) << 1) |
                  ((days & 0x7F) << 2) | ((months & 0xFFF) << 9);
  // 0x24 0F <idx> <mask:4 LE = all 21 bits> <value:4 LE = pack> <cnt> <valcnt>
  std::vector<uint8_t> req = {0x0F, idx,
                              0xFF, 0xFF, 0x1F, 0x00,
                              (uint8_t) (pack & 0xFF), (uint8_t) ((pack >> 8) & 0xFF),
                              (uint8_t) ((pack >> 16) & 0xFF), 0x00,
                              cnt, valcnt};
  this->enqueue_({REQ_WRITE, 0x24, idx, req});
  // keep our cached copy in sync so an enable toggle preserves the schedule
  ProgramRec &p = this->programs_[idx];
  p.pack = pack; p.cnt = cnt; p.valcnt = valcnt;
}

void DucoComponent::write_program_points(uint8_t idx, const std::vector<uint8_t> &pts) {
  if (this->passive_ || this->sniffer_) return;
  uint8_t npts = (uint8_t) (pts.size() / 3);
  std::vector<uint8_t> req = {0x15, idx, 0x01, npts};
  for (uint8_t i = 0; i < npts * 3; i++) req.push_back(pts[i]);
  this->enqueue_({REQ_WRITE, 0x24, idx, req});
  ProgramRec &p = this->programs_[idx];
  p.npts = npts;
  for (uint8_t i = 0; i < npts * 3 && i < sizeof(p.pts); i++) p.pts[i] = pts[i];
}

void DucoComponent::set_program_enable(uint8_t idx, bool en) {
  if (this->passive_ || this->sniffer_) return;
  // preserve the cached schedule (days/months/valcnt); flip only the Enable bit
  ProgramRec &p = this->programs_[idx];
  uint32_t days = (p.pack >> 2) & 0x7F;
  uint32_t months = (p.pack >> 9) & 0xFFF;
  this->write_program_schedule(idx, en, days, months, p.cnt, p.valcnt);
}

// Format a 3-byte Duco error code as "E<a>.<b>.<c>" (e.g. 38 1F 01 -> "E56.31.01").
static std::string duco_error_str(uint8_t a, uint8_t b, uint8_t c) {
  char buf[16];
  snprintf(buf, sizeof(buf), "E%u.%02u.%02u", a, b, c);
  return std::string(buf);
}

void DucoComponent::parse_errors_(uint8_t node, const uint8_t *d, size_t n) {
  // 0x32 reply to 0x30 03 <node>: `01 <node> [<e0 e1 e2> <flag>]... 00`
  // (no active error -> the code triplet is 00 00 00). Collect all non-zero codes.
#ifdef USE_TEXT_SENSOR
  auto it = this->nodes_.find(node);
  if (it == this->nodes_.end() || it->second.active_error == nullptr) return;
  std::string out;
  // start after `01 <node>`; walk 4-byte records (code:3 + flag:1) until terminator/zeros
  for (size_t i = 2; i + 2 < n; i += 4) {
    if (d[i] == 0 && d[i + 1] == 0 && d[i + 2] == 0) continue;  // empty slot
    if (!out.empty()) out += ", ";
    out += duco_error_str(d[i], d[i + 1], d[i + 2]);
  }
  it->second.active_error->publish_state(out);   // "" when clear
  if (!out.empty()) ESP_LOGW(TAG, "node %u active error(s): %s", node, out.c_str());
#endif
}

#ifdef USE_TEXT_SENSOR
// 0x32 reply to 0x30 sub 01: `cnt [node e0 e1 e2 flag]... 00` -- ALL active errors.
// An empty reply (0x33, dlen 0) or cnt 0 means: no active errors.
void DucoComponent::parse_errlog_(const uint8_t *d, size_t n) {
  std::string out, info;
  uint8_t cnt = (n >= 1) ? d[0] : 0;
  size_t i = 1;
  for (uint8_t k = 0; k < cnt && i + 3 < n; k++, i += 5) {
    const uint8_t node = d[i], a = d[i + 1], b = d[i + 2], c = d[i + 3];
    if (a == 0 && b == 0 && c == 0) continue;
    char rec[32];
    snprintf(rec, sizeof(rec), "node %u: %s", node, duco_error_str(a, b, c).c_str());
    if (!out.empty()) out += ", ";
    out += rec;
    const char *txt = duco_error_info(a, b, c);
    char ir[160];
    snprintf(ir, sizeof(ir), "%s (node %u): %s", duco_error_str(a, b, c).c_str(), node,
             txt != nullptr ? txt : "onbekende foutcode; zie Duco-serviceinstructies");
    if (!info.empty()) info += " | ";
    info += ir;
  }
  if (this->errors_all_ != nullptr &&
      (!this->errors_all_->has_state() || this->errors_all_->state != out))
    this->errors_all_->publish_state(out);   // "" when clear
  if (this->errors_info_ != nullptr) {
    if (info.empty()) info = "Geen actieve fouten";
    if (!this->errors_info_->has_state() || this->errors_info_->state != info)
      this->errors_info_->publish_state(info);
  }
  if (!out.empty()) ESP_LOGW(TAG, "active errors: %s", out.c_str());
}

// Push the NL explanation for a single error (from the 0x2C unsolicited push).
void DucoComponent::publish_error_info_(uint8_t node, uint8_t a, uint8_t b, uint8_t c) {
  if (this->errors_info_ == nullptr) return;
  const char *txt = duco_error_info(a, b, c);
  char ir[160];
  snprintf(ir, sizeof(ir), "%s (node %u): %s", duco_error_str(a, b, c).c_str(), node,
           txt != nullptr ? txt : "onbekende foutcode; zie Duco-serviceinstructies");
  this->errors_info_->publish_state(ir);
}
#endif

void DucoComponent::parse_gen_(int idx, const uint8_t *d, size_t n) {
#ifdef USE_SENSOR
  if (idx < 0 || (size_t) idx >= this->gen_reads_.size()) return;
  GenRead &g = this->gen_reads_[idx];
  if (g.sens == nullptr) return;
  if ((size_t)(g.offset + g.size) > n) {
    // a 3-byte reply is the box's "no value" form -- that register is empty on this box.
    ESP_LOGD(TAG, "gen read: register empty / no value (need %u bytes, got %u)",
             g.offset + g.size, (unsigned) n);
    return;
  }
  uint32_t raw = 0;
  for (uint8_t i = 0; i < g.size; i++) raw |= (uint32_t) d[g.offset + i] << (8 * i);
  float val;
  if (g.sgn) {
    int32_t s;
    if (g.size == 1) s = (int8_t) raw;
    else if (g.size == 2) s = (int16_t) raw;
    else s = (int32_t) raw;
    val = (float) s * g.scale;
  } else {
    val = (float) raw * g.scale;
  }
  g.sens->publish_state(val);
#endif
}

#ifdef USE_TEXT_SENSOR
void DucoComponent::parse_str_(int idx, const uint8_t *d, size_t n) {
  if (idx < 0 || (size_t) idx >= this->str_reads_.size()) return;
  StrRead &sr = this->str_reads_[idx];
  if (sr.sens == nullptr) return;
  // reply DATA: [0]=0x01 ok (0x00 = not available on this box), string at [offset..]
  if (n <= sr.offset || d[0] != 0x01) return;
  std::string s;
  for (size_t i = sr.offset; i < n; i++) {
    char c = (char) d[i];
    if (c == '\0') break;
    if (c >= 32 && c < 127) s += c;
  }
  if (s.empty()) return;
  sr.sens->publish_state(s);
  sr.done = true;  // static string: read once, then stop polling it
  ESP_LOGI(TAG, "string read (func 0x%02X): '%s'", sr.func, s.c_str());
}

void DucoComponent::add_string_sensor(uint8_t node, uint8_t func, const std::vector<uint8_t> &req,
                                      uint8_t offset, text_sensor::TextSensor *s) {
  this->str_reads_.push_back(StrRead{func, req, offset, node, false, s});
}
#endif

#ifdef USE_SENSOR
void DucoComponent::add_pushed_sensor(uint8_t param, float scale, bool sgn, sensor::Sensor *s) {
  this->push_subs_.push_back(PushSub{param, scale == 0.0f ? 1.0f : scale, sgn, s});
  ESP_LOGCONFIG(TAG, "passive push sensor: param 0x%02X scale=%.3f", param, scale);
}
#endif

// store + publish a decoded box wall-clock (Unix seconds, UTC)
#ifdef USE_TEXT_SENSOR
void DucoComponent::add_box_string(uint16_t first_id, uint8_t words, text_sensor::TextSensor *s) {
  BoxStrRead b;
  b.first = first_id;
  b.words = words;
  b.got = 0;
  b.buf.assign((size_t) words * 4, 0);
  b.done = false;
  b.sens = s;
  this->box_strings_.push_back(b);
}

void DucoComponent::parse_box_str_(const uint8_t *d, size_t n) {
  // reply: 01 <lo> <hi> <4 chars>
  if (n < 7 || d[0] != 0x01) return;
  uint16_t id = d[1] | (d[2] << 8);
  for (auto &bs : this->box_strings_) {
    if (bs.done || id < bs.first || id >= bs.first + bs.words) continue;
    uint8_t w = (uint8_t)(id - bs.first);
    for (uint8_t i = 0; i < 4; i++) bs.buf[(size_t) w * 4 + i] = d[3 + i];
    bs.got |= (1u << w);
    if (bs.got != (uint32_t)((1u << bs.words) - 1)) return;
    std::string out;
    for (uint8_t c : bs.buf) {
      if (c == 0) break;                       // NUL terminates the run
      if (c >= 32 && c < 127) out += (char) c;
    }
    bs.done = true;
    if (bs.sens != nullptr && !out.empty()) bs.sens->publish_state(out);
    ESP_LOGI(TAG, "box string 0x%04X..0x%04X = %s", bs.first, bs.first + bs.words - 1,
             out.c_str());
    return;
  }
}
#endif  // USE_TEXT_SENSOR

void DucoComponent::record_box_local_time_(uint32_t t) {
  // 0x24 sub 02 answers with TimeZone+Dst already applied, so this is the wall
  // clock the DucoBox display shows. Formatting it as UTC is therefore correct:
  // the offset is already baked into the value.
#ifdef USE_TEXT_SENSOR
  if (this->time_local_text_ != nullptr) {
    time_t tt = (time_t) t;
    struct tm tmv;
    gmtime_r(&tt, &tmv);
    char b[40];
    size_t k = strftime(b, sizeof(b), "%Y-%m-%d %H:%M:%S", &tmv);
    // Append the offset the box itself reports, so the value is unambiguous:
    // TimeZone plus Dst is the offset already baked into this timestamp.
    if (this->box_tz_valid_ && k < sizeof(b)) {
      int off = (int) this->box_tz_ + (int) this->box_dst_;
      snprintf(b + k, sizeof(b) - k, " UTC%+d", off);
    }
    this->time_local_text_->publish_state(b);
  }
#endif
}

#ifdef USE_NUMBER
void DucoComponent::publish_bus_number_(uint16_t bus_id, float v) {
  auto it = this->bus_numbers_.find(bus_id);
  if (it != this->bus_numbers_.end() && it->second != nullptr &&
      (!it->second->has_state() || it->second->state != v))
    it->second->publish_state(v);
}
#endif

void DucoComponent::record_box_tz_(int8_t tz, uint8_t dst) {
  if (this->box_tz_valid_ && this->box_tz_ == tz && this->box_dst_ == dst) return;
  this->box_tz_ = tz;
  this->box_dst_ = dst;
  this->box_tz_valid_ = true;
  ESP_LOGI(TAG, "box TimeZone = %d, Dst = %u", (int) tz, (unsigned) dst);
  this->publish_diag_();
#ifdef USE_TEXT_SENSOR
  if (this->tz_text_ != nullptr) {
    // Show the effective offset: TimeZone plus Dst is what the box actually applies.
    char b[16];
    snprintf(b, sizeof(b), "UTC%+d", (int) tz + (int) dst);
    this->tz_text_->publish_state(b);
  }
#endif
#ifdef USE_NUMBER
  // keep the writable sliders in step with what the box actually reports
  this->publish_bus_number_(0x0802, (float) tz);
  this->publish_bus_number_(0x0806, (float) dst);
#endif
}

void DucoComponent::record_box_time_(uint32_t t) {
  this->last_box_time_ = t;
  this->last_box_time_ms_ = millis();
  ESP_LOGD(TAG, "box time = %u (unix)", (unsigned) t);
#ifdef USE_TEXT_SENSOR
  if (this->time_text_ != nullptr) {
    time_t tt = (time_t) t;
    struct tm tmv;
    gmtime_r(&tt, &tmv);
    char b[24];
    strftime(b, sizeof(b), "%Y-%m-%d %H:%M:%S", &tmv);
    this->time_text_->publish_state(b);
  }
#endif
}

// ----- passive decode ----------------------------------------------------
// Listen-only: decode the frames the DucoBox pushes onto the bus by itself.
// Confirmed so far: FUNC 0x08 sub 0x02 = a parameter report:
//   DATA = 02 <param:u8> <value:u32 LE>
void DucoComponent::decode_passive_(const uint8_t *buf, size_t len) {
  const uint8_t func = buf[1];
  const uint8_t *d = buf + 3;
  const size_t dlen = (size_t) buf[0] - 2;  // LEN - (func+id)

  // Box wall-clock (Unix time, u32 LE): 0x26 report (box->comm) or 0x24 sub 05 set
  // (comm->box). Uses the strict signature so parameter replies are not misread.
  uint32_t bt;
  if (extract_box_time_(func, d, dlen, bt)) { this->record_box_time_(bt); return; }

  // Comm-print RESPONDER: answer the box's print-directed frames as the print would.
  // 0x1C/0x20 are fully answered here (return); 0x08/0x2C are ACKed and then still
  // decoded below (they also carry pushed params / errors we want to publish).
  if (this->comm_responder_ && !this->passive_ && !this->sniffer_) {
    const uint8_t id = buf[2];   // ACKs/replies echo the box's request id
    // health: detect a box frame we already saw very recently = our reply was rejected
    if (func == 0x1C || func == 0x20 || func == 0x08 || func == 0x2C) {
      const uint16_t key = (uint16_t)(func << 8) | id;
      std::array<uint8_t, 3> head{{(uint8_t)(dlen > 0 ? d[0] : 0),
                                   (uint8_t)(dlen > 1 ? d[1] : 0),
                                   (uint8_t)(dlen > 2 ? d[2] : 0)}};
      if (key == this->last_box_key_ && head == this->last_box_head_ &&
          (millis() - this->last_box_ms_) < 400)
        this->cnt_retrans_++;
      this->last_box_key_ = key;
      this->last_box_head_ = head;
      this->last_box_ms_ = millis();
    }
    if (func == 0x1C) {                            // status poll -> ack + 20-byte status
      this->send_reply_(0x1D, id, {});
      std::vector<uint8_t> st(20, 0);
      st[0] = this->comm_state_;                   // connectivity state (3 = operational)
      this->send_reply_(0x1E, id, st);
      this->cnt_status_++;
      return;
    }
    if (func == 0x20 && dlen >= 3 && d[0] == 0x01) {  // identity WRITE (box configures print)
      const uint8_t field = d[1], group = d[2];
      if (dlen >= 7) {
        this->comm_ident_[(group << 8) | field] = {d[3], d[4], d[5], d[6]};
        this->comm_written_.insert((uint16_t)((group << 8) | field));   // never re-seed this
      }
      // In responder mode no external print ever sends 0x22, so the publish that
      // hangs off decode_comm_reply_() never fires. Publish here as well, or the
      // static-config text sensors stay empty forever.
#ifdef USE_TEXT_SENSOR
      if (this->want_comm_ident_) this->publish_comm_ident_();
#endif
      this->send_reply_(0x21, id, {});   // ack
      const uint32_t val = (dlen >= 7)
          ? (d[3] | (d[4] << 8) | (d[5] << 16) | ((uint32_t) d[6] << 24)) : 0;
      // group 0 / field 0 in write direction = Modbus address set on the box display
      if (group == 0 && field == 0 && val != 0) {
        if (val != this->modbus_addr_)
          ESP_LOGI(TAG, "box set Modbus address = %u", (unsigned) val);
        this->modbus_addr_ = val;
      }
      this->cnt_queries_++;
      return;
    }
    if (func == 0x20 && dlen >= 3 && d[0] == 0x00) {  // identity query -> 0x21 ack + 0x22
      const uint8_t field = d[1], group = d[2];
      if (group == 0 && field == 0) {                 // start of a handshake
        if (this->g0f0_seen_) this->cnt_enroll_++;     // ... again = box restarted it
        this->g0f0_seen_ = true;
      }
      this->seed_comm_ident_();                        // make sure the table is built
      if (this->comm_ident_.find((group << 8) | field) == this->comm_ident_.end())
        this->cnt_unknown_++;
      this->handle_comm_query_(id, field, group);
      this->cnt_queries_++;
      if (group == 0x02 && field == 0x00) this->cnt_ip_shared_++;   // IP sent to the box
      if (group == 0x02 && field == 0x02) this->cnt_mac_shared_++;  // MAC sent to the box
      return;
    }
    if (func == 0x08) { this->send_reply_(0x0B, id, {}); this->cnt_pushes_++; }  // -> empty ack
    if (func == 0x2C) { this->send_reply_(0x2D, id, {}); this->cnt_pushes_++; }  // -> empty ack
    // fall through so 0x08 params and 0x2C errors are still decoded/published
  }

  if (func == 0x08 && dlen >= 6 && d[0] == 0x02) {
    const uint8_t param = d[1];
    const uint32_t raw = d[2] | (d[3] << 8) | (d[4] << 16) | ((uint32_t) d[5] << 24);
    ESP_LOGD(TAG, "pushed param 0x%02X = %u", param, (unsigned) raw);
#ifdef USE_SENSOR
    for (auto &p : this->push_subs_) {
      if (p.param != param) continue;
      float v = p.sgn ? (float) (int32_t) raw : (float) raw;
      p.sens->publish_state(v * p.scale);
    }
#endif
    return;
  }
  // Error event push: the box announces a new/active node error unsolicited via
  // `0x2C 11 <node> 01 <code:3>` (e.g. 11 06 01 38 1F 01 = node 6, E56.31.01).
  if (func == 0x2C && dlen == 6 && d[0] == 0x11 && d[2] == 0x01) {
    const uint8_t node = d[1];
    const std::string err = duco_error_str(d[3], d[4], d[5]);
    ESP_LOGW(TAG, "node %u error pushed %s: %s", node, this->log_prefix_.c_str(), err.c_str());
#ifdef USE_TEXT_SENSOR
    auto it = this->nodes_.find(node);
    if (it != this->nodes_.end() && it->second.active_error != nullptr)
      it->second.active_error->publish_state(err);
    this->publish_error_info_(node, d[3], d[4], d[5]);
#endif
    return;
  }
  // Install/pairing-mode announcement: the box pushes an unsolicited 0x2C frame
  // `0C 01 04 01` on entering install mode and `0C 01 04 02` on returning to
  // operational (each observed once in the captures -- the only 4-byte 0x2C frames).
  if (func == 0x2C && dlen == 4 && d[0] == 0x0C && d[2] == 0x04) {
    const bool on = (d[3] == 0x01);
    ESP_LOGI(TAG, "install mode %s (announced by box)", on ? "ON" : "OFF");
#ifdef USE_TEXT_SENSOR
    if (this->install_text_ != nullptr) this->install_text_->publish_state(on ? "ON" : "OFF");
#endif
    return;
  }
  // Comm-print identity handshake. The box asks with 0x20 `00 <field> <group>` and the
  // print answers 0x22 `01 <field> <group> <4 bytes>`. Decode replies we hear, and (if
  // we are standing in for the print) answer the box's queries ourselves.
  if (func == 0x22 && dlen >= 3 && d[0] == 0x01) {   // passive decode of the print's reply
    this->decode_comm_reply_(d, dlen);
    return;
  }
  // Other pushed frame types (network scan) are logged in sniffer mode;
  // add decoders here as they are confirmed.
  ESP_LOGV(TAG, "passive frame func=0x%02X dlen=%u", func, (unsigned) dlen);
}

// ----- control -----------------------------------------------------------

void DucoComponent::set_mode(uint8_t mode, uint8_t node) {
  // confirmed set-mode: FUNC 0x0C, DATA = 04 <node> <mode> (node 1 = the box)
  ESP_LOGI(TAG, "set ventilation mode node %u -> %s (%u)", node, mode_to_string(mode), mode);
  this->enqueue_({REQ_SETMODE, 0x0C, node, {0x04, node, mode}});
}

// The comm print always issues this after a node write: a node refresh/commit
// (0x24 06 <node> 01 19 / 1A) followed by a re-enumerate (0x30 03 <node>).
void DucoComponent::node_refresh_(uint8_t node) {
  this->enqueue_({REQ_WRITE, 0x24, node, {0x06, node, 0x01, 0x19}});
  this->enqueue_({REQ_WRITE, 0x24, node, {0x06, node, 0x01, 0x1A}});
  this->enqueue_({REQ_WRITE, 0x30, node, {0x03, node}});
}

void DucoComponent::write_box_param(uint16_t id, uint32_t val) {
  if (this->passive_ || this->sniffer_) return;   // never transmit in listen-only modes
  std::vector<uint8_t> d = {0x01, (uint8_t)(id & 0xFF), (uint8_t)(id >> 8),
                            (uint8_t)(val & 0xFF), (uint8_t)((val >> 8) & 0xFF),
                            (uint8_t)((val >> 16) & 0xFF), (uint8_t)((val >> 24) & 0xFF)};
  ESP_LOGI(TAG, "write box param 0x%04X = %u", id, (unsigned) val);
  this->enqueue_({REQ_WRITE, 0x24, 1, d});
}

void DucoComponent::write_node_param18(uint8_t node, uint8_t reg, uint16_t val) {
  if (this->passive_ || this->sniffer_) return;
  // LET OP de byte-volgorde. Service 6 leest little-endian maar SCHRIJFT
  // big-endian. Bewezen 25-08 door de echte print Co2SetPoint op node 5 naar 800
  // te zien zetten: `01 05 03 03 20`, en 0x0320 = 800. Little-endian zou 0x2003 =
  // 8195 zijn, ver buiten het bereik van 0..2000. Deze functie stuurde het tot dan
  // toe little-endian, dus elke schrijfactie kwam verkeerd aan.
  ESP_LOGI(TAG, "write node %u reg 0x%02X = %u (0x18, big-endian)", node, reg, (unsigned) val);
  this->enqueue_({REQ_WRITE, 0x18, node, {0x01, node, reg, (uint8_t)(val >> 8), (uint8_t)(val & 0xFF)}});
  this->node_refresh_(node);
}

void DucoComponent::write_node_param10(uint8_t node, uint8_t reg, int16_t val) {
  if (this->passive_ || this->sniffer_) return;
  uint16_t v = (uint16_t) val;   // signed -> two's complement LE
  ESP_LOGI(TAG, "write node %u reg 0x%02X = %d (0x10, signed)", node, reg, (int) val);
  this->enqueue_({REQ_WRITE, 0x10, node, {0x02, node, 0x00, reg, 0x01, (uint8_t)(v & 0xFF), (uint8_t)(v >> 8)}});
  this->node_refresh_(node);
}

void DucoComponent::set_identify(uint8_t node, bool on) {
  if (this->passive_ || this->sniffer_) return;
  ESP_LOGI(TAG, "identify node %u -> %s", node, on ? "ON" : "OFF");
  this->enqueue_({REQ_WRITE, 0x10, node, {0x04, node, (uint8_t)(on ? 1 : 0)}});
  this->node_refresh_(node);
}

void DucoComponent::set_install_mode(bool on) {
  if (this->passive_ || this->sniffer_) return;
  ESP_LOGI(TAG, "installation mode -> %s", on ? "ON" : "OFF");
  this->enqueue_({REQ_WRITE, 0x10, 1, {0x00, (uint8_t)(on ? 1 : 0)}});
}

void DucoComponent::reset_filter() {
  ESP_LOGI(TAG, "reset filter timer");
  this->write_box_param(0x0A30, 1);
}
void DucoComponent::node_reboot(uint8_t node) {
  if (this->passive_ || this->sniffer_) return;
  ESP_LOGI(TAG, "reboot node %u", node);
  this->enqueue_({REQ_WRITE, 0x10, node, {0x08, node}});   // 0x10 08 <node>
}
void DucoComponent::node_reset_config(uint8_t node) {
  if (this->passive_ || this->sniffer_) return;
  ESP_LOGI(TAG, "reset config node %u", node);
  this->enqueue_({REQ_WRITE, 0x10, node, {0x0A, node}});   // 0x10 0A <node>
}
void DucoComponent::set_link_mode(uint8_t node, bool on) {
  if (this->passive_ || this->sniffer_) return;
  ESP_LOGI(TAG, "link mode node %u -> %s (vereist INSTALL mode)", node, on ? "ON" : "OFF");
  this->enqueue_({REQ_WRITE, 0x10, node, {0x0B, node, (uint8_t)(on ? 1 : 0)}});
}
void DucoComponent::set_node_parent(uint8_t node, uint8_t parent) {
  if (this->passive_ || this->sniffer_) return;
  ESP_LOGI(TAG, "set node %u parent -> %u", node, parent);
  this->enqueue_({REQ_WRITE, 0x0C, node, {0x08, node, parent}});  // 0x0C 08 <node> <parent>
  this->node_refresh_(node);
}
void DucoComponent::set_node_asso(uint8_t node, uint8_t asso) {
  if (this->passive_ || this->sniffer_) return;
  ESP_LOGI(TAG, "set node %u asso -> %u", node, asso);
  this->enqueue_({REQ_WRITE, 0x0C, node, {0x09, node, asso}});    // 0x0C 09 <node> <asso>
  this->node_refresh_(node);
}
void DucoComponent::write_zone_param(uint8_t circuit, uint8_t prop, uint8_t group, uint32_t value) {
  if (this->passive_ || this->sniffer_) return;
  ESP_LOGI(TAG, "write zone %u prop 0x%02X grp 0x%02X = %u", circuit, prop, group, (unsigned) value);
  // 0x24 12 <circuit> <prop> <group> <value u32 LE>
  this->enqueue_({REQ_WRITE, 0x24, circuit,
                  {0x12, circuit, prop, group, (uint8_t)(value & 0xFF), (uint8_t)((value >> 8) & 0xFF),
                   (uint8_t)((value >> 16) & 0xFF), (uint8_t)((value >> 24) & 0xFF)}});
}
void DucoComponent::write_zone_name(uint8_t circuit, const std::string &name) {
  if (this->passive_ || this->sniffer_) return;
  ESP_LOGI(TAG, "write zone %u name -> '%s'", circuit, name.c_str());
  // name in group 0x02 as forward-ASCII 4-byte words starting at prop 0x01, plus two
  // trailing zero words to clear any longer previous name.
  size_t words = (name.size() + 3) / 4 + 2;
  for (size_t w = 0; w < words; w++) {
    uint8_t b[4] = {0, 0, 0, 0};
    for (size_t i = 0; i < 4; i++) {
      size_t k = w * 4 + i;
      if (k < name.size()) b[i] = (uint8_t) name[k];
    }
    this->enqueue_({REQ_WRITE, 0x24, circuit,
                    {0x12, circuit, (uint8_t)(0x01 + w), 0x02, b[0], b[1], b[2], b[3]}});
  }
}

// ----- comm-print identity handshake (0x20 / 0x22) -----------------------

// Each identity field is 4 bytes. ASCII fields are stored as little-endian words,
// i.e. every 4-byte group is byte-reversed on the wire. Encode a string that way.
static void enc_str_fields(std::map<uint16_t, std::array<uint8_t, 4>> &tbl, uint8_t group,
                           uint8_t first_field, const std::string &s) {
  for (size_t off = 0, f = first_field; off < s.size(); off += 4, f++) {
    std::array<uint8_t, 4> w{0, 0, 0, 0};
    for (size_t i = 0; i < 4 && off + i < s.size(); i++)
      w[3 - i] = (uint8_t) s[off + i];      // reverse within the word
    tbl[(group << 8) | (uint8_t) f] = w;
  }
}
static std::string dec_str_fields(const std::map<uint16_t, std::array<uint8_t, 4>> &tbl,
                                  uint8_t group, uint8_t first, uint8_t count) {
  std::string out;
  for (uint8_t f = first; f < first + count; f++) {
    auto it = tbl.find((group << 8) | f);
    if (it == tbl.end()) break;
    for (int i = 3; i >= 0; i--) {           // un-reverse
      char c = (char) it->second[i];
      if (c == '\0') return out;
      if (c >= 32 && c < 127) out += c;
    }
  }
  return out;
}
static bool parse_ipv4(const std::string &s, std::array<uint8_t, 4> &out) {
  unsigned a, b, c, d;
  if (sscanf(s.c_str(), "%u.%u.%u.%u", &a, &b, &c, &d) != 4) return false;
  out = {(uint8_t) a, (uint8_t) b, (uint8_t) c, (uint8_t) d};
  return true;
}

void DucoComponent::decode_comm_reply_(const uint8_t *d, size_t n) {
  // 0x22 DATA = 01 <field> <group> <b0 b1 b2 b3>
  if (n < 3) return;
  const uint8_t field = d[1], group = d[2];
  std::array<uint8_t, 4> w{0, 0, 0, 0};
  for (size_t i = 0; i < 4 && 3 + i < n; i++) w[i] = d[3 + i];
  this->comm_ident_[(group << 8) | field] = w;
  if (this->want_comm_ident_) this->publish_comm_ident_();
}

void DucoComponent::publish_comm_ident_() {
#ifdef USE_TEXT_SENSOR
  auto ip_str = [&](uint8_t g, uint8_t f) -> std::string {
    auto it = this->comm_ident_.find((g << 8) | f);
    if (it == this->comm_ident_.end()) return "";
    char b[16];
    snprintf(b, sizeof(b), "%u.%u.%u.%u", it->second[0], it->second[1], it->second[2], it->second[3]);
    return std::string(b);
  };
  auto pub = [&](const char *key, const std::string &val) {
    auto it = this->comm_text_.find(key);
    if (it != this->comm_text_.end() && it->second != nullptr && !val.empty())
      it->second->publish_state(val);
  };
  pub("comm_ip", ip_str(0x02, 0));
  pub("comm_gateway", ip_str(0x02, 1));
  pub("comm_netmask", ip_str(0x01, 1));
  // Group 1 is the STATIC config block, group 2 the values actually in use.
  // Proven 19-08: changing them on either the box display or the print web UI
  // moves exactly these fields (f0/f1/f2/f5, plus f3 = Dhcp).
  pub("comm_static_ip", ip_str(0x01, 0));
  pub("comm_static_netmask", ip_str(0x01, 1));
  pub("comm_static_gateway", ip_str(0x01, 2));
  pub("comm_static_dns", ip_str(0x01, 5));
  pub("comm_hostname", dec_str_fields(this->comm_ident_, 0x02, 3, 4));
  pub("comm_serial_board", dec_str_fields(this->comm_ident_, 0x03, 5, 3));
  pub("comm_serial_duco", dec_str_fields(this->comm_ident_, 0x03, 9, 5));
  // product id (group3 field0, u32 LE) + version (fields 1/2/3)
  auto num = [&](uint8_t g, uint8_t f) -> uint32_t {
    auto it = this->comm_ident_.find((g << 8) | f);
    if (it == this->comm_ident_.end()) return 0;
    return it->second[0] | (it->second[1] << 8) | (it->second[2] << 16) | ((uint32_t) it->second[3] << 24);
  };
  uint32_t pid = num(0x03, 0);
  if (pid) { char b[12]; snprintf(b, sizeof(b), "%u", (unsigned) pid); pub("comm_product_id", b); }
  if (this->comm_ident_.count(0x0301)) {
    char b[24];
    snprintf(b, sizeof(b), "%u.%u.%u", (unsigned) num(0x03, 1), (unsigned) num(0x03, 2),
             (unsigned) num(0x03, 3));
    pub("comm_sw_version", b);
  }
#endif
}

// Fill any identity field the user did not configure from the ESP's own network,
// so the box shows the replacement print's real IP / gateway / netmask / host / MAC.
void DucoComponent::auto_fill_network_() {
  if (this->comm_cfg_host_.empty()) this->comm_cfg_host_ = App.get_name();
  if (this->comm_cfg_mac_.empty()) {
    std::string m = get_mac_address();   // "b8d61a56ce43"
    if (m.size() == 12)
      this->comm_cfg_mac_ = m.substr(0, 2) + ":" + m.substr(2, 2) + ":" + m.substr(4, 2) + ":" +
                            m.substr(6, 2) + ":" + m.substr(8, 2) + ":" + m.substr(10, 2);
  }
#ifdef USE_NETWORK
  if (this->comm_cfg_ip_.empty()) {
    for (auto &ip : network::get_ip_addresses()) {
      char buf[network::IP_ADDRESS_BUFFER_SIZE];   // str_to() replaces the deprecated str()
      ip.str_to(buf);
      std::string s(buf);
      if (!s.empty() && s != "0.0.0.0" && s.find(':') == std::string::npos) {  // IPv4, set
        this->comm_cfg_ip_ = s;
        break;
      }
    }
  }
#endif
  if (this->comm_cfg_mask_.empty()) this->comm_cfg_mask_ = "255.255.255.0";
  if (this->comm_cfg_gw_.empty() && !this->comm_cfg_ip_.empty()) {
    auto p = this->comm_cfg_ip_.rfind('.');   // gateway = <subnet>.1
    if (p != std::string::npos) this->comm_cfg_gw_ = this->comm_cfg_ip_.substr(0, p) + ".1";
  }
}

void DucoComponent::seed_comm_ident_() {
  this->auto_fill_network_();     // refresh network-derived fields (fills only empties)
  auto &t = this->comm_ident_;
  this->comm_seeded_ = true;
  // Seed a default, but NEVER overwrite a field the box configured on us. Before
  // this guard existed, every query reset e.g. the Modbus address and the static
  // IP back to the default, so the box kept rewriting them every ~40 s and the
  // setting never stuck.
  auto set = [this, &t](uint16_t key, std::array<uint8_t, 4> v) {
    if (!this->comm_written_.count(key)) t[key] = v;
  };
  // constant fields observed in the capture (defaults; overridden by config below)
  set(0x0000, {1, 0, 0, 0}); set(0x0001, {1, 0, 0, 0});
  set(0x0103, {1, 0, 0, 0}); set(0x0105, {8, 8, 8, 8});
  // g2/f06 = cloudverbinding (bewezen 24-08 met Stop/RestartAzureConnection).
  // De echte print meldt hier 1 als hij met Azure verbonden is. Wij hebben geen
  // cloudverbinding, dus 1 melden laat het display ten onrechte CLOUD: ACTIVE
  // tonen -- dat deed het tot nu toe. Daarom 0.
  set(0x0206, {0, 0, 0, 0});
  set(0x0308, {1, 0, 0, 0});
  set(0x0400, {1, 0, 0, 0}); set(0x0500, {1, 0, 0, 0});
  set(0x0600, {2, 0, 0, 0});
  // fields the real print answers with status 01 + zeros (missing -> box may
  // treat the handshake as incomplete and hide LAN info on the display)
  set(0x0100, {0, 0, 0, 0}); set(0x0102, {0, 0, 0, 0});
  set(0x030E, {0, 0, 0, 0}); set(0x030F, {0, 0, 0, 0});
  // Board-type identity of the real Duco Connectivity Board (comm print).
  // The box reads ProductId (g3/f0=21123) + version (g3/f1..3 = 8.3.0) to
  // recognise a board WITH a LAN interface; without it the display hides
  // INSTELLINGEN -> LAN-INSTELLINGEN. Defaults; user config overrides below.
  set(0x0300, {0x83, 0x52, 0x00, 0x00});   // ProductId 21123 (0x5283)
  set(0x0301, {0x08, 0x00, 0x00, 0x00});   // version major 8
  set(0x0302, {0x03, 0x00, 0x00, 0x00});   // version minor 3
  set(0x0303, {0x00, 0x00, 0x00, 0x00});   // version patch 0
  // g3/f4 = uptime of the comm board in seconds (verified against the real
  // print: the value tracks wall-clock exactly, 0 at power-on). A frozen value
  // makes the board look hung, so report our own uptime.
  uint32_t up = millis() / 1000;
  set(0x0304, {(uint8_t) up, (uint8_t)(up >> 8), (uint8_t)(up >> 16), (uint8_t)(up >> 24)});
  // Serial numbers of the Connectivity Board (g3/f5..f7 and g3/f9..f0D). The box
  // queries all of them; leaving them unanswered makes the handshake incomplete.
  // These are placeholder defaults with the same shape as a real board's -- set
  // `comm_identity: serial_board / serial_duco` to report your own.
  if (this->comm_cfg_serb_.empty()) enc_str_fields(t, 0x03, 5, "PS2335005678");
  if (this->comm_cfg_serd_.empty()) enc_str_fields(t, 0x03, 9, "P200002-230101-002");
  std::array<uint8_t, 4> ip;
  if (parse_ipv4(this->comm_cfg_ip_, ip)) set(0x0200, ip);
  if (parse_ipv4(this->comm_cfg_gw_, ip)) set(0x0201, ip);
  if (parse_ipv4(this->comm_cfg_mask_, ip)) set(0x0101, ip);
  if (!this->comm_cfg_mac_.empty()) {          // low 4 bytes, reversed (best effort)
    unsigned m[6];
    if (sscanf(this->comm_cfg_mac_.c_str(), "%x:%x:%x:%x:%x:%x",
               &m[0], &m[1], &m[2], &m[3], &m[4], &m[5]) == 6)
      set(0x0202, {(uint8_t) m[5], (uint8_t) m[4], (uint8_t) m[3], (uint8_t) m[2]});
  }
  if (!this->comm_cfg_host_.empty()) {
    enc_str_fields(t, 0x02, 3, this->comm_cfg_host_);
    // The box always queries g2/f3..f5; a short hostname must still fill all
    // three words with zero padding, or the handshake stays incomplete.
    for (uint16_t f = 0x0203; f <= 0x0205; f++) t.emplace(f, std::array<uint8_t, 4>{0, 0, 0, 0});
  }
  if (!this->comm_cfg_serb_.empty()) enc_str_fields(t, 0x03, 5, this->comm_cfg_serb_);
  if (!this->comm_cfg_serd_.empty()) enc_str_fields(t, 0x03, 9, this->comm_cfg_serd_);
  if (this->comm_cfg_pid_)
    set(0x0300, {(uint8_t) this->comm_cfg_pid_, (uint8_t)(this->comm_cfg_pid_ >> 8),
                 (uint8_t)(this->comm_cfg_pid_ >> 16), (uint8_t)(this->comm_cfg_pid_ >> 24)});
  if (!this->comm_cfg_ver_.empty()) {
    unsigned a = 0, b = 0, c = 0;
    sscanf(this->comm_cfg_ver_.c_str(), "%u.%u.%u", &a, &b, &c);
    set(0x0301, {(uint8_t) a, 0, 0, 0});
    set(0x0302, {(uint8_t) b, 0, 0, 0});
    set(0x0303, {(uint8_t) c, 0, 0, 0});
  }
}

void DucoComponent::handle_comm_query_(uint8_t id, uint8_t field, uint8_t group) {
  this->seed_comm_ident_();
  this->send_reply_(0x21, id, {});   // ACK (empty)
  auto it = this->comm_ident_.find((group << 8) | field);
  std::vector<uint8_t> data;
  if (it != this->comm_ident_.end()) {
    data = {0x01, field, group, it->second[0], it->second[1], it->second[2], it->second[3]};
  } else {
    // "no value": the real print still pads 4 data bytes (observed: 00 <f> <g> 00 00 00 00)
    data = {0x00, field, group, 0x00, 0x00, 0x00, 0x00};
  }
  this->send_reply_(0x22, id, data);
}

void DucoComponent::publish_diag_() {
#ifdef USE_SENSOR
  if (this->diag_sensors_.empty()) return;
  auto pub = [&](const char *k, uint32_t v) {
    auto it = this->diag_sensors_.find(k);
    if (it != this->diag_sensors_.end() && it->second != nullptr) it->second->publish_state(v);
  };
  pub("comm_queries_answered", this->cnt_queries_);
  pub("comm_unknown_queries", this->cnt_unknown_);
  pub("comm_status_polls", this->cnt_status_);
  pub("comm_pushes_acked", this->cnt_pushes_);
  pub("comm_box_retransmits", this->cnt_retrans_);
  pub("comm_enrollment_restarts", this->cnt_enroll_);
  pub("comm_ip_shared", this->cnt_ip_shared_);
  pub("comm_mac_shared", this->cnt_mac_shared_);
  if (this->modbus_addr_) pub("modbus_address", this->modbus_addr_);
  if (this->box_tz_valid_) {
    // TimeZone is signed (-11..12), so it cannot go through the uint32_t `pub`.
    auto it_tz = this->diag_sensors_.find("box_timezone");
    if (it_tz != this->diag_sensors_.end() && it_tz->second != nullptr)
      it_tz->second->publish_state((float) this->box_tz_);
    pub("box_dst", this->box_dst_);
  }
  // Static network config the box wrote into our identity table (group 1).
  // g1/f3 = Dhcp (0/1), proven 19-08; g6/f0 = WiFi-AP state (1 = on, 0 = off).
  auto ident = [&](uint16_t key, uint32_t &out) {
    auto it = this->comm_ident_.find(key);
    if (it == this->comm_ident_.end()) return false;
    out = it->second[0] | (it->second[1] << 8) | (it->second[2] << 16) |
          ((uint32_t) it->second[3] << 24);
    return true;
  };
  uint32_t v;
  if (ident(0x0103, v)) pub("static_dhcp", v);
  if (ident(0x0206, v)) pub("cloud_connected", v);
  // g6/f0 = NETWORK MODE. The enum was recovered 23-08 from the pointer table in
  // the ESP32 image (DROM, file offset 0x18D90):
  //   0=DISABLED  1=AUTOMATIC  2=ETHERNET  3=WIFI_CLIENT  4=WIFI_AP
  // 2 = ETHERNET matches what the real print reports at rest, on LAN.
  if (ident(0x0600, v)) { pub("network_mode", v); pub("wifi_ap_state", v); }
  // g5/f0 = the print's own state machine: 1 = idle, 4 = change in progress,
  // 3 = AP active. Names for these are not known yet.
  if (ident(0x0500, v)) pub("network_state", v);
#endif
}

void DucoComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "DucoBox UART:");
  ESP_LOGCONFIG(TAG, "  configured nodes: %u", (unsigned) this->nodes_.size());
  ESP_LOGCONFIG(TAG, "  discovery scan max node: %u", this->scan_max_);
  if (this->comm_responder_)
    ESP_LOGCONFIG(TAG, "  comm-print responder: ENABLED (0x20 identity + 0x1C status + 0x08/0x2C ack)");
  this->check_uart_settings(57600);
}

}  // namespace duco
}  // namespace esphome
