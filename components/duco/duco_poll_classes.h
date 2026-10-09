#pragma once
// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 esphome-ventilation-connect contributors
//
// Deze component is een afgeleid werk van ESPHome, waarvan de C++/runtime onder
// GPLv3 staat. Zie LICENSE.
//
// Box-parameters die de echte communicatieprint maar EEN keer per 15 minuten leest.
// Gemeten met de sniffer over 5,9 dagen (03-10 t/m 09-10-2026, 722k box-param reads):
// de print kent vaste klassen -- 0x0601 elke 2 s, temperaturen en bypass-doelen elke
// minuut, fan/calibratie/weer/status elke 3 minuten, en deze 92 instellingen elke
// 15 minuten (een paar daarvan twee-drie keer kort na elkaar). Ze veranderen alleen
// als iemand ze wijzigt, dus elke ronde opvragen (de standaard van deze component)
// belast de bus voor niets.
// Gebruikt als standaard-interval voor `param:`/`bus_id:`-sensoren zonder eigen
// `update_interval:` -- zie `config_param_interval:` op de hub.
#include <cstdint>
namespace esphome { namespace duco {
static const uint16_t DUCO_SETTING_PARAMS[] = {
  0x0000, 0x0001, 0x0002, 0x0003, 0x010A, 0x0301, 0x0302, 0x0303,
  0x0304, 0x0305, 0x0306, 0x0307, 0x0400, 0x0401, 0x0407, 0x0408,
  0x0409, 0x040F, 0x0802, 0x0806, 0x094A, 0x094B, 0x094C, 0x098D,
  0x098E, 0x0A10, 0x0A11, 0x0A15, 0x0A16, 0x0A20, 0x0A21, 0x0A22,
  0x0A31, 0x0A51, 0x0A52, 0x0A53, 0x0A54, 0x0A55, 0x0A56, 0x0A57,
  0x0A5A, 0x0A60, 0x0A66, 0x0A67, 0x0A6F, 0x0A75, 0x0A76, 0x0A79,
  0x0A7A, 0x0A80, 0x1300, 0x1301, 0x1302, 0x1304, 0x1308, 0x1309,
  0x2700, 0x2701, 0x2702, 0x2703, 0x2704, 0x3000, 0x3001, 0x3002,
  0x3003, 0x3500, 0x3501, 0x3502, 0x3503, 0x3504, 0x3505, 0x3506,
  0x3507, 0x3508, 0x35F0, 0x35F1, 0x3902, 0x3903, 0x3B04, 0x4001,
  0x4100, 0x4101, 0x4102, 0x4103, 0x4104, 0x4105, 0x4200, 0x4201,
  0x4A00, 0x4D00, 0x5100, 0x5101,
};
inline bool duco_is_setting_param(uint16_t id) {
  for (uint16_t p : DUCO_SETTING_PARAMS)
    if (p == id) return true;
  return false;
}
}}  // namespace esphome::duco
