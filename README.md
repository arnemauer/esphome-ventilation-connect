# ESPHome component voor de DucoBox

Externe ESPHome-component die de **interne UART-bus** van een DucoBox uitleest en
aanstuurt. De bus loopt tussen de ventilatiebox en de originele communicatieprint;
deze component kan meelezen, zelf uitvragen, of de communicatieprint volledig
vervangen.

Ontwikkeld en getest op een **DucoBox Energy Comfort Plus D450 R** met een
WT32-ETH01. Andere Duco-toestellen gebruiken dezelfde bus, maar de
parameteradressen zijn alleen op dit model geverifieerd.

> Dit is reverse-engineering, geen officiële documentatie. Niet gelieerd aan Duco.

## Wat het kan

- **Lezen** — temperaturen, drukken, toerentallen, debieten, CO2, filterstand,
  per node en per zone. 123 box-parameters zijn op de bus geverifieerd en op naam
  op te vragen.
- **Schrijven** — ventilatiestand (inclusief de MAN1x2 t/m MAN3x3-varianten),
  node-instellingen zoals CO2-instelpunt en handbedieningsduur, de klok van de
  box, netwerkconfiguratie.
- **Acties** — zone identificeren, filtertimer resetten, install mode, node
  herstarten, link mode.
- **Communicatieprint emuleren** — met `comm_responder: true` beantwoordt de ESP
  de identiteitshandshake, zodat het boxdisplay het LAN-menu toont en de box denkt
  dat zijn eigen print aangesloten is.
- **Sniffen** — twee ontvangers tussen box en print om het verkeer te ontleden
  zonder in te grijpen.

## Aansluiten

De bus is **57600 8N1** op de connector van de communicatieprint.

| ESP | bus |
|---|---|
| TX | RX van de box |
| RX | TX van de box |
| GND | GND |

**Let op bij meeluisteren naast de echte print.** ESP-IDF forceert in
`uart_set_pin()` een interne pull-up op de RX-pin, en ESPHome draait dat niet
terug. Op een gedeelde lijn verstoort dat de communicatie: de echte print kan de
box dan niet meer lezen. Gebruik voor sniffen **GPIO34-39** — dat zijn
input-only pinnen zonder interne pull-up.

## Installeren

```yaml
external_components:
  - source:
      type: local
      path: components
    components: [duco]
```

Of rechtstreeks vanaf GitHub:

```yaml
external_components:
  - source: github://<gebruiker>/esphome-duco@main
    components: [duco]
```

## Minimale configuratie

```yaml
uart:
  id: duco_uart
  tx_pin: GPIO17
  rx_pin: GPIO33
  baud_rate: 57600
  rx_buffer_size: 256

duco:
  id: my_duco
  uart_id: duco_uart
  update_interval: 15s

sensor:
  - platform: duco
    duco_id: my_duco
    param: Ventilation.Fan.PressSup
    name: "Toevoerdruk"
    unit_of_measurement: "Pa"
    accuracy_decimals: 1

select:
  - platform: duco
    duco_id: my_duco
    node: 1
    name: "Ventilatiestand"
```

Zie [`example.yaml`](example.yaml) voor een volledige configuratie met alle
geverifieerde parameters, in dezelfde volgorde als
[`docs/parameters-per-module.md`](docs/parameters-per-module.md).

## Parameters opvragen

Parameters hebben een **volledig API-pad** als naam, overgenomen uit `/info` en
`/config` van de communicatieprint:

```yaml
param: Ventilation.Fan.PressSup      # volledig pad, altijd eenduidig
param: PressSup                      # bladnaam, mag als die maar EEN keer bestaat
```

Namen als `State`, `Mode` en `NodeIn` komen in meerdere modules voor en zijn
daarom alleen via het volledige pad bereikbaar.

### Waarom er twee adressentabellen zijn

`duco_params.h` is een generieke parameterlijst en dekt 463 parameters, maar is
voor dit toestel **bij vier modules aantoonbaar onjuist** (0x00, 0x03, 0x0A,
0x13). Een naam opzoeken in die tabel levert daar het verkeerde register op,
zonder foutmelding.

`duco_verified.h` bevat alleen adressen die **daadwerkelijk op de bus zijn
waargenomen**, en wordt als eerste geraadpleegd. Dat bestand wordt gegenereerd
uit `docs/parameters-per-module.md`, zodat documentatie en code niet uit elkaar
kunnen lopen.

## Drie adresruimtes

| ruimte | vraag | antwoord | in de yaml |
|---|---|---|---|
| box-parameter | `0x24 00 <lo> <hi>` | `0x26 01 <lo> <hi> <4 bytes>` | `param:` / `bus_id:` |
| node-parameter (service 6) | `0x18 00 <node> <param>` | `0x1A 01 <val> <inc> <min> <max>` | `type:` + `node:` |
| identity (print ↔ box) | handshake `0x20` / `0x22` | groep/veld `g<n>/f<n>` | `type: comm_*` |

**Twee valkuilen bij schrijven.** Box-parameters gaan met `0x24 01` en zijn
**little-endian**; node-parameters met `0x18 01` en zijn **big-endian**, terwijl
de leesreply van diezelfde service weer little-endian is. Bij waarden onder 256
valt dat niet op, want dan verschilt alleen de plek van de nulbyte.

## Frameformaat

```
AA 55 | LEN | FUNC | ID | DATA... | CRC16(MODBUS, little-endian)
```

- `LEN` telt `FUNC` + `ID` + `DATA`
- CRC over `LEN` + `FUNC` + `ID` + `DATA`
- byte-stuffing: elke `0xAA` binnen `LEN..CRC` krijgt een `0x01` erachter; die
  filler telt niet mee in `LEN` en niet in de CRC
- `FUNC = service × 4`; de onderste twee bits onderscheiden verzoek, ack en reply

## Documentatie

| bestand | inhoud |
|---|---|
| [`docs/parameters-per-module.md`](docs/parameters-per-module.md) | alle parameters per module, met busadres, eenheid en hoe zeker het is |
| [`docs/parameters-by-screen.md`](docs/parameters-by-screen.md) | dezelfde gegevens, geordend als de schermen van de Installer Service Tool |
| [`docs/programs-and-zones.md`](docs/programs-and-zones.md) | weekprogramma's en zone-/groepsindeling |

## Werkwijze bij verder onderzoek

In [`tools/`](tools) staan de scripts waarmee de parameters zijn achterhaald.
Twee dingen die bij dit protocol steeds misgaan:

1. **Zoek nooit met een regex naar frames binnen losse logregels.** Frames breken
   over regelgrenzen; dan match je op een halve staart plus het begin van het
   volgende frame. Bouw eerst de bytestroom per richting op en unstuf
   `AA 01` → `AA` (`ident_trace.py`).
2. **Koppel vraag en antwoord op frame-id, niet op volgorde.** Binnen één seconde
   is de volgorde van een samengevoegde lijst willekeurig. Uitzondering: bij
   service 9 delen vraag en antwoord het frame-id *niet* — daar echoot het
   antwoord het parameternummer zelf terug.

De betrouwbaarste methode om een parameter te vinden: wijzig hem in de
webinterface terwijl je sniffer loopt, en **zet hem daarna meteen terug**. Dat
levert twee onafhankelijke waarnemingen per adres op, en je installatie blijft
zoals hij was.

## Waarschuwingen

- `reset_config` zet de configuratie van een node terug naar fabriekswaarden. Er
  is geen commando om dat terug te draaien.
- Alles behalve `STOP` bij `calibration_state` start een echte inregelprocedure
  die de ventilatoren op uitersten laat draaien.
- `link_mode` werkt alleen met de box in install mode; daarbuiten weigert de
  print het commando.

## Licentie

**GPLv3** — zie [LICENSE](LICENSE). Dat geldt voor de hele repository: de
component, de documentatie en de scripts.
