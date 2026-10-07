#pragma once
// Duco error-code explanations (NL), from "Information sheet - Error codes
// mechanical ventilation" L2003312-D (13.08.2025), pages 2-6.
// Codes print as E<a>.<b>.<c> (bus bytes 38 1F 01 -> E56.31.1).

namespace esphome {
namespace duco {

struct DucoErrDef {
  uint8_t a, b, c;
  const char *txt;
};

static const DucoErrDef DUCO_ERROR_INFO[] = {
    {1, 4, 1, "Geen reactie van de afvoerventilator; controleer/vervang de ventilator"},
    {28, 4, 1, "Geen reactie van de toevoerventilator; controleer/vervang de ventilator"},
    // temperature sensors ODA/ETA/EHA/SUP
    {3, 1, 3, "ODA-temperatuursensor niet uitleesbaar; controleer/vervang de sensor"},
    {3, 1, 5, "ODA-temperatuursensor kortgesloten; controleer/vervang de sensor"},
    {3, 1, 6, "ODA-temperatuursensor heeft een slechte verbinding; controleer/vervang de sensor"},
    {4, 1, 3, "ETA-temperatuursensor niet uitleesbaar; controleer/vervang de sensor"},
    {4, 1, 5, "ETA-temperatuursensor kortgesloten; controleer/vervang de sensor"},
    {4, 1, 6, "ETA-temperatuursensor heeft een slechte verbinding; controleer/vervang de sensor"},
    {6, 1, 3, "EHA-temperatuursensor niet uitleesbaar; controleer/vervang de sensor"},
    {6, 1, 5, "EHA-temperatuursensor kortgesloten; controleer/vervang de sensor"},
    {6, 1, 6, "EHA-temperatuursensor heeft een slechte verbinding; controleer/vervang de sensor"},
    {30, 1, 3, "SUP-temperatuursensor niet uitleesbaar; controleer/vervang de sensor"},
    {30, 1, 5, "SUP-temperatuursensor kortgesloten; controleer/vervang de sensor"},
    {30, 1, 6, "SUP-temperatuursensor heeft een slechte verbinding; controleer/vervang de sensor"},
    // pressure sensors
    {16, 1, 3, "Druksensor afvoerlucht niet uitleesbaar; controleer/vervang de sensor"},
    {29, 1, 3, "Druksensor toevoerlucht niet uitleesbaar; controleer/vervang de sensor"},
    // maintenance / environment
    {31, 19, 1, "Filters vuil; vervang de filters"},
    {32, 1, 1, "Buitentemperatuur te laag: ventilatie 8 uur uitgeschakeld"},
    {33, 10, 1, "Buitentemperatuur te laag: ventilatie permanent uit; herstart de DucoBox later"},
    // control/room sensors
    {35, 17, 1, "Waarde temperatuursensor te laag; controleer/vervang de sensor"},
    {35, 17, 2, "Waarde temperatuursensor te hoog; controleer/vervang de sensor"},
    {35, 17, 3, "Temperatuursensor niet uitleesbaar; controleer/vervang de sensor"},
    {36, 24, 1, "Verbinding met het weerstation verbroken; controleer de verbinding"},
    {36, 24, 2, "Verbinding met de buitentemperatuursensor verbroken; controleer de verbinding"},
    {36, 24, 3, "Verbinding met de motor van de DucoGrille Close 105 verbroken"},
    {36, 24, 6, "Verbinding met de Duco ExtensionHub verbroken; controleer/vervang de hub"},
    {37, 17, 1, "Waarde CO2-sensor te laag; controleer/vervang de sensor"},
    {37, 17, 2, "Waarde CO2-sensor te hoog; controleer/vervang de sensor"},
    {37, 17, 3, "CO2-sensor niet uitleesbaar; controleer/vervang de sensor"},
    {40, 17, 1, "Waarde vochtsensor te laag; controleer/vervang de sensor"},
    {40, 17, 2, "Waarde vochtsensor te hoog; controleer/vervang de sensor"},
    {40, 17, 3, "Vochtsensor niet uitleesbaar; controleer/vervang de sensor"},
    // installation counts
    {42, 22, 1, "Te veel kleppen aangesloten; zie handleiding voor het maximum"},
    {42, 22, 2, "Te weinig kleppen aangesloten (minimum is 2)"},
    {42, 22, 3, "Te veel regelkleppen aangesloten (maximum is 11)"},
    {42, 22, 4, "Te weinig regelkleppen aangesloten (minimum is 2)"},
    {42, 22, 5, "Te veel CO2-regelkleppen aangesloten (maximum is 7)"},
    // adjustment / calibration
    {43, 23, 1, "Inregeling nog niet gedaan of mislukt; voer de inregeling (opnieuw) uit"},
    {43, 23, 2, "Gewenste debiet niet haalbaar; controleer de volledige installatie"},
    {43, 23, 11, "Configuratie onjuist: som van toevoerdebiet groter dan maximum; inregeling opnieuw"},
    {43, 23, 12, "Kalibratie onderbroken door handmatige actie op het display; inregeling opnieuw"},
    {43, 23, 13, "Fout bij berekenen van de weerstand; controleer installatie, inregeling opnieuw"},
    {43, 23, 14, "Fout tijdens kalibratie; controleer installatie, inregeling opnieuw"},
    {43, 23, 15, "Inregeling nog niet gedaan of mislukt; voer de inregeling (opnieuw) uit"},
    {43, 23, 16, "Inregeling onderbroken via Duco Installation App of API; inregeling opnieuw"},
    {43, 23, 17, "Inregeling onderbroken via de DNT-tool; inregeling opnieuw"},
    {43, 23, 18, "Inregeling onderbroken door activeren installateursmodus; inregeling opnieuw"},
    {43, 23, 19, "Time-out opgetreden; voer de inregeling opnieuw uit"},
    {43, 23, 20, "Configuratie bevat fouten: te veel/te weinig multizone-regelkleppen aangesloten"},
    {43, 23, 21, "Kalibratie onderbroken door ontbrekende druksensordata; controleer/vervang de sensor"},
    // accessories / weather
    {44, 6, 1, "Geen verbinding met de pre-heater; controleer de verbinding"},
    {46, 1, 3, "Buitentemperatuur niet beschikbaar; controleer verbinding en gekoppelde sensoren"},
    {47, 1, 3, "Windsnelheid niet beschikbaar; controleer verbinding en gekoppelde sensoren"},
    {48, 1, 3, "Windrichting niet beschikbaar; controleer verbinding en gekoppelde sensoren"},
    {49, 1, 3, "Lichtinval niet beschikbaar; controleer verbinding en gekoppelde sensoren"},
    {50, 1, 3, "Neerslag niet beschikbaar; controleer verbinding en gekoppelde sensoren"},
    // DucoGrille Close motor
    {51, 26, 1, "Motor DucoGrille Close buiten werkbereik; controleer/vervang de motor"},
    {51, 26, 2, "Motor DucoGrille Close 105 mechanisch overbelast; controleer op obstructies"},
    {51, 26, 4, "Verbinding met motor DucoGrille Close 105 verbroken; controleer de verbinding"},
    {51, 26, 5, "Tandwiel van motor DucoGrille Close 105 losgekomen; controleer drukknop/vervang motor"},
    {51, 26, 6, "Interne activiteit bezig; motor tijdelijk niet in normale modus (spanning eraf en herstart)"},
    // weather protection (info)
    {52, 27, 1, "Weerbescherming geactiveerd door regen"},
    {52, 27, 2, "Weerbescherming geactiveerd door wind"},
    {52, 27, 3, "Het weerstation registreert geen data; controleer de installatie"},
    {52, 27, 4, "Weerbescherming geactiveerd door wind en regen"},
    {52, 27, 5, "Waarde temperatuursensor al lang constant; controleer/vervang de sensor"},
    {52, 27, 6, "Weerbescherming geactiveerd door externe logica"},
    {52, 27, 7, "Weerbescherming van een groep van dit component is geactiveerd"},
    // motor control
    {53, 28, 1, "Motor geblokkeerd door handmatige actie"},
    {53, 28, 2, "Motor reageert niet op handmatige actie; controleer communicatie van de bediening"},
    {53, 28, 3, "Motor kan niet automatisch worden aangestuurd (ontbrekende data); controleer installatie"},
    {54, 29, 1, "Verbinding met de motor verbroken; controleer/vervang de motor"},
    {54, 29, 2, "De motor meldt een fout; controleer de installatie"},
    {54, 29, 3, "Gewenste motorpositie niet haalbaar; controleer/vervang de motor"},
    {55, 30, 1, "Buitentemperatuursensor niet uitleesbaar; controleer/vervang de sensor"},
    // network components
    {56, 31, 1, "Fout met een Duco RF-component; controleer/vervang de component"},
    {56, 31, 2, "Fout met een Duco Wired-component; controleer/vervang de component"},
    // control logic
    {57, 32, 1, "Fout in de aansturing van de ventilatieve koeling; controleer de installatie"},
    {58, 32, 1, "Fout in de aansturing van de zonwering; controleer de installatie"},
    // DucoGrille Close 105 temperature sensor
    {60, 34, 1, "Waarde temperatuursensor DucoGrille Close 105 te hoog; controleer/vervang de sensor"},
    {60, 34, 2, "Waarde temperatuursensor DucoGrille Close 105 te laag; controleer/vervang de sensor"},
    {60, 34, 3, "Temperatuursensor DucoGrille Close 105 al minstens 1 uur te laag"},
    {60, 34, 4, "Temperatuursensor DucoGrille Close 105 al minstens 1 dag te laag"},
    {60, 34, 5, "Temperatuursensor DucoGrille Close 105 al minstens 1 week te laag"},
    // configuration
    {61, 35, 1, "De voorbereide configuratie bevat fouten; controleer de configuratie"},
    {61, 35, 2, "Configuratiefout: een of meer componenten niet aan een groep gekoppeld"},
    {61, 35, 3, "Configuratiefout: een of meer componenten aan meerdere groepen gekoppeld"},
    {62, 36, 1, "De tijd is niet ingesteld; bepaalde functies zijn niet beschikbaar. Stel de tijd in"},
};

inline const char *duco_error_info(uint8_t a, uint8_t b, uint8_t c) {
  for (auto &e : DUCO_ERROR_INFO)
    if (e.a == a && e.b == b && e.c == c) return e.txt;
  // fall back to the family (a.b.*) when the exact variant is not listed
  for (auto &e : DUCO_ERROR_INFO)
    if (e.a == a && e.b == b) return e.txt;
  return nullptr;
}

}  // namespace duco
}  // namespace esphome
