# Lexus NX300h OBD2 / TPMS Telemetria Kijelző

## Projekt leírása
Ez a projekt egy egyedi építésű, hardveres telemetria kijelző, amely kifejezetten a Lexus NX300h hibrid járművekhez készült. Az eszköz egy ESP32 mikrokontrollerre épül, és Bluetooth kapcsolaton keresztül, Master (Mester) módban kommunikál az autóba csatlakoztatott ELM327 OBD2 diagnosztikai adapterrel. 

A műszer egy 4 állású, nyomógombokkal vezérelhető (MVVM architektúrájú) menürendszerrel rendelkezik. Fő funkciói közé tartozik a hibrid akkumulátor hőmérsékletének valós idejű megjelenítése, a TPMS (guminyomás) adatok kerekenkénti monitorozása (2 tizedesjegy pontossággal), valamint a jármű hibakódjainak (DTC) olvasása és törlése. A fizikai megvalósítás fókuszában a megbízhatóság és a "Clean Code" analógiájára épülő, kereszteződésmentes nyáklap-tervezés (*Nested Routing*) állt.

### Képernyőképek
![TinkerCAD Szimuláció](ide_jon_a_tinkercad_kep_urlje.png)
*Logikai szimuláció a TinkerCAD környezetben*

![Valós Eszköz](ide_jon_a_valos_eszkoz_kepe_urlje.jpg)
*A megépített fizikai hardver az OLED kijelzővel*

---

## Projekt elérhetősége
A projekt logikai szimulációja és a menürendszer alapjainak tesztelése a TinkerCAD platformon készült.

* **TinkerCAD Projekt:** [Kattints ide a szimuláció megtekintéséhez](ide_jon_a_tinkercad_link_urlje)

---

## Eltérés a TinkerCAD és a fizikai megépítés között
Mivel a TinkerCAD környezet limitált hardveres szimulációs képességekkel rendelkezik, a logikai modell és a valós, megépített műszer között az alábbi kritikus mérnöki eltérések vannak:

1. **A Mikrokontroller:** A TinkerCAD szimulációban egy szabványos **Arduino Uno** futtatja a kódot, míg a fizikai megépítéshez egy sokkal erősebb, beépített rádióval rendelkező **ESP32 (Lolin32 Lite klón)** lett felhasználva.
2. **Az Adatkapcsolat (Bluetooth vs. Serial):** A szimulátorban a gépjármű (OBD2) válaszait a Soros Monitoron (Serial) keresztüli manuális karakter-beküldés helyettesíti. A valós eszközben egy dedikált `BluetoothSerial` kapcsolat fut, amely MAC-cím alapú azonosítással és PIN-kóddal csatlakozik az ELM327 adapterhez.
3. **Lábkiosztás és I2C Routing:** A szimulátor alapértelmezett digitális lábakat használ. A valós nyomtatott áramkörön a rövidzárlatok elkerülése és az optimális ón-ösvények (Solder Bridging) kialakítása érdekében egyedi lábkiosztást alkalmaztunk. A nyomógombok a *Nested Routing* elv alapján az ESP32 `5`, `18`, és `23`-as lábaira kerültek, a kijelző pedig Szoftveres I2C-vel (`19` SCK, `22` SDA) csatlakozik, áthidalva a fizikai kereszteződéseket.

---

## 🛠️ Alkatrész lista
A fizikai műszer megépítéséhez az alábbi komponensek szükségesek:

* **1 db** ESP32 fejlesztői lapka (Wemos Lolin32 Lite klón, 26 lábú kivitel)
* **1 db** 1.3" (vagy 0.96") OLED Kijelző (I2C kommunikáció, 4 tűs: VCC, GND, SCK, SDA)
* **3 db** 2 lábú mikrokapcsoló (Nyomógomb a menü vezérléséhez)
* **1 db** Előfúrt nyomtatott áramköri próbapanel (Perfboard)
* Szilárd erű vezeték ("Mikro-gerinc" és "Felüljáró" a tápellátáshoz) és forrasztóón
* **1 db** ELM327 Bluetooth OBD2 olvasó adapter (A gépjármű diagnosztikai portjához)

---

## 💻 Forráskód
Az alábbi kód tartalmazza a menürendszert, a TPMS dekódolást és a Bluetooth kommunikációt.

```cpp
// IDE ILLLESZD BE A VÉGLEGES FORRÁSKÓDOT
// Másold be ide a kódot, és a fenti és lenti "```" jelek gondoskodnak a színezésről!
