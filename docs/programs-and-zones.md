# Programma's en zones over de bus (0x24 sub 0x0B/0x0C/0x0F/0x15 en 0x11/0x12/0x13)

Gedecodeerd op 18-08 uit bestaande captures — **geen printwissel nodig**, de box praat deze
service gewoon tegen onze ESP.

Bronnen, telkens bus én REST naast elkaar gelegd:
- `duco_frames prog.csv` + `192.168.1.50 chrome log programm.har` (`/programs`)
- `duco_frames zone.csv` + `192.168.1.50 chrome log zone.har` (`/info/zones/<n>`, `/config/zones/<n>`)
- `DUCO Programs.pdf` is een screenshot zónder tekstlaag; dezelfde pagina zit als data in de HAR.

Alle waarden hieronder zijn geverifieerd: de busbytes zijn 1-op-1 teruggevonden in de
REST-respons van dezelfde box.

---

## 1. Programma's

### 0x24 sub 0x0B — programmalijst

```
verzoek:  24 <id> 0B
antwoord: 26 <id> 01 <32 bytes bitmap>
```

Waargenomen: `01 FF 00 00 ... 00` → bit 0-7 gezet = **8 programma's**, indices 0-7.
Komt overeen met de 8 items in `/programs` (daar genummerd P1-P8).

### 0x24 sub 0x0C — programma per index

```
verzoek:  24 <id> 0C <index>
antwoord: 26 <id> 01 <index> <record>
```

**Recordformaat** (offsets vanaf de statusbyte `01`):

| Offset | Veld | Codering |
|---|---|---|
| [2] | `Type` | 01=VENT_SUP, 05=NIGHT_BOOST, 06=VENT_RH_SUMMER |
| [3] | `Zone` | **0-based** (00 = UI-zone 1), FF = 255 = alle |
| [4] | `Group` | FF = 255 = alle |
| [5][6][7] | Dagen + maanden | 24-bit **LE**, zie hieronder |
| [8] | — | altijd 00 |
| [9] | `Enable` | 01 = aan, FF = uit |
| [10] | `Mode` | 01=EVERYDAY, 0C=WORKWEEK, 0D=UNDEFINED |
| [11] | aantal `Points` | 0, 1, 2, … |
| [12+] | `Points` | **3 bytes per punt**: uur, minuut, `Val` |

**Het 24-bits dag/maand-veld** ([5][6][7], little-endian):
- bit 0-1 — onbekend (staan altijd aan bij een actief programma)
- **bit 2-8 = Days**, bit 2 = Monday … bit 8 = Sunday
- **bit 9-20 = Months**, bit 9 = January … bit 20 = December

### Verificatie

| Index | Bus-record | `/programs` |
|---|---|---|
| 0 | `01 00 05 FF FF FF FF 1F 00 01 01 02 00 00 01 17 3B 00` | P1 NIGHT_BOOST, zone 255, alle dagen+maanden, Enable 1, EVERYDAY, punten 00:00→1 en 23:59→0 |
| 7 | `01 07 06 FF FF FF E1 07 00 01 01 02 00 00 01 17 3B 00` | P8 VENT_RH_SUMMER, maanden mei-okt |
| 2 | `01 02 01 00 FF 7C FE 1F 00 FF 0C 00` | P3 VENT_SUP, zone 1, ma-vr, Enable 0, WORKWEEK, 0 punten |
| 3 | `01 03 01 00 FF 80 FF 1F 00 FF 0D 00` | P4 VENT_SUP, zone 1, za-zo, UNDEFINED |

Twee sluitende controles:
- Index 7, bytes `FF E1 07` = 0x07E1FF. Bits 9-20 geven Jan=0 … Apr=0, **mei t/m okt = 1**,
  Nov=0, Dec=0 — exact `Months: 000011111100` uit de REST-respons.
- Index 0 en 7, staart `02 00 00 01 17 3B 00` = 2 punten: (00:00, 1) en (0x17=23, 0x3B=59, 0) —
  exact `Points: [{"Time":"00:00","Val":1},{"Time":"23:59","Val":0}]`.

### 0x24 sub 0x0F + 0x15 — programma schrijven (GEDECODEERD 23-08)

De HAR bevat de twee PUT's die de busframes veroorzaakten, met tijdstempels die precies
samenvallen (HAR is UTC, buslog lokaal +2). Daarmee is elk veld te herleiden.

```
PUT /programs/2  {"Enable":1,"Cnt":0,"ValCnt":255,"Days":alle,"Months":alle,
                  "Points":[{"Time":"00:00","Val":255}]}
   bus:  0F 01 FF FF 1F 00 FD FF 1F 00 01 FF
         15 01 01 01 00 00 FF

PUT /programs/2  {"Enable":0,"Cnt":0,"ValCnt":255,"Days":alle,"Months":alle,"Points":[]}
   bus:  0F 01 FF FF 1F 00 FC FF 1F 00 01 FF
         15 01 01 00
```

**`sub 0F` — header** (11 bytes na het sub-byte):

| Offset | Veld |
|---|---|
| [1] | programma-index, **0-based** (01 = P2) |
| [2..5] | **masker**, u32 LE — welke bits geschreven worden (hier 0x001FFFFF = alles) |
| [6..9] | **waarde**, u32 LE — zelfde bitindeling als het leespack |
| [10] | `Mode` (01 = EVERYDAY) |
| [11] | `ValCnt` (FF = 255) |

**De bitindeling van masker en waarde**, nu compleet:

| Bit(s) | Veld |
|---|---|
| **0** | **`Enable`** |
| 1 | onbekend (0 in alle waarnemingen) |
| 2-8 | `Days`, bit 2 = maandag … bit 8 = zondag |
| 9-20 | `Months`, bit 9 = januari … bit 20 = december |

Bewijs: PUT 1 heeft `Enable:1` en waarde 0x001FFFFD, PUT 2 heeft `Enable:0` en waarde 0x001FFFFC —
het enige verschil is bit 0. Dagen en maanden staan in beide gevallen volledig aan, wat overeenkomt
met bits 2 t/m 20 gezet.

**Dit lost meteen een open punt in de LEESrichting op:** bit 0 van het dag/maand-pack is dus ook
daar `Enable`. Controle op de leesrecords: P1 (Enable 1) heeft pack 0x1FFFFF met bit 0 = 1, P3
(Enable 0) heeft 0x1FFE7C met bit 0 = 0. Klopt in beide richtingen. Alleen bit 1 blijft onbekend.

**`sub 15` — puntenlijst**:
```
15 <idx> 01 <aantal> [<uur> <minuut> <waarde>] x aantal
```
PUT 1: `15 01 01 01 00 00 FF` = index 1, één punt: 00:00 met waarde 255.
PUT 2: `15 01 01 00` = index 1, nul punten.
Byte [2] is in beide gevallen 01; betekenis onbekend (mogelijk een deelnummer).

Beide worden beantwoord met `0x26` payload `01` = OK.

---

## 2. Zones en groepen

### 0x24 sub 0x11 — zoneparameter lezen

```
verzoek:  24 <id> 11 <zone> <index> <module> [aantal woorden]
antwoord: 26 <id> 01 <zone> <index> <module> <4 bytes>      (waarde)
          26 <id> 00 <zone> <index> <module>                 (geen waarde)
```

`<zone>` is **0-based**: 00 = UI-zone 1. Het optionele vijfde byte is het aantal 4-byte-woorden;
zonder dat byte komt er precies één woord terug.

**Modules en indices die vaststaan:**

| module/index | Parameter | Zone 1 |
|---|---|---|
| mod 02, idx 00 | `DeviceGroupConfig.General.Type` | 2 |
| mod 02, idx 01 | `DeviceGroupConfig.General.Name` (ASCII) | "VentEtaCentral" |
| mod 01, idx 02 | `DeviceGroupConfig.General.Error` | 0 |
| mod 04, idx 00 | `ActiveHouse.TempHandler.Temp` | 0 |
| mod 04, idx 01 | `ActiveHouse.TempHandler.TempFilt` | 0 |
| mod 04, idx 06 | `ActiveHouse.TempHandler.TempReqHigh` | 240 (F0) |
| mod 04, idx 07 | `ActiveHouse.TempHandler.TempReqLow` | 200 (C8) |

Zonenamen over drie zones, alle drie teruggevonden in de REST-respons:
zone 00 = "VentEtaCentral", zone 01 = "VentSupCentral", zone 02 = "VentSupZonal".

**Let op — andere byte-volgorde dan bij de identity-handshake:** zonenamen staan gewoon
voorwaarts in de bytes (`56 65 6E 74` = "Vent"), níét per 4-byte-woord omgekeerd zoals bij
`SerialBoardComm` en `HostName`.

**Module 0x80 = `ActiveHouse.DataExt`** (GEDECODEERD 23-08). De gebruiker had destijds systematisch
één veld tegelijk gewijzigd; `chrome log zone.har` bevat 21 PATCH-verzoeken op `/config/zones/1` en
de tijdstempels vallen samen met de `sub 0x12`-writes op de bus. Daarmee is elke index te herleiden:

| Index | Veld |
|---|---|
| 0x00 | `Temp` |
| 0x01 | `TempTimeValid` |
| 0x02 | `TempStatus` |
| 0x04 | `TempReqHigh` |
| 0x08 | `TempReqLow` |
| 0x09 | `TempReqTimeValid` |
| 0x0A | `TempReqStatus` |

Indices 0x03, 0x05, 0x06 en 0x07 zijn niet gewijzigd in die sessie en dus onbekend; gezien het
patroon (`Temp*` op 00-02, `TempReq*` op 04/08-0A) zitten daar vermoedelijk de Co2- of Rh-varianten.

Dezelfde correlatie bevestigt ook de al bestaande toewijzingen:

| module/index | Veld |
|---|---|
| mod 0x05, idx 00 | `ActiveHouse.TempHandler.TempReq` (221 -> 220 waargenomen) |
| mod 0x06, idx 01 | `ActiveHouse.TempHandler.TempNodeSrc` (254 -> 1 -> 255) |
| mod 0x02, idx 01-06 | `DeviceGroupConfig.General.Name`, 4 tekens per index |

**Wat dit betekent:** `DataExt` is een **externe-data-injectie**. Je kunt er een eigen temperatuur
in schrijven (`Temp` + `TempTimeValid` + `TempStatus`) en de box gebruikt die als bron. Dat is via
`0x24 sub 0x12` volledig vanaf onze ESP te doen.

### 0x24 sub 0x12 — zoneparameter schrijven

```
verzoek:  24 <id> 12 <zone> <index> <module> <4 bytes>
antwoord: 26 <id> 01 <zone> <index> <module> <4 bytes>
```

In `duco_frames zone.csv` staat een complete **hernoemactie** vastgelegd: de naam wordt in
opeenvolgende woorden geschreven op module 02, index 01-05:
`"Vent"` → `"EtaC"` → `"entr"` → `"al1\0"` → `"\0\0\0\0"`, oftewel de zone werd hernoemd naar
**"VentEtaCentral1"**. Strings worden dus per index van 4 tekens weggeschreven en met nullen
afgesloten.

### 0x24 sub 0x13 — array lezen (groepen/nodes)

```
verzoek:  24 <id> 13 <zone> <?> <index> <module> [aantal]
antwoord: 26 <id> 01 <zone> <?> <index> <module> <aantal × 4 bytes>
```

Verificatie:
- `13 00 00 01 01 01` → `… 34 00 00 00` = **node 52**. `/info/zones/1` meldt
  `Groups[0].Nodes = [52]` — de SWITCH-node. ✓
- `13 02 00 01 01 03` → `… 02 …, 04 …, 06 …` = **nodes 2, 4 en 6** in zone 3. ✓
- `13 02 00 00 02` → `08` = `Type` 8, en `/zones/3/groups/1` meldt `Type: {Val: 8}`. ✓

Het tweede byte (hier `00`) is vermoedelijk het groepsnummer; in alle waargenomen frames is het 0.

### Aantal groepen: box-parameter 0x1308

De `sub 0x13`-service heeft **geen** veld dat het aantal groepen teruggeeft. Het staat wel als
**box-parameter `0x1308`**, gevonden in de capture van een **print-herstart**
(`duco-sniff-2026-08-19_21-14-35.txt`) waar de print zijn hele configuratie eenmalig uitleest.
Module 0x13 is de groep/zone-service, dus een teller in dat blok is logisch.

Waarde: **4**. Onafhankelijk gecontroleerd door alle slots 0..7 af te tasten met
`13 <g> 00 00 02`: alleen groep 0, 1, 2 en 3 melden een niet-nul `Type` (5, 6, 8, 8); slots 4-7
geven 0. Let op: de box antwoordt met **status 01 voor élk slot**, dus de statusbyte zegt niets
over bestaan -- het `Type` doet dat.

Andere tellers uit hetzelfde blok, ook waargenomen bij de printherstart:
`0x1300` = 1, `0x1301` = 1, `0x1302` = 1, `0x1304` = 1, `0x1309` = 1. Betekenis onbekend.

**Implementatie:** één `bus_id: 0x1308`-sensor. Ik had eerst de aftastvariant gebouwd (acht extra
reads per pollcyclus) en die weer verwijderd toen deze parameter opdook -- dit is wat de echte
print zelf doet.

---

## 3. Wat hiervan nog open is

1. ~~Schrijfformaat van programma's~~ — **gedecodeerd 23-08**, zie §1. Alleen bit 1 van het
   dag/maand-veld en byte [2] van `sub 15` zijn nog onbekend.
2. ~~Module 0x80 bij zones~~ — **gedecodeerd 23-08**: het is `ActiveHouse.DataExt`, zie §2.
   Alleen de indices 0x03 en 0x05-0x07 zijn nog onbekend.
3. **Bits 0-1 van het dag/maand-veld** — betekenis onbekend.
4. **Betekenis van `Type`-codes** buiten de drie waargenomen programmatypes en de
   zone/groep-types 2, 5 en 8.
5. Niets hiervan is nog **geïmplementeerd** in de component; de frames worden alleen gedecodeerd
   en gelogd.
