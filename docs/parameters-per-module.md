# DucoBox — parameters per module

Indeling en namen volgen `/info` en `/config` van de communicatieprint (`boxinfo.html` → `/info?module=<naam>`).

| | |
|---|---|
| ✅ | busadres waargenomen en op waarde gecontroleerd |
| ◆ | in gebruik door de component (`bus_id` in de yaml), eerder vastgesteld |
| 🟡 | naam komt voor in de adressentabel; niet op de bus gezien |
| ⚙️ | `DataExt`: via `0x24 01` naar module 0x80, index nog onbekend |

**Indeling van module 0x80.** Elke grootheid krijgt **vier opeenvolgende plekken**: `waarde`,
`TimeValid`, `Status`, en een ongebruikte vierde. Waar meerdere grootheden een `TimeValid`/`Status`
delen (zoals de vier `Irrad`-richtingen) staat dat gedeelde paar in het blok van de laatste.

    0x8000 SpeedWind    0x8018 TempOutside   0x8028 RoofWindow   0x8044 ActiveHouse.TempCore
    0x8004 DirWind      0x801C Precip        0x8048 Slide        0x8054 ClimateCtrl.HeatingPosIn
    0x8008 IrradNorth   0x8020 Screen                            0x8058 CoolingPosIn
    0x800C IrradEast    0x8024 LouvreVent                        0x805C HeatingPosTgtOut
    0x8010 IrradSouth                                            0x8060 CoolingPosTgtOut
    0x8014 IrradWest                                             0x8064 Mode
                                                                 0x8068 ModeTgt

`TimeStart`/`TimeStop` van WeatherProtect staan apart: 0x802C/0x8030 (Screen), 0x8034/0x8038
(LouvreVent), 0x803C/0x8040 (RoofWindow), 0x804C/0x8050 (Slide).
| ❓ | geen busadres bekend |
| 🖥️ | gaat NIET over de bus: instelling van de communicatieprint zelf |

**Waarschuwing bij 🟡.** **Behandel 🟡 als een aanwijzing, niet als een adres.**


## ActiveHouse


### ActiveHouse.AutoCtrl

| Naam | R/W | Waarde | Bereik | Eenheid | Busadres | |
|---|---|---|---|---|---|---|
| `EnableGroupAlign` | W | 0 | 0..1 /1 |  | 0x3B04 | ✅ |

### ActiveHouse.DataExt

| Naam | R/W | Waarde | Bereik | Eenheid | Busadres | |
|---|---|---|---|---|---|---|
| `TempCore` | W | 0 | -200..500 /1 | 0,1 °C | 0x8044 | ✅ |
| `TempCoreStatus` | W | 0 | 0..3 /1 |  | 0x8046 | ✅ |
| `TempCoreTimeValid` | W | 0 | 0..4294967295 /1 |  | 0x8045 | ✅ |

### ActiveHouse.TempHandler

| Naam | R/W | Waarde | Bereik | Eenheid | Busadres | |
|---|---|---|---|---|---|---|
| `TempCore` | R | 229 |  | 0,1 °C | 0x3E00 | 🟡 |
| `TempCoreFilt` | R | 225 |  | 0,1 °C | 0x3E01 | 🟡 |
| `TempCoreNodeSrc` | W | 255 | 0..255 /1 |  | 0x4001 | ✅ |

## ClimateCtrl


### ClimateCtrl.Cooling

| Naam | R/W | Waarde | Bereik | Eenheid | Busadres | |
|---|---|---|---|---|---|---|
| `NodeIn` | W | 0 | 0..255 /1 |  | 0x4101 | ✅ |
| `NodeOut` | W | 0 | 0..255 /1 |  | 0x4103 | ✅ |
| `Passthrough` | W | 0 | 0..1 /1 |  | 0x4105 | ✅ |
| `PosIn` | R | 0 |  |  | 0x3F00 of 0x3F01 | 🟡 |
| `PosOut` | R | 0 |  |  | 0x3F02 of 0x3F03 | 🟡 |
| `PosTgtOut` | R | 0 |  |  | 0x3F04 of 0x3F05 | 🟡 |

### ClimateCtrl.DataExt

| Naam | R/W | Waarde | Bereik | Eenheid | Busadres | |
|---|---|---|---|---|---|---|
| `CoolingPosIn` | W | 0 | 0..1 /1 |  | 0x8058 | ✅ |
| `CoolingPosInStatus` | W | 0 | 0..3 /1 |  | 0x805A | ✅ |
| `CoolingPosInTimeValid` | W | 0 | 0..4294967295 /1 |  | 0x8059 | ✅ |
| `CoolingPosTgtOut` | W | 0 | 0..1 /1 |  | 0x8060 | ✅ |
| `CoolingPosTgtOutStatus` | W | 0 | 0..3 /1 |  | 0x8062 | ✅ |
| `CoolingPosTgtOutTimeValid` | W | 0 | 0..4294967295 /1 |  | 0x8061 | ✅ |
| `HeatingPosIn` | W | 0 | 0..1 /1 |  | 0x8054 | ✅ |
| `HeatingPosInStatus` | W | 0 | 0..3 /1 |  | 0x8056 | ✅ |
| `HeatingPosInTimeValid` | W | 0 | 0..4294967295 /1 |  | 0x8055 | ✅ |
| `HeatingPosTgtOut` | W | 0 | 0..1 /1 |  | 0x805C | ✅ |
| `HeatingPosTgtOutStatus` | W | 0 | 0..3 /1 |  | 0x805E | ✅ |
| `HeatingPosTgtOutTimeValid` | W | 0 | 0..4294967295 /1 |  | 0x805D | ✅ |
| `Mode` | W | 0 | 0..4 /1 |  | 0x8064 | ✅ |
| `ModeStatus` | W | 0 | 0..3 /1 |  | 0x8066 | ✅ |
| `ModeTgt` | W | 0 | 0..4 /1 |  | 0x8068 | ✅ |
| `ModeTgtStatus` | W | 0 | 0..3 /1 |  | 0x806A | ✅ |
| `ModeTgtTimeValid` | W | 0 | 0..4294967295 /1 |  | 0x8069 | ✅ |
| `ModeTimeValid` | W | 0 | 0..4294967295 /1 |  | 0x8065 | ✅ |

### ClimateCtrl.General

| Naam | R/W | Waarde | Bereik | Eenheid | Busadres | |
|---|---|---|---|---|---|---|
| `Mode` | R | 5 |  |  | 0x1104 of 0x1305 of 0x1703 … | 🟡 |
| `ModeTgt` | R | 3 |  |  | 0x3F0C | 🟡 |
| `RhSummerRegimeActive` | R | True |  |  | 0x3F1B | 🟡 |
| `TempEnv` | R | 180 |  | 0,1 °C | 0x3F0F | 🟡 |
| `TempEnvCooling` | W | 160 | 100..240 /1 | 0,1 °C | 0x4200 | ✅ |
| `TempEnvHeating` | W | 120 | 100..240 /1 | 0,1 °C | 0x4201 | ✅ |

### ClimateCtrl.Heating

| Naam | R/W | Waarde | Bereik | Eenheid | Busadres | |
|---|---|---|---|---|---|---|
| `NodeIn` | W | 0 | 0..255 /1 |  | 0x4100 | ✅ |
| `NodeOut` | W | 0 | 0..255 /1 |  | 0x4102 | ✅ |
| `Passthrough` | W | 1 | 0..1 /1 |  | 0x4104 | ✅ |
| `PosIn` | R | 0 |  |  | 0x3F00 of 0x3F01 | 🟡 |
| `PosOut` | R | 0 |  |  | 0x3F02 of 0x3F03 | 🟡 |
| `PosTgtOut` | R | 0 |  |  | 0x3F04 of 0x3F05 | 🟡 |

## Firmware


### Firmware.General

| Naam | R/W | Waarde | Bereik | Eenheid | Busadres | |
|---|---|---|---|---|---|---|
| `DowngradeAllow` | W | 1 | 0..1 /1 |  | n.v.t. | 🖥️ |

## General


### General.AutoRebootComm

| Naam | R/W | Waarde | Bereik | Eenheid | Busadres | |
|---|---|---|---|---|---|---|
| `Period` | W | 7 | 0..365 /1 |  | n.v.t. | 🖥️ |
| `Time` | W | 0 | 0..1439 /1 | epoch | n.v.t. | 🖥️ |

### General.Board

| Naam | R/W | Waarde | Bereik | Eenheid | Busadres | |
|---|---|---|---|---|---|---|
| `ApiAccessSecurityLvl` | R | 16 |  |  | n.v.t. | 🖥️ |
| `ApiVersion` | R | 2.7.2.0 |  |  | n.v.t. | 🖥️ |
| `BoxName` | R | ENERGY |  |  | afgeleid van BoxSubType | ✅ |
| `BoxSubType` | R | 97 |  |  | node 1 reg 0x15 | ✅ |
| `BoxSubTypeName` | R | COMFORT_PLUS_D450_R |  |  | afgeleid van BoxSubType (enum in de print) | ✅ |
| `CommSubType` | R | 1 |  |  | n.v.t. | 🖥️ |
| `CommSubTypeName` | R | INSTALLER_KIT |  |  | n.v.t. | 🖥️ |
| `DateTime` | R | 2026-01-01 12:00:00 |  |  | n.v.t. (klok van de print) | 🖥️ |
| `ProductIdBox` | R | 23010 |  |  | node 1 reg 0x00, offset 2 | ✅ |
| `ProductIdComm` | R | 21123 |  |  | identity, comm_product_id | ✅ |
| `PublicApiVersion` | R | 2.7 |  |  | n.v.t. | 🖥️ |
| `SerialBoardBox` | R | RS2440001234 |  |  | node 1 reg 0x08 | ✅ |
| `SerialBoardComm` | R | PS2335005678 |  |  | identity, comm_serial_board | ✅ |
| `SerialDucoBox` | R | P100001-250101-001 |  |  | node 1 reg 0x1A / box 0x0602..0x0606 | ✅ |
| `SerialDucoComm` | R | P200002-230101-002 |  |  | identity, comm_serial_duco | ✅ |
| `SwVersionBox` | R | 23010.6.4.0 |  |  | node 1 reg 0x00, offset 4-8 | ✅ |
| `SwVersionBoxBoot` | R | n/a |  |  | — (box meldt "n/a") | ❓ |
| `SwVersionBoxSlave` | R | 15071.3.0.0 |  |  | service 5 (0x14) | ✅ |
| `SwVersionComm` | R | 21123.8.3.0 |  |  | identity, comm_sw_version | ✅ |
| `SwVersionCommBoot` | R | 21182.2.3.0 |  |  | n.v.t. | 🖥️ |
| `Time` | R | 1767265200 |  | epoch | 0x24 sub 02 (lokaal) | ✅ |
| `TimeUtc` | R | 1767258000 |  | epoch | 0x24 sub 04 (UTC) | ✅ |
| `TimeValid` | R | True |  |  | — | ❓ |
| `UpTime` | R | 951 |  | min | n.v.t. (uptime van de PRINT) | 🖥️ |

### General.Lan

| Naam | R/W | Waarde | Bereik | Eenheid | Busadres | |
|---|---|---|---|---|---|---|
| `DefaultGateway` | R | 192.168.1.1 |  |  | identity g2/f01 | ✅ |
| `Dhcp` | W | 1 | 0..1 /1 |  | identity g1/f03 | ✅ |
| `Dns` | R | 192.168.1.1 |  |  | identity g1/f05 | ✅ |
| `DucoClientIp` | R/W | 0.0.0.0 |  |  | n.v.t. | 🖥️ |
| `HostName` | R | duco_001234 |  |  | identity g2/f03-f04 | ✅ |
| `Ip` | R | 192.168.1.50 |  |  | identity g2/f00 | ✅ |
| `Mac` | R | b8:d6:1a:00:12:34 |  |  | identity g2/f02 | ✅ |
| `Mode` | R/W | 1 | 0..4 /1 |  | identity g6/f00 | ✅ |
| `MqttBroker` | W | 192.168.1.1 |  |  | n.v.t. | 🖥️ |
| `NetMask` | R | 255.255.255.0 |  |  | identity g1/f01 | ✅ |
| `StaticDefaultGateway` | W | 192.168.1.1 |  |  | identity g1/f02 | ✅ |
| `StaticDns` | W | 192.168.1.1 |  |  | identity g1/f05 | ✅ |
| `StaticIp` | W | 192.168.1.50 |  |  | identity g1/f00 | ✅ |
| `StaticNetMask` | W | 255.255.255.0 |  |  | identity g1/f01 | ✅ |
| `TimeDucoClientIp` | W | 600 | 0..3600 /1 |  | n.v.t. | 🖥️ |
| `WifiApKey` | R | aBC123456 |  |  | 0x060A..0x060C | ✅ |
| `WifiApSsid` | R | DUCO-P100001-250101-001 |  |  | afgeleid van 0x0602..0x0606 | ✅ |
| `WifiClientKey` | W |  |  |  | n.v.t. | 🖥️ |
| `WifiClientSsid` | W | DUCO |  |  | n.v.t. | 🖥️ |

### General.Modbus

| Naam | R/W | Waarde | Bereik | Eenheid | Busadres | |
|---|---|---|---|---|---|---|
| `Addr` | W | 1 | 1..254 /1 |  | 0xE102 | ✅ |
| `DailyWriteReqCnt` | W | 0 | 0..10000 /1 |  | n.v.t. | 🖥️ |
| `Offset` | W | 1 | 0..1 /1 |  | n.v.t. | 🖥️ |
| `WriteReqCntRemain` | R | 300 |  |  | 0xF301 of 0xF501 | 🟡 |

### General.NetworkDuco

| Naam | R/W | Waarde | Bereik | Eenheid | Busadres | |
|---|---|---|---|---|---|---|
| `HomeId` | R | 0x123456 |  |  | 0x0600 | 🟡 |
| `ResetAvail` | R | True |  |  | 0x0610 | 🟡 |
| `State` | R | OPERATIONAL |  |  | svc4 `0x10 00 <0|1>` | ✅ |

### General.NodeData

| Naam | R/W | Waarde | Bereik | Eenheid | Busadres | |
|---|---|---|---|---|---|---|
| `UpdateRate` | W | 60 | 5..3600 /1 |  | n.v.t. | 🖥️ |

### General.PublicApi

| Naam | R/W | Waarde | Bereik | Eenheid | Busadres | |
|---|---|---|---|---|---|---|
| `DailyWriteReqCnt` | W | 0 | 0..10000 /1 |  | n.v.t. | 🖥️ |
| `WriteReqCntRemain` | R | 300 |  |  | 0xF301 of 0xF501 | 🟡 |

### General.Setup

| Naam | R/W | Waarde | Bereik | Eenheid | Busadres | |
|---|---|---|---|---|---|---|
| `Complete` | W | 1 | 1..1 /1 |  | 0x1300 of 0x1302 | 🟡 |
| `Country` | W | 1 | 1..1 /1 |  | 0x1300 of 0x1302 | 🟡 |
| `EnableDisplayErrorAlert` | W | 1 | 0..1 /1 |  | 0x1309 | ✅ |
| `EnableDisplayUc` | W | 1 | 0..1 /1 |  | 0x1304 | ✅ |
| `Language` | W | 1 | 0..4 /1 |  | 0x1301 | ✅ |

### General.Time

| Naam | R/W | Waarde | Bereik | Eenheid | Busadres | |
|---|---|---|---|---|---|---|
| `Dst` | W | 1 | 0..1 /1 |  | 0x0806 | ✅ |
| `NtpServer` | W | pool.ntp.org |  |  | n.v.t. | 🖥️ |
| `TimeZone` | W | 1 | -11..12 /1 |  | 0x0802 | ✅ |

## HeatRecovery


### HeatRecovery.Bypass

| Naam | R/W | Waarde | Bereik | Eenheid | Busadres | |
|---|---|---|---|---|---|---|
| `Adaptive` | W | 1 | 0..1 /1 |  | 0x0A11 | ◆ |
| `DoubleBypass` | W | 0 | 0..1 /1 |  | 0x0A15 | ✅ |
| `Mode` | W | 0 | 0..2 /1 |  | 0x0A10 | ◆ |
| `Pos` | R | 100 |  | % | 0x0910 | ◆ |
| `TempSupTgt` | R | 204 |  | 0,1 °C | 0x0911 | ◆ |
| `TempSupTgtZone1` | W | 215 | 100..255 /1 | 0,1 °C | 0x0A13 | ◆ |
| `TempSupTgtZone2` | W | 215 | 100..255 /1 | 0,1 °C | 0x0A14 | ◆ |
| `TimeFilter` | W | 180 | 90..360 /90 | dagen | 0x0A31 | ✅ |

### HeatRecovery.General

| Naam | R/W | Waarde | Bereik | Eenheid | Busadres | |
|---|---|---|---|---|---|---|
| `TimeFilterRemain` | R | 167 |  | dagen | 0x0930 | ◆ |

### HeatRecovery.ProtectFrost

| Naam | R/W | Waarde | Bereik | Eenheid | Busadres | |
|---|---|---|---|---|---|---|
| `HeaterOdaExtPresent` | W | 0 | 0..1 /1 |  | 0x0A22 | ✅ |
| `HeaterOdaPresent` | R | False |  |  | 0x0923 | 🟡 |
| `PassiveHouse` | W | 0 | 0..1 /1 |  | 0x0A21 | ✅ |
| `PressReduct` | R | 0 |  |  | 0x0921 | 🟡 |
| `State` | R | 0 |  |  | 0x0601 of 0x0603 of 0x0709 … | 🟡 |

## MotorRelayCtrl


### MotorRelayCtrl.General

| Naam | R/W | Waarde | Bereik | Eenheid | Busadres | |
|---|---|---|---|---|---|---|
| `MoveCountMax230Vac` | W | 255 | 1..255 /1 |  | — | ❓ |
| `MoveCountMax24Vdc` | W | 255 | 1..255 /1 |  | — | ❓ |
| `MoveCountMax48Vdc` | W | 255 | 1..255 /1 |  | — | ❓ |
| `MoveCountMaxVf` | W | 1 | 1..255 /1 |  | — | ❓ |

## MotorStateCtrl


### MotorStateCtrl.Emergency

| Naam | R/W | Waarde | Bereik | Eenheid | Busadres | |
|---|---|---|---|---|---|---|
| `NodeIn` | W | 0 | 0..255 /1 |  | — | ❓ |
| `State` | R | 0 |  |  | 0x0601 of 0x0603 of 0x0709 … | 🟡 |
| `Time` | W | 300000 | 0..3600000 /60000 | epoch | 0xF016 | 🟡 |
| `Timer` | R | 0 |  |  | — | ❓ |

## NightBoost


### NightBoost.General

| Naam | R/W | Waarde | Bereik | Eenheid | Busadres | |
|---|---|---|---|---|---|---|
| `Enable` | W | 1 | 0..1 /1 |  | 0x0301 | ✅ |
| `FlowLvlReqMax` | W | 100 | 10..100 /5 | % | 0x0307 | ✅ |
| `FlowLvlReqZone1` | R | 26 |  | % | 0x2C12 | 🟡 |
| `FlowLvlReqZone2` | R | 0 |  | % | 0x2C13 | 🟡 |
| `MonthStart` | W | 1 | 1..12 /1 | maand | 0x0303 | ✅ |
| `MonthStop` | W | 12 | 1..12 /1 | maand | 0x0304 | ✅ |
| `TempComfort` | R | 234 |  | 0,1 °C | 0x2C03 of 0x2D06 | 🟡 |
| `TempOutside` | R | 216 |  | 0,1 °C | 0x0201 of 0x2C00 of 0x2D07 … | 🟡 |
| `TempOutsideAvg` | R | 180 |  | 0,1 °C | 0x2C01 of 0x2D01 | 🟡 |
| `TempOutsideAvgThs` | R | 120 |  | 0,1 °C | 0x2C02 of 0x2D02 | 🟡 |
| `TempStart` | W | 24 | 0..60 /1 | °C | 0x0302 | ✅ |
| `TempZone1` | R | 242 |  | 0,1 °C | 0x2C04 | 🟡 |
| `TempZone2` | R | 216 |  | 0,1 °C | 0x2C05 | 🟡 |
| `TimeCond` | R | True |  |  | 0x2C20 | 🟡 |
| `TimeStart` | W | 0 | 0..1439 /1 | min na 0:00 | 0x0305 | ✅ |
| `TimeStop` | W | 1439 | 0..1439 /1 | min na 0:00 | 0x0306 | ✅ |

## Ventilation


### Ventilation.Boost

| Naam | R/W | Waarde | Bereik | Eenheid | Busadres | |
|---|---|---|---|---|---|---|
| `Enable` | W | 0 | 0..1 /1 |  | 0x0A76 | ✅ |
| `Lvl` | W | 130 | 100..150 /5 | % | 0x0A75 | ✅ |

### Ventilation.Calibration

| Naam | R/W | Waarde | Bereik | Eenheid | Busadres | |
|---|---|---|---|---|---|---|
| `Error` | R | 0 |  |  | 0x070A of 0x0E02 of 0x1101 … | 🟡 |
| `FlowEhaCfg` | R/W | 295 | 0..450 /5 | m³/h | 0x0959 | ◆ |
| `FlowSupCfgZone1` | R/W | 205 | 0..275 /5 | m³/h | 0x0953 | ◆ |
| `FlowSupCfgZone2` | R/W | 90 | 0..275 /5 | m³/h | 0x0955 | ◆ |
| `GroundBound` | W | 1 | 0..1 /1 |  | 0x0408 | ✅ |
| `MethodInitMeasSysLoadAvail` | R | False |  |  | 0x0996 | 🟡 |
| `MethodPwmAvail` | R | False |  |  | 0x098F | 🟡 |
| `MethodSelectCstFlowPressAvail` | R | False |  |  | 0x0997 | 🟡 |
| `PressEha` | R | 150 |  | Pa | 0x0946 of 0x0958 | 🟡 |
| `PressEhaCfg` | R/W | 150 | 0..999 /1 | Pa | 0x0957 | ◆ |
| `PressSupCfgZone1` | R/W | 150 | 0..999 /1 | Pa | 0x0950 | ◆ |
| `PressSupCfgZone2` | R/W | 150 | 0..999 /1 | Pa | 0x095B | ◆ |
| `ResistEha` | R | 28917 |  |  | 0x095A | 🟡 |
| `ResistSupCommon` | R | 30000 |  |  | 0x0981 | 🟡 |
| `ResistSupZone1` | R | 22196 |  |  | 0x0954 | 🟡 |
| `ResistSupZone2` | R | 245651 |  |  | 0x0956 | 🟡 |
| `SpeedSupZone1` | R | 2086 |  |  | 0x0977 | 🟡 |
| `SpeedSupZone2` | R | 1871 |  |  | 0x0978 | 🟡 |
| `State` | R | IDLE |  |  | 0x0A50 | ✅ |
| `Status` | R | NOT_APPLICABLE |  |  | 0x1902 of 0x3600 | 🟡 |
| `SubState` | R/W | 0 | 0..21 /1 |  | 0x0A58 | ✅ |
| `Valid` | R | True |  |  | 0x0700 of 0x0E01 of 0x3602 | 🟡 |

### Ventilation.Ctrl

| Naam | R/W | Waarde | Bereik | Eenheid | Busadres | |
|---|---|---|---|---|---|---|
| `TempDepEnable` | W | 1 | 0..1 /1 |  | 0x0001 | ✅ |
| `TempDepThsHigh` | W | 240 | 160..350 /1 | 0,1 °C | 0x0003 | ✅ |
| `TempDepThsLow` | W | 160 | 100..240 /1 | 0,1 °C | 0x0002 | ✅ |
| `TreeBalanceThs` | W | 0 | 0..100 /1 |  | 0x0000 | ✅ |

### Ventilation.Fan

| Naam | R/W | Waarde | Bereik | Eenheid | Busadres | |
|---|---|---|---|---|---|---|
| `PressEha` | R | 78 |  | 0,1 Pa | 0x0946 | ✅ |
| `PressEhaTgt` | R | 130 |  | 0,1 Pa | 0x0945 | ✅ |
| `PressSup` | R | 227 |  | 0,1 Pa | 0x0942 | ✅ |
| `PressSupTgt` | R | 240 |  | 0,1 Pa | 0x0941 | ✅ |
| `PwmEha` | R | 11 |  |  | 0x094E | ✅ |
| `PwmLvlEha` | R | 4690 |  |  | 0x0947 | ✅ |
| `PwmLvlSup` | R | 10095 |  |  | 0x0943 | ✅ |
| `PwmSup` | R | 22 |  |  | 0x094D | ◆ |
| `SpeedEha` | R | 574 |  | rpm | 0x0944 | ✅ |
| `SpeedSup` | R | 907 |  | rpm | 0x0940 | ✅ |

### Ventilation.IdentifyZone

| Naam | R/W | Waarde | Bereik | Eenheid | Busadres | |
|---|---|---|---|---|---|---|
| `State` | R | IDLE |  |  | 0x0410 | ✅ |

### Ventilation.Sensor

| Naam | R/W | Waarde | Bereik | Eenheid | Busadres | |
|---|---|---|---|---|---|---|
| `TempEha` | R | 219 |  | 0,1 °C | 0x0903 | ✅ |
| `TempEta` | R | 218 |  | 0,1 °C | 0x0902 | ✅ |
| `TempOda` | R | 217 |  | 0,1 °C | 0x0900 | ✅ |
| `TempSup` | R | 215 |  | 0,1 °C | 0x0901 | ✅ |

## WeatherHandler


### WeatherHandler.DataExt

| Naam | R/W | Waarde | Bereik | Eenheid | Busadres | |
|---|---|---|---|---|---|---|
| `DirWind` | W | 0 | 0..359 /1 |  | 0x8004 | ✅ |
| `DirWindStatus` | W | 0 | 0..3 /1 |  | 0x8006 | ✅ |
| `DirWindTimeValid` | W | 0 | 0..4294967295 /1 |  | 0x8005 | ✅ |
| `IrradEast` | W | 0 | 0..200 /1 |  | 0x800C | ✅ |
| `IrradNorth` | W | 0 | 0..200 /1 |  | 0x8008 | ✅ |
| `IrradSouth` | W | 0 | 0..200 /1 |  | 0x8010 | ✅ |
| `IrradStatus` | W | 0 | 0..3 /1 |  | 0x8016 | ✅ |
| `IrradTimeValid` | W | 0 | 0..4294967295 /1 |  | 0x8015 | ✅ |
| `IrradWest` | W | 0 | 0..200 /1 |  | 0x8014 | ✅ |
| `Precip` | W | 0 | 0..1 /1 |  | 0x801C | ✅ |
| `PrecipStatus` | W | 0 | 0..3 /1 |  | 0x801E | ✅ |
| `PrecipTimeValid` | W | 0 | 0..4294967295 /1 |  | 0x801D | ✅ |
| `SpeedWind` | W | 0 | 0..1000 /1 |  | 0x8000 | ✅ |
| `SpeedWindStatus` | W | 0 | 0..3 /1 |  | 0x8002 | ✅ |
| `SpeedWindTimeValid` | W | 0 | 0..4294967295 /1 |  | 0x8001 | ✅ |
| `TempOutside` | W | 0 | -200..500 /1 | 0,1 °C | 0x8018 | ✅ |
| `TempOutsideStatus` | W | 0 | 0..3 /1 |  | 0x801A | ✅ |
| `TempOutsideTimeValid` | W | 0 | 0..4294967295 /1 |  | 0x8019 | ✅ |

### WeatherHandler.General

| Naam | R/W | Waarde | Bereik | Eenheid | Busadres | |
|---|---|---|---|---|---|---|
| `DirWind` | R | 0 |  |  | 0x2F04 | 🟡 |
| `IrradEastFilt` | R | 0 |  |  | 0x2F0B | 🟡 |
| `IrradNorthFilt` | R | 0 |  |  | 0x2F0A | 🟡 |
| `IrradSouthFilt` | R | 0 |  |  | 0x2F0C | 🟡 |
| `IrradWestFilt` | R | 0 |  |  | 0x2F0D | 🟡 |
| `Precip` | R | False |  |  | 0x0204 of 0x2F12 | 🟡 |
| `PrecipFilt` | R | False |  |  | 0x2F2B | 🟡 |
| `SpeedWindFilt` | R | 0 |  |  | 0x2F02 | 🟡 |
| `TempOutsideFilt` | R | 206 |  | 0,1 °C | 0x2F10 | 🟡 |

### WeatherHandler.WeatherStation

| Naam | R/W | Waarde | Bereik | Eenheid | Busadres | |
|---|---|---|---|---|---|---|
| `Type` | R | 0 |  |  | 0xE100 of 0xF901 | 🟡 |

### WeatherHandler.WeatherStationDiag

| Naam | R/W | Waarde | Bereik | Eenheid | Busadres | |
|---|---|---|---|---|---|---|
| `Enable` | R | False |  |  | 0x34FE | 🟡 |
| `Period` | W | 60 | 10..1200 /5 |  | 0x3401 of 0xF706 | 🟡 |
| `Type` | W | 0 | 0..3 /1 |  | 0xE100 of 0xF901 | 🟡 |

## WeatherProtect


### WeatherProtect.DataExt

| Naam | R/W | Waarde | Bereik | Eenheid | Busadres | |
|---|---|---|---|---|---|---|
| `LouvreVentState` | W | 0 | 0..1 /1 |  | 0x8024 | ✅ |
| `LouvreVentStatus` | W | 0 | 0..3 /1 |  | 0x8026 | ✅ |
| `LouvreVentTimeStart` | W | 0 | 0..4294967295 /1 |  | 0x8034 | ✅ |
| `LouvreVentTimeStop` | W | 0 | 0..4294967295 /1 |  | 0x8038 | ✅ |
| `LouvreVentTimeValid` | W | 0 | 0..4294967295 /1 |  | 0x8025 | ✅ |
| `RoofWindowState` | W | 0 | 0..1 /1 |  | 0x8028 | ✅ |
| `RoofWindowStatus` | W | 0 | 0..3 /1 |  | 0x802A | ✅ |
| `RoofWindowTimeStart` | W | 0 | 0..4294967295 /1 |  | 0x803C | ✅ |
| `RoofWindowTimeStop` | W | 0 | 0..4294967295 /1 |  | 0x8040 | ✅ |
| `RoofWindowTimeValid` | W | 0 | 0..4294967295 /1 |  | 0x8029 | ✅ |
| `ScreenState` | W | 0 | 0..1 /1 |  | 0x8020 | ✅ |
| `ScreenStatus` | W | 0 | 0..3 /1 |  | 0x8022 | ✅ |
| `ScreenTimeStart` | W | 0 | 0..4294967295 /1 |  | 0x802C | ✅ |
| `ScreenTimeStop` | W | 0 | 0..4294967295 /1 |  | 0x8030 | ✅ |
| `ScreenTimeValid` | W | 0 | 0..4294967295 /1 |  | 0x8021 | ✅ |
| `SlideState` | W | 0 | 0..1 /1 |  | 0x8048 | ✅ |
| `SlideStatus` | W | 0 | 0..3 /1 |  | 0x804A | ✅ |
| `SlideTimeStart` | W | 0 | 0..4294967295 /1 |  | 0x804C | ✅ |
| `SlideTimeStop` | W | 0 | 0..4294967295 /1 |  | 0x8050 | ✅ |
| `SlideTimeValid` | W | 0 | 0..4294967295 /1 |  | 0x8049 | ✅ |

### WeatherProtect.General

| Naam | R/W | Waarde | Bereik | Eenheid | Busadres | |
|---|---|---|---|---|---|---|
| `LouvreVentState` | R | False |  |  | 0x3102 | 🟡 |
| `RoofWindowState` | R | False |  |  | 0x3104 | 🟡 |
| `ScreenState` | R | False |  |  | 0x3100 | 🟡 |
| `SlideState` | R | False |  |  | 0x3106 | 🟡 |

---


# Node-parameters (service 6)

Aparte adresruimte: `0x18 00 <node> <param>` om te lezen, `0x18 01 <node> <param> <waarde:u16 BIG-endian>` om te schrijven.

Bereik en stapgrootte komen uit het leesantwoord van de box zelf.

| node | type |
|---|---|
| 1 | `BOX` |
| 2 | `EAV` |
| 3 | `EAV` |
| 4 | `UCCO2` |
| 5 | `UCCO2` |
| 6 | `UCCO2` |
| 52 | `SWITCH` |

## Configuratie per node

| Naam | param | R/W | Eenheid | Nodes (waarde) | Bereik | |
|---|---|---|---|---|---|---|
| `FlowLvlAutoMin` | 0x00 | R/W | % | 1=30, 2=30, 3=30 | 10..100 /5 | ✅ |
| `FlowLvlAutoMax` | 0x01 | R/W | % | 1=100, 2=100, 3=100 | 30..100 /5 | ✅ |
| `FlowMax` | 0x02 | R/W | m³/h | 1=295, 2=205, 3=90 | 0..750 /5 | ✅ |
| `Co2SetPoint` | 0x03 | R/W | ppm | 4=1230, 5=800, 6=810 | 0..2000 /10 | ✅ |
| `FlowLvlMan1` | 0x06 | R/W | % | 1=15, 2=10, 3=10, 4=10, 5=10, 6=10, 52=15 | 0..50 /5 | ✅ |
| `FlowLvlMan2` | 0x07 | R/W | % | 1=50, 2=50, 3=50, 4=50, 5=50, 6=50, 52=50 | 15..100 /5 | ✅ |
| `FlowLvlMan3` | 0x08 | R/W | % | 1=100, 2=100, 3=100, 4=100, 5=100, 6=100, 52=100 | 50..100 /5 | ✅ |
| `TimeMan` | 0x09 | R/W | min | 1=15, 2=15, 3=15, 4=15, 5=15, 6=15, 52=15 | 5..720 /5 | ✅ |
| `SwitchMode` | 0x0A | R/W |  | 52=2 | 1..12 /1 | ✅ |
| `FlowLvlSwitch` | 0x0B | R/W | % | 52=100 | 0..100 /5 | ✅ |
| `TempDepEnable` | 0x0E | R/W |  | 4=1, 5=1, 6=1 | 0..1 /1 | ✅ |
| `ShowSensorLvl` | 0x12 | R/W | % | 4=0, 5=0, 6=0 | 0..100 /5 | ✅ |
| `TimeOverrunSwitch` | 0x13 of 0x17 | R/W | min | 52=30 | 0..60 /1 | 🟡 |
| `UcErrorMode` | 0x14 | R/W |  | 1=2, 4=2, 5=2 | 0..2 /1 | ✅ |
| `UcWarningMode` | 0x18 | R/W |  | 1=2, 4=2, 5=2 | 0..2 /1 | ✅ |
| `UcInfoMode` | 0x19 | R/W |  | 1=2, 4=2, 5=2 | 0..2 /1 | ✅ |
| `Co2DetMode` | 0x1A | R/W |  | 4=0, 5=0, 6=0 | 0..2 /1 | ✅ |
| `TempOffset` | — | R/W | 0,1 °C | 4=0, 5=0 | -50..50 /1 | ❓ |

## Live-waarden per node

| Naam | Uitlezen | Eenheid | |
|---|---|---|---|
| `Temp` | `0x10` reg 0x49, offset 2 | 0,1 °C | ✅ |
| `Co2` | `0x10` reg 0x49, offset 4 | ppm | ✅ |
| `UpTime` | `0x10` reg 0x49, offset 8 | min | ✅ |
| `WeekCtr` | `0x10` reg 0x18, offset 2 | weken | ✅ |
| `BootCtr` | `0x10` reg 0x18, offset 4 | aantal | ✅ |
| `SerialBoard` | `0x10` reg 0x1A | tekst | ✅ |
| `ProductId` | `0x10` reg 0x00, offset 2 | — | ✅ |

## Enum-waarden

| Parameter | 0 | 1 | 2 |
|---|---|---|---|
| `UcErrorMode` / `UcWarningMode` / `UcInfoMode` | Disabled | On input | Automatic |
| `SwitchMode` | Overrule | Heatpump | Presence |
| `Co2DetMode` | Standard Air Quality detection | Toilet Detection | — |

`SwitchMode` meldt bereik 1..12; waarden 3..12 zijn onbekend.

