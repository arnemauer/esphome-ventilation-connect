#pragma once
// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 esphome-duco contributors
//
// Deze component is een afgeleid werk van ESPHome, waarvan de C++/runtime onder
// GPLv3 staat. Zie LICENSE.
// GEGENEREERD uit duco-parameters-per-module.md -- niet met de hand bijwerken.
//
// Waarom dit bestand naast duco_params.h bestaat: die tabel is een generieke
// parameterlijst en is voor deze box AANTOONBAAR ONJUIST bij minstens vier
// modules (0x00, 0x03, 0x0A, 0x13). `param: TempDepEnable` zou daar het verkeerde
// register aanspreken. De namen hieronder zijn het volledige API-pad zoals
// /info en /config ze gebruiken, en elk adres is op de bus waargenomen.
//
// Gebruik in de yaml:   param: Ventilation.Fan.PressSup
#include <cstdint>
namespace esphome { namespace duco {
struct DucoVerifiedDef { const char *path; uint16_t id; float scale; bool sgn; };
static const DucoVerifiedDef DUCO_VERIFIED[] = {
  {"ActiveHouse.AutoCtrl.EnableGroupAlign", 0x3B04, 1.000f, false},
  {"ActiveHouse.DataExt.TempCore", 0x8044, 0.100f, true},  // 0,1 °C
  {"ActiveHouse.DataExt.TempCoreStatus", 0x8046, 1.000f, false},
  {"ActiveHouse.DataExt.TempCoreTimeValid", 0x8045, 1.000f, false},
  {"ActiveHouse.TempHandler.TempCoreNodeSrc", 0x4001, 1.000f, false},
  {"ClimateCtrl.Cooling.NodeIn", 0x4101, 1.000f, false},
  {"ClimateCtrl.Cooling.NodeOut", 0x4103, 1.000f, false},
  {"ClimateCtrl.Cooling.Passthrough", 0x4105, 1.000f, false},
  {"ClimateCtrl.DataExt.CoolingPosIn", 0x8058, 1.000f, false},
  {"ClimateCtrl.DataExt.CoolingPosInStatus", 0x805A, 1.000f, false},
  {"ClimateCtrl.DataExt.CoolingPosInTimeValid", 0x8059, 1.000f, false},
  {"ClimateCtrl.DataExt.CoolingPosTgtOut", 0x8060, 1.000f, false},
  {"ClimateCtrl.DataExt.CoolingPosTgtOutStatus", 0x8062, 1.000f, false},
  {"ClimateCtrl.DataExt.CoolingPosTgtOutTimeValid", 0x8061, 1.000f, false},
  {"ClimateCtrl.DataExt.HeatingPosIn", 0x8054, 1.000f, false},
  {"ClimateCtrl.DataExt.HeatingPosInStatus", 0x8056, 1.000f, false},
  {"ClimateCtrl.DataExt.HeatingPosInTimeValid", 0x8055, 1.000f, false},
  {"ClimateCtrl.DataExt.HeatingPosTgtOut", 0x805C, 1.000f, false},
  {"ClimateCtrl.DataExt.HeatingPosTgtOutStatus", 0x805E, 1.000f, false},
  {"ClimateCtrl.DataExt.HeatingPosTgtOutTimeValid", 0x805D, 1.000f, false},
  {"ClimateCtrl.DataExt.Mode", 0x8064, 1.000f, false},
  {"ClimateCtrl.DataExt.ModeStatus", 0x8066, 1.000f, false},
  {"ClimateCtrl.DataExt.ModeTgt", 0x8068, 1.000f, false},
  {"ClimateCtrl.DataExt.ModeTgtStatus", 0x806A, 1.000f, false},
  {"ClimateCtrl.DataExt.ModeTgtTimeValid", 0x8069, 1.000f, false},
  {"ClimateCtrl.DataExt.ModeTimeValid", 0x8065, 1.000f, false},
  {"ClimateCtrl.General.TempEnvCooling", 0x4200, 0.100f, true},  // 0,1 °C
  {"ClimateCtrl.General.TempEnvHeating", 0x4201, 0.100f, true},  // 0,1 °C
  {"ClimateCtrl.Heating.NodeIn", 0x4100, 1.000f, false},
  {"ClimateCtrl.Heating.NodeOut", 0x4102, 1.000f, false},
  {"ClimateCtrl.Heating.Passthrough", 0x4104, 1.000f, false},
  {"General.Modbus.Addr", 0xE102, 1.000f, false},
  {"General.Setup.EnableDisplayErrorAlert", 0x1309, 1.000f, false},
  {"General.Setup.EnableDisplayUc", 0x1304, 1.000f, false},
  {"General.Setup.Language", 0x1301, 1.000f, false},
  {"General.Time.Dst", 0x0806, 1.000f, false},
  {"General.Time.TimeZone", 0x0802, 1.000f, true},
  {"HeatRecovery.Bypass.Adaptive", 0x0A11, 1.000f, false},
  {"HeatRecovery.Bypass.DoubleBypass", 0x0A15, 1.000f, false},
  {"HeatRecovery.Bypass.Mode", 0x0A10, 1.000f, false},
  {"HeatRecovery.Bypass.Pos", 0x0910, 1.000f, false},  // %
  {"HeatRecovery.Bypass.TempSupTgt", 0x0911, 0.100f, true},  // 0,1 °C
  {"HeatRecovery.Bypass.TempSupTgtZone1", 0x0A13, 0.100f, true},  // 0,1 °C
  {"HeatRecovery.Bypass.TempSupTgtZone2", 0x0A14, 0.100f, true},  // 0,1 °C
  {"HeatRecovery.Bypass.TimeFilter", 0x0A31, 1.000f, false},  // dagen
  {"HeatRecovery.General.TimeFilterRemain", 0x0930, 1.000f, false},  // dagen
  {"HeatRecovery.ProtectFrost.HeaterOdaExtPresent", 0x0A22, 1.000f, false},
  {"HeatRecovery.ProtectFrost.PassiveHouse", 0x0A21, 1.000f, false},
  {"NightBoost.General.Enable", 0x0301, 1.000f, false},
  {"NightBoost.General.FlowLvlReqMax", 0x0307, 1.000f, false},  // %
  {"NightBoost.General.MonthStart", 0x0303, 1.000f, false},  // maand
  {"NightBoost.General.MonthStop", 0x0304, 1.000f, false},  // maand
  {"NightBoost.General.TempStart", 0x0302, 1.000f, false},  // °C
  {"NightBoost.General.TimeStart", 0x0305, 1.000f, false},  // min na 0:00
  {"NightBoost.General.TimeStop", 0x0306, 1.000f, false},  // min na 0:00
  {"Ventilation.Boost.Enable", 0x0A76, 1.000f, false},
  {"Ventilation.Boost.Lvl", 0x0A75, 1.000f, false},  // %
  {"Ventilation.Calibration.FlowEhaCfg", 0x0959, 1.000f, false},  // m³/h
  {"Ventilation.Calibration.FlowSupCfgZone1", 0x0953, 1.000f, false},  // m³/h
  {"Ventilation.Calibration.FlowSupCfgZone2", 0x0955, 1.000f, false},  // m³/h
  {"Ventilation.Calibration.GroundBound", 0x0408, 1.000f, false},
  {"Ventilation.Calibration.PressEhaCfg", 0x0957, 1.000f, false},  // Pa
  {"Ventilation.Calibration.PressSupCfgZone1", 0x0950, 1.000f, false},  // Pa
  {"Ventilation.Calibration.PressSupCfgZone2", 0x095B, 1.000f, false},  // Pa
  {"Ventilation.Calibration.State", 0x0A50, 1.000f, false},
  {"Ventilation.Calibration.SubState", 0x0A58, 1.000f, false},
  {"Ventilation.Ctrl.TempDepEnable", 0x0001, 1.000f, false},
  {"Ventilation.Ctrl.TempDepThsHigh", 0x0003, 0.100f, true},  // 0,1 °C
  {"Ventilation.Ctrl.TempDepThsLow", 0x0002, 0.100f, true},  // 0,1 °C
  {"Ventilation.Ctrl.TreeBalanceThs", 0x0000, 1.000f, false},
  {"Ventilation.Fan.PressEha", 0x0946, 0.100f, false},  // 0,1 Pa
  {"Ventilation.Fan.PressEhaTgt", 0x0945, 0.100f, false},  // 0,1 Pa
  {"Ventilation.Fan.PressSup", 0x0942, 0.100f, false},  // 0,1 Pa
  {"Ventilation.Fan.PressSupTgt", 0x0941, 0.100f, false},  // 0,1 Pa
  {"Ventilation.Fan.PwmEha", 0x094E, 1.000f, false},
  {"Ventilation.Fan.PwmLvlEha", 0x0947, 1.000f, false},
  {"Ventilation.Fan.PwmLvlSup", 0x0943, 1.000f, false},
  {"Ventilation.Fan.PwmSup", 0x094D, 1.000f, false},
  {"Ventilation.Fan.SpeedEha", 0x0944, 1.000f, false},  // rpm
  {"Ventilation.Fan.SpeedSup", 0x0940, 1.000f, false},  // rpm
  {"Ventilation.IdentifyZone.State", 0x0410, 1.000f, false},
  {"Ventilation.Sensor.TempEha", 0x0903, 0.100f, true},  // 0,1 °C
  {"Ventilation.Sensor.TempEta", 0x0902, 0.100f, true},  // 0,1 °C
  {"Ventilation.Sensor.TempOda", 0x0900, 0.100f, true},  // 0,1 °C
  {"Ventilation.Sensor.TempSup", 0x0901, 0.100f, true},  // 0,1 °C
  {"WeatherHandler.DataExt.DirWind", 0x8004, 1.000f, false},
  {"WeatherHandler.DataExt.DirWindStatus", 0x8006, 1.000f, false},
  {"WeatherHandler.DataExt.DirWindTimeValid", 0x8005, 1.000f, false},
  {"WeatherHandler.DataExt.IrradEast", 0x800C, 1.000f, false},
  {"WeatherHandler.DataExt.IrradNorth", 0x8008, 1.000f, false},
  {"WeatherHandler.DataExt.IrradSouth", 0x8010, 1.000f, false},
  {"WeatherHandler.DataExt.IrradStatus", 0x8016, 1.000f, false},
  {"WeatherHandler.DataExt.IrradTimeValid", 0x8015, 1.000f, false},
  {"WeatherHandler.DataExt.IrradWest", 0x8014, 1.000f, false},
  {"WeatherHandler.DataExt.Precip", 0x801C, 1.000f, false},
  {"WeatherHandler.DataExt.PrecipStatus", 0x801E, 1.000f, false},
  {"WeatherHandler.DataExt.PrecipTimeValid", 0x801D, 1.000f, false},
  {"WeatherHandler.DataExt.SpeedWind", 0x8000, 1.000f, false},
  {"WeatherHandler.DataExt.SpeedWindStatus", 0x8002, 1.000f, false},
  {"WeatherHandler.DataExt.SpeedWindTimeValid", 0x8001, 1.000f, false},
  {"WeatherHandler.DataExt.TempOutside", 0x8018, 0.100f, true},  // 0,1 °C
  {"WeatherHandler.DataExt.TempOutsideStatus", 0x801A, 1.000f, false},
  {"WeatherHandler.DataExt.TempOutsideTimeValid", 0x8019, 1.000f, false},
  {"WeatherHandler.WeatherStationDiag.Period", 0x3501, 1.000f, false},
  {"WeatherHandler.WeatherStationDiag.Type", 0x3500, 1.000f, false},
  {"WeatherProtect.DataExt.LouvreVentState", 0x8024, 1.000f, false},
  {"WeatherProtect.DataExt.LouvreVentStatus", 0x8026, 1.000f, false},
  {"WeatherProtect.DataExt.LouvreVentTimeStart", 0x8034, 1.000f, false},
  {"WeatherProtect.DataExt.LouvreVentTimeStop", 0x8038, 1.000f, false},
  {"WeatherProtect.DataExt.LouvreVentTimeValid", 0x8025, 1.000f, false},
  {"WeatherProtect.DataExt.RoofWindowState", 0x8028, 1.000f, false},
  {"WeatherProtect.DataExt.RoofWindowStatus", 0x802A, 1.000f, false},
  {"WeatherProtect.DataExt.RoofWindowTimeStart", 0x803C, 1.000f, false},
  {"WeatherProtect.DataExt.RoofWindowTimeStop", 0x8040, 1.000f, false},
  {"WeatherProtect.DataExt.RoofWindowTimeValid", 0x8029, 1.000f, false},
  {"WeatherProtect.DataExt.ScreenState", 0x8020, 1.000f, false},
  {"WeatherProtect.DataExt.ScreenStatus", 0x8022, 1.000f, false},
  {"WeatherProtect.DataExt.ScreenTimeStart", 0x802C, 1.000f, false},
  {"WeatherProtect.DataExt.ScreenTimeStop", 0x8030, 1.000f, false},
  {"WeatherProtect.DataExt.ScreenTimeValid", 0x8021, 1.000f, false},
  {"WeatherProtect.DataExt.SlideState", 0x8048, 1.000f, false},
  {"WeatherProtect.DataExt.SlideStatus", 0x804A, 1.000f, false},
  {"WeatherProtect.DataExt.SlideTimeStart", 0x804C, 1.000f, false},
  {"WeatherProtect.DataExt.SlideTimeStop", 0x8050, 1.000f, false},
  {"WeatherProtect.DataExt.SlideTimeValid", 0x8049, 1.000f, false},
};
static const unsigned DUCO_VERIFIED_COUNT = sizeof(DUCO_VERIFIED)/sizeof(DUCO_VERIFIED[0]);

// Zoekt op volledig pad (Ventilation.Fan.PressSup) en, als dat niets oplevert,
// op alleen de bladnaam (PressSup) zolang die maar EEN keer voorkomt.
inline const DucoVerifiedDef *duco_find_verified(const char *n) {
  for (unsigned i = 0; i < DUCO_VERIFIED_COUNT; i++) {
    const char *a = DUCO_VERIFIED[i].path, *b = n;
    while (*a && *a == *b) { a++; b++; }
    if (*a == 0 && *b == 0) return &DUCO_VERIFIED[i];
  }
  const DucoVerifiedDef *hit = nullptr;
  for (unsigned i = 0; i < DUCO_VERIFIED_COUNT; i++) {
    const char *p = DUCO_VERIFIED[i].path, *blad = p;
    for (const char *q = p; *q; q++) if (*q == '.') blad = q + 1;
    const char *a = blad, *b = n;
    while (*a && *a == *b) { a++; b++; }
    if (*a == 0 && *b == 0) { if (hit != nullptr) return nullptr; hit = &DUCO_VERIFIED[i]; }
  }
  return hit;
}
}}  // namespace esphome::duco
