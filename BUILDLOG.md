---

# Projektsummering: Minimalistisk digital skrivmaskin

## 1. Hårdvarukonfiguration

Efter utvärdering har vi spikat följande hårdvaruuppsättning, med fokus på att använda de delar du redan äger och att maximera drifttiden:

* **Processor:** Raspberry Pi Zero W (v1). Vald framför Zero 2 W eftersom den har fastlödda headers färdiga att använda, samt en extremt låg strömförbrukning (ARMv6-arkitektur).
* **Skärm:** 6-tums e-bläcksskärm (HD, upplösning $1448 \times 1072$) ansluten via ribbon-kablar till en dedikerad **IT8951 Driver HAT**, som i sin tur pratar med din Pi via **SPI**.
* **Strömförsörjning:** Adafruit PowerBoost 1000C (för stabil 5.2V-matning och laddning under drift) parad med ett massivt **8000 mAh LiPo-batteri** (vilket ger uppskattningsvis 40–60+ timmars skrivtid).
* **Inmatning:** Ett färdigt 65% mekaniskt tangentbord som ansluts via USB (Micro-USB OTG till din Pi Zero) och skickar standard key-koder.
* **Kontroller:** * En taktil strömbrytare (switch) kopplad till **EN (Enable)**-stiftet på PowerBoost för att helt bryta strömmen från batteriet vid avstängt läge (förhindrar urladdning).
* En separat tryckknapp kopplad till GPIO för att trigga ett "Safe Shutdown"-skript och hantera asynkron synk.

---

## 2. Projektprioriteringar (Kärnfokus)

För att projektet ska vara intressant och framgångsrikt måste vi kompromisslöst prioritera följande:

1. **Lägsta möjliga latens:** Tangenttryck till skärmrespons måste kännas omedelbart. Skärmen ska hänga med även under snabba "bursts" i skrivandet (upp till 80 ord i minuten / ~6.7 tecken per sekund).
2. **Extrem strömsnålhet:** WiFi och onödiga processer ska hållas helt avstängda under skrivfasen. Endast rå inmatning och skärmdrivning får belasta den enkärniga ARMv6-processorn.
3. **Säkerhet för data:** All text ska skrivas kontinuerligt till SD-kortet för att förhindra förlust vid plötsligt spänningsfall, samt synkas säkert och självständigt mot en NAS eller till Dropbox.

---

## 3. Mjukvaru- och Arkitekturbeslut

För att uppnå våra mål och undvika flaskhalsar på ARMv6-arkitekturen har vi tagit följande strategiska beslut för mjukvaran:

### Inmatning & Logik (C-baserat "Bare-Metal"-tänk)

* Vi använder C. Vi förkastar Python (och bibliotek som Pillow) för själva renderingsloopen. Python är för långsamt för att beräkna bounding boxes och skjuta bilddata på en 1 GHz ARMv6-processor utan märkbar latens.
* Vi bygger en **C-baserad renderingsmotor** som körs direkt mot Waveshares officiella C-bibliotek (bcm2835-baserat) för att styra IT8951 direkt via SPI.
* **Glyph Caching:** Vid uppstart renderar C-programmet ditt önskade typsnitt (motsvarande storlek 10–12 i Word, vilket på HD-skärmen motsvarar cirka 45–50 pixlar höga bokstäver) och sparar dem som monokroma ($32 \times 64$px) bitmapps-arrayer direkt i RAM-minnet (`font.h`). Beroende på hur eink-skärmen fungerar kan det vara nödvändigt med 32px eller 64px. Möjligen med padding. Just nu finns en renderad ascii-fil: lib/Fonts/font24.c
* När en tangent trycks ned läser `evdev` av detta. Programmet gör en blixtsnabb `memcpy` av rätt bokstavs-bitmapp och skickar endast den lilla förändrade rutan (damage box, ca 256 bytes) över SPI.

### Skärmstyrning (IT8951-optimering)

* **Skrivläge (A2-mode):** Under aktivt skrivande körs skärmen i det monokroma, asynkrona **A2 (Animation)-läget**. Detta minimerar latensen till under 50 ms.
* **Tyst städning (DU-mode):** Vid naturliga pauser, som vid tryck på `Enter` (ny rad), ska den aktuella raden eller skärmen uppdateras i det skarpare **DU (Direct Update)-läget** för att rensa bort eftersläpningar (ghosting).
* **Helskärms-refresh:** En fullständig nollställning (flash) görs endast vid stora händelser (t.ex. sidbyte eller manuellt knapptryck).

### Nätverk & Synk (Tailscale + NAS)

* **Helt självständig enhet:** Skrivmaskinen sköter allt själv på kommando, utan att du behöver interagera med din Linux Mint-dator.
* **Säker tunnel via Tailscale:** Vi installerar Tailscale på din Pi Zero W (fullt kompatibelt med ARMv6). Den tunnlar sig automatiskt och säkert in till din NAS så fort nätverket aktiveras.
* **Okrypterad Rsync (Ingen dubbelkryptering):** Eftersom Tailscale (WireGuard) redan krypterar nätverkstrafiken, kör vi en **okrypterad `rsync` direkt mot din NAS rsync-port (873)** över Tailscale-IP:n. Detta sparar massor av CPU-resurser och batteri på din Pi då vi helt slipper SSH-kryptering ovanpå VPN-krypteringen.
* **On-demand sekvens (Synk-knapp):**
1. Slå på WiFi (`rfkill unblock`).
2. Vänta på anslutning till Tailscale-nätverket.
3. Kör okrypterad Rsync-push till din NAS.
4. Slå av WiFi helt och hållet (`rfkill block`).


## Minnesanteckningar ##

Vi har beslutat oss för att använda

* OS: Raspberry Pi OS Bullseye (Debian 11)
* Hårdvara: Pi Zero W med IT8951-drivkort. Just nu monterad som en shield eller hat. Bör senare kopplas till PINs på RPi för att vi ska komma åt oanvända PINs. All hårdvara fungerar, just nu drivs den på batteri.
* Mjukvara: Waveshares nya bibliotek i ~/IT8951-ePaper/Raspberry.
* Drivrutin: Standard-SPI via det inbyggda BCM2835-biblioteket (kompilerat med enbart sudo make -j4).

# Projektlogg: writerDeck – Statusuppdatering

Detta dokument beskriver nuläget för *writerDeck* efter dagens utvecklingsinsats.

## Nuläge: Från "låst" till "responsiv"
Efter dagens arbete har vi tagit ett avgörande kliv framåt i projektets mjukvaruarkitektur. Vi har gått från en initieringsprocess som låste sig vid SPI-kommunikation till en stabil och fungerande uppkoppling mellan Raspberry Pi Zero och IT8951-kontrollern.

### Vad vi har uppnått idag:
1. **Kompilering och drivrutiner:** Vi har integrerat Waveshares `Makefile`-logik och säkerställt att rätt flaggor (`-D BCM`) skickas till kompilatorn. Detta aktiverade de nödvändiga SPI-drivrutinerna för BCM2835-biblioteket.
2. **Hårdvaruinitiering:** Vi har framgångsrikt implementerat `EPD_IT8951_Init` och hämtat korrekt skärminformation (1448x1072, firmware-version, VCOM-värde) direkt från hårdvaran.
3. **Rendering i gång:** Vi har löst strukturella problem i `main.c` kring variabelskop och funktionsanrop. Programmet kan nu trigga renderingsanrop vid varje tangenttryckning, och en visuell reaktion (fyrkant/bounding box) syns på e-bläcksskärmen för varje tryck.

## Identifierade tekniska utmaningar
* **Renderingens innehåll:** För närvarande ritas endast en "bounding box" (fyrkant). Vi har bekräftat att kommunikationen fungerar, men behöver nu finjustera hur själva tecken-bitmappen läses och skickas till skärmen.
* **Datatyp och format:** Det råder en diskrepans mellan fontens lagringsformat (bitmapp) och vad IT8951-kontrollern förväntar sig i `Packed Pixel`-läget.

## Nästa steg (Att göra)
1. **Analys av bitmap-data:** Verifiera innehållet i `bitmap`-bufferten innan den skickas för rendering. Vi behöver säkerställa att teckendata (`A`, `B`, `C` osv.) faktiskt finns representerade som 1:or och 0:or i minnet.
2. **Finjustering av `render_char_fast`:**
    * Testa byte-ordningen och pixelformatet (växla mellan `1BPP` och `8BPP` i anropen).
    * Kontrollera att font-tabellens offset pekar korrekt på den valda bokstaven.
3. **Implementera partiell uppdatering:** När tecknet ritas korrekt, säkerställa att vi endast anropar uppdatering för den specifika rektangeln (istället för att flasha hela skärmen) för att hålla latensen nere.
4. **Tangentbordsinmatning:** Koppla samman `evdev`-logiken med den nu fungerande renderingsfunktionen.

---
*Loggen upprättad 2026-07-20.*

# Referens #

### Git ###

Publikt git-repo för detta projekt f inns: https://github.com/Wim4rk/wimwriter
Sent i processen ska vi göra detta offentligt så att andra som vill bygga en writerdeck får fler idéer och lösningar.

Andra git-repon där e-bläckskärmar används. Till exempel:
* ZeroWriter
* PaperTTY
* Med flera

### Tangentbords-översättaren (Scancode till ASCII): ###
* Vi ska bygga en rå översättningstabell (if/else-block) som mappar Linux scancodes (t.ex. 30 för tangenten 'A') till tecken. Vi har förberett för de svenska tecknen Å, Ä, Ö genom att mappa dem till specifika index där de förväntas ligga i fontmatrisen.

### Specar för e-bläckskärmen ###
https://www.waveshare.com/wiki/6inch_HD_e-Paper_HAT?srsltid=AfmBOorzHeeWp2Pp1WDUMXZvQR7rJ5uvXGIq74KlfZNBFP9B932APp6L
6inch e-Paper HAT uses IT8951 as a controller, it can be controlled by USB/SPI/I80/I2C interface with the resolution of 1448 × 1072, 6inch EPD (Electronic Paper Display) display.

* Operating voltage: 5V
* Interface: USB/SPI/I80/I2C
* Outline dimension: 138.4mm X 101.8mm X 0.67mm
* Display size: 122.356mm X 90.584mm
* Dot pitch: 0.0845mm X 0.0845mm
* Resolution: 1448 X 1072
* Display color: black, white
* Grayscale: 2-16 (1-4 bit)
* Full refresh time: <1s
* Refresh power: 0.6W (typ.)
* Standby power: 0.3W (typ.)
* Viewing angle: >170°
* Operating temperature: 0 ~ 50 ℃
* Storage temperature: -25 ~ 70 ℃

### Input ###

Keyboarden benämns /dev/input/event0. Det är en TADA68.

---
