# DucoBox-parameters — wat er te lezen en te schrijven valt

Naslagtabel voor de **DucoBox Energy Comfort Plus D450 R** met de originele communicatieprint.
De indeling volgt de **Installer Service Tool**, blok voor blok, zodat je een veld op het scherm
direct kunt terugvinden.

## Hoe je de kolommen leest

**R/W** — of het veld gelezen en/of geschreven kan worden over de bus.

**Zekerheid** — dit is geen officiële documentatie maar reverse-engineering, dus de betrouwbaarheid
verschilt per regel:

| | betekenis |
|---|---|
| ✅ | op de bus waargenomen **en** gecontroleerd tegen een tweede bron (API, tool of display) |
| 🟡 | adres afgeleid uit de parameternaam; nog nooit op de bus langs zien komen |
| ❓ | geen busadres bekend |

**Neem 🟡 niet over zonder te controleren.** Namen in dezelfde module lijken sterk op elkaar en de
schalen verschillen: binnen module 0x09 staan de ventilatordrukken in **0,1 Pa** en de
inregeldrukken in **hele Pa**.

## De drie adresruimtes

Er lopen drie manieren om iets uit te lezen door elkaar. Welke je nodig hebt, staat per regel in de
kolom "uitlezen".

| ruimte | vraag | antwoord | in de yaml |
|---|---|---|---|
| **box-parameter** | `0x24 00 <lo> <hi>` | `0x26 01 <lo> <hi> <4 bytes>` | `bus_id: 0x0942` of `param: PressSup` |
| **node-parameter** (service 6) | `0x18 00 <node> <param>` | `0x1A 01 <val> <inc> <min> <max>` | `type: time_man` + `node: 1` |
| **identity** (print ↔ box) | handshake `0x20` / `0x22` | groep/veld `g<n>/f<n>` | `type: comm_ip` |

**Twee valkuilen bij het schrijven.** Box-parameters schrijf je met `0x24 01`, node-parameters met
`0x18 01`. Bij node-parameters is het waardeveld **u16 big-endian**, terwijl de leesreply van
diezelfde service little-endian is. En de reply op een box-parameter echoot het parameternummer
terug, dus die hoef je niet op frame-id te koppelen — bij service 6 juist wél.

---

# Box info

## Toestel

| Naam | adres / hoe | R/W | eenheid | zekerheid |
|---|---|---|---|---|
| Product | `BoxSubTypeName` 0xf00b | R | tekst | 🟡 |
| SubType | `BoxSubType` 0xf009 | R | enum | 🟡 |
| Serienummer toestel | box-param 0x0602 t/m 0x0606 | R | tekst (18 tekens) | ✅ |
| Serienummer printplaat | `SerialBoardBox` | R | tekst | ❓ |
| Softwareversie | `SwVersionBox` 0xf005 | R | versie | 🟡 |
| — (niet in de tool) | `SwVersionBoxSlave` via **service 5** (`0x14`) | R | versie | ✅ |

Het serienummer wordt door de print **precies één keer** gelezen, 9 tot 12 seconden na zijn eigen
boot. Bij normaal bedrijf komt het niet langs.

## Connectiviteit

Dit blok gaat **niet** over de box maar over de communicatieprint, en loopt daarom via de
identity-handshake in plaats van via box-parameters.

| Naam | adres / hoe | R/W | eenheid | zekerheid |
|---|---|---|---|---|
| Softwareversie | identity, `comm_sw_version` | R | versie | ✅ |
| Serienummer printplaat | identity, `comm_serial_board` | R | tekst | ✅ |
| IP Address | identity `g2/f00` | R | IPv4 | ✅ |
| Default Gateway | identity `g2/f01` | R | IPv4 | ✅ |
| MAC Address | identity `g2/f02` | R | MAC (4 bytes) | ✅ |
| Wifi Access Point Key | box-param 0x060A t/m 0x060C | R | tekst | ✅ |

**Let op bij het MAC-adres:** de laagste byte verandert mee met de actieve interface — `43` bij
ethernet, `41` bij AP, `40` zonder verbinding. De ESP32 leidt per interface een adres af van één
basisadres. Wie dit als vast MAC leest, ziet het onverwacht verspringen.

## Filter

| Naam | adres / hoe | R/W | eenheid | zekerheid |
|---|---|---|---|---|
| Resterende dagen | `TimeFilterRemain` 0x0930 | R | dagen | 🟡 |

## Temperatuur

| Naam | adres / hoe | R/W | eenheid | zekerheid |
|---|---|---|---|---|
| ETA Temperatuur | `TempEta` 0x0902 | R | 0,1 °C | ✅ |
| EHA Temperatuur | `TempEha` 0x0903 | R | 0,1 °C | ✅ |
| ODA Temperatuur | `TempOda` 0x0900 | R | 0,1 °C | ✅ |
| SUP Temperatuur | `TempSup` 0x0901 | R | 0,1 °C | ✅ |

Geverifieerd langs drie wegen: de bus, de Installer Service Tool en het projectrapport naast de
Duco Home-app (bus 208 ↔ 20,8 °C).

**De Duco Network Tool verwisselt EHA en SUP** — die toont ze omgedraaid. De Installer Service Tool
en onze uitlezing komen wel overeen.

## Bypass

| Naam | adres / hoe | R/W | eenheid | zekerheid |
|---|---|---|---|---|
| Positie | `Pos` 0x0910 | R | % | 🟡 |
| — | `TempSupTgt` 0x0911 | R | 0,1 °C | 🟡 |
| — | `State` 0x0912 | R | enum | 🟡 |

## Vorstbeveiliging

| Naam | adres / hoe | R/W | eenheid | zekerheid |
|---|---|---|---|---|
| Status | `State` 0x0920 | R | enum | 🟡 |
| Niveau ODA Heater | `LvlHeaterOda` 0x0922 | R | % | 🟡 |
| Externe ODA Heater | `HeaterOdaPresent` 0x0923 | R | 0/1 | 🟡 |

## Inregeling

De ingestelde maxima per kanaal. **Deze drukken staan in hele Pa**, anders dan de ventilatordrukken
hieronder.

| Naam | adres / hoe | R/W | eenheid | zekerheid |
|---|---|---|---|---|
| Afvoerdebiet | `FlowEhaCfg` 0x0959 | R | m³/h | 🟡 |
| Toevoerdebiet Zone 1 | `FlowSupCfgZone1` 0x0953 | R | m³/h | 🟡 |
| Toevoerdebiet Zone 2 | `FlowSupCfgZone2` 0x0955 | R | m³/h | 🟡 |
| Afvoerdruk | `PressEhaCfg` 0x0957 | R | Pa | 🟡 |
| Toevoerdruk Zone 1 | `PressSupCfgZone1` 0x0950 | R | Pa | 🟡 |
| Toevoerdruk Zone 2 | `PressSupCfgZone2` 0x095B | R | Pa | 🟡 |

**Dezelfde debieten zijn óók bereikbaar als node-parameter**, en langs die weg wél geverifieerd:

| | node | node-param | waarde |
|---|---|---|---|
| Afvoerdebiet | 1 (BOX) | `FlowMax` 0x02 | 295 ✅ |
| Toevoerdebiet Zone 1 | 2 (EAV) | `FlowMax` 0x02 | 205 ✅ |
| Toevoerdebiet Zone 2 | 3 (EAV) | `FlowMax` 0x02 | 90 ✅ |

Twee wegen naar dezelfde instelling. De node-weg is **lezen én schrijven**, de box-weg is alleen
gecontroleerd op lezen.

## Toevoerventilator

| Naam | adres / hoe | R/W | eenheid | zekerheid |
|---|---|---|---|---|
| Streefdruk toevoerventilator | `PressSupTgt` 0x0941 | R | **0,1 Pa** | ✅ |
| Druk toevoerventilator | `PressSup` 0x0942 | R | **0,1 Pa** | ✅ |
| PWM toevoerventilator | `PwmSup` 0x094D | R | — | 🟡 |
| Snelheid toevoerventilator | `SpeedSup` 0x0940 | R | rpm | ✅ |

## Afvoerventilator

| Naam | adres / hoe | R/W | eenheid | zekerheid |
|---|---|---|---|---|
| Streefdruk afvoerventilator | `PressEhaTgt` 0x0945 | R | **0,1 Pa** | ✅ |
| Druk afvoerventilator | `PressEha` 0x0946 | R | **0,1 Pa** | ✅ |
| PWM afvoerventilator | `PwmEha` 0x094E | R | — | ✅ |
| Snelheid afvoerventilator | `SpeedEha` 0x0944 | R | rpm | ✅ |

**De drukschaal is 0,1 Pa**, bewezen met de constante streefwaarden: bus 240 ↔ 24,0 Pa en bus 130 ↔
13,0 Pa. Dit ging eerder mis omdat de bus werd vergeleken met de **REST-API van de print**, en die
geeft dezelfde ruwe waarde door. Twee bronnen die het eens zijn bewijzen niets als het dezelfde
meting is, twee keer verteld.

**PWM is niet wat het lijkt.** Er zijn **vier** pwm-parameters:

| adres | naam | waarde |
|---|---|---|
| 0x0943 / 0x0947 | `PwmLvlSup` / `PwmLvlEha` | ~10600 / ~8000 |
| 0x094D / 0x094E | `PwmSup` / `PwmEha` | 24 / 19 |

De verhouding is **geen constante factor** (10664/24 = 444, 8027/19 = 422), dus het zijn
verschillende grootheden en geen schaal van elkaar. Wat de fijnmazige `PwmLvl`-waarde telt, is
onbekend — vandaar geen eenheid. De tool labelt beide als "(Pa)", wat een fout in de tool is.

---

# Node 1 — Ventilatie

Per node, via de live-uitlezing. Node 1 is de box zelf.

| Naam | adres / hoe | R/W | eenheid | zekerheid |
|---|---|---|---|---|
| Stand | `type: mode` | R/W | enum (AUTO, MAN1-3, …) | ✅ |
| Resterende tijd voor stand | `type: time_remaining` | R | s | ✅ |
| Debiet | `type: flow` (werkelijk) / `flow_target` (gevraagd) | R | % | ✅ |
| — (niet in de tool) | `type: uptime`, reg 0x49 offset 8 | R | minuten | ✅ |
| — | `type: co2`, reg 0x49 offset 4 | R | ppm | ✅ |
| — | `type: temp`, reg 0x49 offset 2 | R | 0,1 °C | ✅ |

**`uptime` heette eerder `iaq_co2`, en dat was fout.** Offset 8 van het 0x49-blok is de uptime van
de node in minuten, niet een luchtkwaliteitspercentage. Bewezen: node 4 gaf op de bus 386 terwijl de
API `IaqCo2 = 100` en `UpTime = 393` meldde. De oude naam werkt nog als alias.

Bij de standen: de x2- en x3-varianten (MAN1x2 tot MAN3x3) zijn wél instelbaar. De firmware pakt een
druktelling in de bovenste twee bits: `waarde = (aantal << 6) | basisstand`.

---

# Instellingen

## Local Area Network

| Naam | adres / hoe | R/W | eenheid | zekerheid |
|---|---|---|---|---|
| IP Mode (DHCP) | identity `g1/f03` | R/W | 0 = statisch, 1 = DHCP | ✅ |
| Static IP Address | identity groep 1 | R/W | IPv4 | ✅ |
| Static Subnet Mask | identity groep 1 | R/W | IPv4 | ✅ |
| Static Default Gateway | identity groep 1 | R/W | IPv4 | ✅ |
| — (niet in de tool) | netwerktoestand `g5/f0` | R | zie hieronder | ✅ |
| — | netwerkmodus `g6/f0` | R/W | zie hieronder | ✅ |

`g5/f0` — **1** = verbonden, **2** = geen verbinding, **3** = AP actief, **4** = bezig met wijzigen.
Die 4 is een **overgang**, geen toestand: hij gaat aan elke wissel vooraf en duurt 6 tot 10 seconden.

`g6/f0` — 0 = DISABLED, 1 = AUTOMATIC, 2 = ETHERNET, 3 = WIFI_CLIENT, 4 = WIFI_AP. **Dit is een
opdrachtkanaal, geen statusveld.** De box zet er kortstondig een opdracht in (1 om de AP te starten,
0 om te stoppen) en de print schrijft binnen 7 tot 9 seconden zijn werkelijke modus terug. Voor de
echte toestand is `g5/f0` de betrouwbare bron.

## Modbus

| Naam | adres / hoe | R/W | eenheid | zekerheid |
|---|---|---|---|---|
| Address | `Addr` 0xe102 | R/W | 1-247 | ✅ |
| Offset | — | — | — | ❓ |

## NightBoost

| Naam | adres / hoe | R/W | eenheid | zekerheid |
|---|---|---|---|---|
| Inschakelen | — | — | 0/1 | ❓ |
| Start temperatuur | — | — | °C | ❓ |
| Start maand / Stop maand | — | — | 1-12 | ❓ |
| Start tijd / Stop tijd | — | — | minuten na middernacht | ❓ |
| Debiet | — | — | % | ❓ |

Deze staan bewust op ❓. In de API heten ze `NightBoost.General.*` en de waarden zijn bekend
(24 °C, januari t/m december, 0:00–23:59, 100 %), maar het busadres is **niet vastgesteld**. Module
0x03 in onze tabel draagt namen als `PosReq*` die daar niet bij passen — een eerdere match op
0x0306 = 1439 was waarschijnlijk toeval, want 1439 is simpelweg 23:59 in minuten.

## Bedieningsschakelaar

| Naam | adres / hoe | R/W | eenheid | zekerheid |
|---|---|---|---|---|
| Foutmodus | node-param **0x14** `UcErrorMode` | R/W | enum | ✅ |
| — (niet in de tool) | node-param **0x18** `UcWarningMode` | R/W | enum | ✅ |
| — | node-param **0x19** `UcInfoMode` | R/W | enum | ✅ |

Waarden: **0 = Disabled, 1 = On input, 2 = Automatic**.

## Manuele bediening

Node-parameters, dus per node instelbaar.

| Naam | node-param | R/W | eenheid | bereik | zekerheid |
|---|---|---|---|---|---|
| MAN1 Debiet | 0x06 `FlowLvlMan1` | R/W | % | 0-50, stap 5 | ✅ |
| MAN2 Debiet | 0x07 `FlowLvlMan2` | R/W | % | 10/15-100, stap 5 | ✅ |
| MAN3 Debiet | 0x08 `FlowLvlMan3` | R/W | % | 50-100, stap 5 | ✅ |
| Tijd | 0x09 `TimeMan` | R/W | minuten | 5-720, stap 5 | ✅ |

Het bereik en de stapgrootte komen **van de box zelf**: de leesreply van service 6 levert waarde,
stap, minimum en maximum mee. De ondergrens van MAN2 verschilt per nodetype (10 of 15).

## Vraaggestuurd

| Naam | node-param | R/W | eenheid | bereik | zekerheid |
|---|---|---|---|---|---|
| AUTO Debiet Min | 0x00 `FlowLvlAutoMin` | R/W | % | 10-100, stap 5 | ✅ |
| AUTO Debiet Max | 0x01 `FlowLvlAutoMax` | R/W | % | 30-100, stap 5 | ✅ |

---

# Per node — overige instellingen

Niet in het Box info-blok, maar wel per node beschikbaar via service 6.

| parameter | node-param | R/W | eenheid | nodes | zekerheid |
|---|---|---|---|---|---|
| `Co2SetPoint` | 0x03 | R/W | ppm, 0-2000 stap 10 | CO2-sensoren | ✅ |
| `Co2DetMode` | 0x1A | R/W | 0 = Standard Air Quality, 1 = Toilet Detection | CO2-sensoren | ✅ |
| `TempDepEnable` | 0x0E | R/W | 0/1 | CO2-sensoren | ✅ |
| `ShowSensorLvl` | 0x12 | R/W | %, 0-100 stap 5 | CO2-sensoren | ✅ |
| `SwitchMode` | 0x0A | R/W | 0 = Overrule, 1 = Heatpump, 2 = Presence | 52 (SWITCH) | ✅ |
| `FlowLvlSwitch` | 0x0B | R/W | %, 0-100 stap 5 | 52 | ✅ |
| `TimeOverrunSwitch` | 0x13 **of** 0x17 | R/W | minuten, 0-60 | 52 | 🟡 |

**`SwitchMode` heeft een gat.** De box meldt bereik **1..12**, maar er zijn maar drie namen bekend.
Wat 3 tot en met 12 doen is onbekend, en of waarde 0 (Overrule) geaccepteerd wordt is niet getest —
die ligt onder het gemelde minimum.

**0x13 versus 0x17** geven allebei 30 met bereik 0..60 en matchen dus allebei op
`TimeOverrunSwitch`. Eén van de twee is iets anders. Te beslechten door de nalooptijd te wijzigen
met de sniffer aan.

**Een antwoord van service 6 is geen bewijs dat de node bereikbaar is.** Een onbereikbare node
krijgt gewoon een terugvalwaarde terug, zonder foutmelding — waargenomen bij een offline CO2-sensor,
die op de `Uc*Mode`-registers de waarde van `TimeMan` teruggaf.

---

# Klok en tijdzone

| parameter | adres / hoe | R/W | eenheid | zekerheid |
|---|---|---|---|---|
| Box-tijd UTC | `0x24 sub 04` | R/W | epoch | ✅ |
| Box-tijd lokaal | `0x24 sub 02` | R | epoch | ✅ |
| TimeZone | box-param 0x0802 | R/W | uren, −11..12 | ✅ |
| Dst (zomertijd) | box-param 0x0806 | R/W | 0/1 | ✅ |

De tijdzone zit ook in byte 9 van het `0x26`-antwoord op `sub 02`/`sub 04`, en zomertijd in byte 10.
Negatieve tijdzones worden als tweecomplement geschreven; gecontroleerd met −10, −11 en +1.

---

# Cloud

| parameter | adres / hoe | R/W | eenheid | zekerheid |
|---|---|---|---|---|
| Cloudverbinding | identity `g2/f06` | R | 0/1 | ✅ |

Dit veld stuurt `CLOUD: ACTIVE` op het boxdisplay. Bewezen door de verbinding twee keer om te
schakelen met `StopAzureConnection` en `RestartAzureConnection`; het veld volgde binnen 6 tot 9
seconden.

**Valkuil:** `RegistrationMode` en `AllowRemoteIntervention` op False zetten verbreekt de verbinding
**niet** — dat blokkeert alleen sturing vanuit de cloud.

---

# Wat er niet over de bus gaat

Compleet nagetrokken met een sweep van alle 28 REST-endpoints (104 verzoeken):

- `Board.Time`, `Board.TimeUtc`, `Board.UpTime`, `ProductIdComm`, `WriteReqCntRemain` — dit zijn
  waarden van de **communicatieprint zelf**, niet van de box.
- `TimeDucoClientIp`.
- `IaqCo2` — de tool toont hem (100 bij node 4), maar hij is nergens op de bus teruggevonden.

En de **Duco Network Tool en Installer Service Tool zetten niets op deze bus**: van alle 196
verzoeksleutels in een opname van elf minuten was er geen enkele nieuw. Ze praten via USB
rechtstreeks met de box, langs de communicatieprint om.

---

## Bronnen

- De opnames in [`sniffs/`](sniffs/), met per bestand de uitgevoerde actie.
- De volledige adressentabel in [`components/duco/duco_params.h`](components/duco/duco_params.h)
  (516 namen, waarvan de meeste nooit op deze installatie zijn waargenomen).
- Het onderzoeksverslag in [`OVERDRACHT-chat-samenvatting.md`](OVERDRACHT-chat-samenvatting.md).
- Wat nog openstaat in [`duco-testplan-openstaand.md`](duco-testplan-openstaand.md).
