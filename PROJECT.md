# Projekt wimwriter: en minimalistisk digital skrivmaskin

## Prio 1: Det synbarligen oöverstigliga problemet som måste lösas

Innan något annat kan göras måste vi kunna rendera text på skärmen. Vi behöver lösa **glyph-caching**. Några strategi-förslag utan inbördes prioritet:

### 1. En virtuell skärm i RAM
Skapa en fullstor framebuffer i RAM-minnet på Pi Zeron. Eftersom vi arbetar i 1-bpp (svartvitt) tar en skärm på $1448 \times 1072$ pixlar enbart cirka 194 KB RAM ((1448 * 1072) / 8). Detta är extremt lättdrivet även för ARMv6. Vi uppdaterarr bläckskärmen endast med förändringar.

### 2. Bounding boxes
För att uppdatera e-bläcksskärmen blixtsnabbt skickar vi inte hela skärmen (vilket vore för långsamt), utan vi gör en partiell uppdatering.
* Vi beräknar en bounding box (den rektangel som ändrats).
* Vi skickar en perfekt byte-alignerad 1-bpp-ruta till IT8951.

---

## Övriga specifikationer

### 1. Hårdvarukonfiguration

Efter utvärdering har vi spikat följande hårdvaruuppsättning, med fokus på att använda de delar du redan äger och att maximera drifttiden:

* **Processor:** Raspberry Pi Zero W (v1). Vald framför Zero 2 W eftersom den har fastlödda headers färdiga att använda, samt en extremt låg strömförbrukning (ARMv6-arkitektur).
* **Skärm:** 6-tums e-bläcksskärm HD ansluten via ribbon-kablar till en dedikerad **IT8951 Driver HAT**, som i sin tur pratar med din Pi via **SPI**. 
- VCOM = -2.14 vilket oftast noteras 2140
- Panel(W,H) = (1448,1072)
- Memory Address = 122480
- FW Version = SWv_0.6.
- LUT Version = M841_TFAB512
- VCOM = -2.14V
- A2 Mode:6

* **Strömförsörjning:** Adafruit PowerBoost 1000C (för stabil 5.2V-matning och laddning under drift) parad med ett massivt **8000 mAh LiPo-batteri** (vilket ger uppskattningsvis 40–60+ timmars skrivtid).
* **Inmatning:** Ett färdigt 65% mekaniskt tangentbord som ansluts via USB (Micro-USB OTG till din Pi Zero) och skickar standard key-koder.
* **Övriga Kontroller:** 
  - En tryckknapp kopplad till GPIO för att trigga en asynkron synk och ett "Safe Shutdown"-skript.
  - En taktil strömbrytare (switch) kopplad till **EN (Enable)**-stiftet på PowerBoost för att helt bryta strömmen från batteriet vid avstängt läge (förhindrar urladdning).

---

### 2. Projektprioriteringar (Kärnfokus)

För att projektet ska vara intressant och framgångsrikt måste vi kompromisslöst prioritera följande:

1. **Lägsta möjliga latens:** Tangenttryck till skärmrespons måste kännas omedelbart. Skärmen ska helst hänga med även under snabba "bursts" i skrivandet (upp till 80 ord i minuten / ~6.7 tecken per sekund).
2. **Extrem strömsnålhet:** WiFi och onödiga processer ska hållas helt avstängda under skrivfasen. Endast rå inmatning och skärmdrivning får belasta den enkärniga ARMv6-processorn.
3. **Säkerhet för data:** All text ska skrivas kontinuerligt till SD-kortet för att förhindra förlust vid plötsligt spänningsfall, samt synkas säkert och självständigt mot en NAS eller till Dropbox.
4. **Enkel filhantering** Möjlighet att växla mellan olika textdokument.

---

### 3. Beslut kring Mjukvara och Arkitektur

För att uppnå våra mål och undvika flaskhalsar på ARMv6-arkitekturen har vi tagit följande strategiska beslut för mjukvaran:

#### Inmatning & Logik (C-baserat "Bare-Metal"-tänk)

* Vi använder C. Python (och bibliotek som Pillow) är för tunga för att uppnå minimal latens.
* Vi bygger en **C-baserad renderingsmotor** som körs direkt mot Waveshares officiella C-bibliotek (bcm2835-baserat) för att styra IT8951 direkt via SPI.
* **Glyph Caching:** Vid uppstart renderar C-programmet ditt önskade typsnitt (motsvarande storlek 10–12 i Word, vilket på HD-skärmen motsvarar cirka 45–50 pixlar höga bokstäver) och sparar dem som monokroma ($32 \times 64$px) bitmapps-arrayer direkt i RAM-minnet (`font.h`). Beroende på hur eink-skärmen fungerar kan det vara nödvändigt med 32px eller 64px. Möjligen med padding. Just nu finns en renderad ascii-fil: lib/Fonts/font20.c
* När en tangent trycks ned ska `evdev` läsa av detta. Programmet ska göra en blixtsnabb `memcpy` av rätt bokstavs-bitmapp och skicka endast den lilla förändrade rutan (damage box, ca 256 bytes) över SPI.
* **Undvika "ghosting"**

När du kör i A2-läge (som är optimalt för din skrivmaskinsvision) ackumuleras "spökskuggor" snabbare än i GC16. I C kan du enkelt implementera en enkel räknare i din huvudloop:
* Lättvikts-uppdatering: Varje tangenttryck triggar en IT8951_Display_Area (Partial Refresh) i A2-läge.
* Fullständig uppdatering: När char_count % 500 == 0 (eller vid en specifik knapptryckning), anropar du funktionen för IT8951_Clear_Screen följt av en fullständig ominitiering av hela ytan.

#### Skärmstyrning (IT8951-optimering)

* **Skrivläge (A2-mode):** Under aktivt skrivande körs skärmen i det monokroma, asynkrona **A2 (Animation)-läget**.
* **Tyst städning (DU-mode):** Vid naturliga pauser, som vid tryck på `Enter` (ny rad), ska den aktuella raden eller skärmen uppdateras i det skarpare **DU (Direct Update)-läget** för att rensa bort eftersläpningar (ghosting).
* **Helskärms-refresh:** En fullständig nollställning (flash) görs endast vid stora händelser (t.ex. sidbyte, retur för nytt stycke, eller jump enligt nedan).
* **Skrivposition (jump)** För att uppdatera så lite av skärmen som möjligt skall skrivprompten få nå botten av skrivytan innan texten hoppar upp till den övre tredjedelen av skärmen och prompten följer med. En bra lösning för att navigera uppåt kan vara det motsvarande, texten hoppar två tredjedelar av skrivytans höjd.Konventionell scroll leder till att för mycket av skärmen uppdateras för ofta.

#### Nätverk & Synk (Tailscale + NAS)

* **Helt självständig enhet:** Skrivmaskinen sköter allt själv på kommando, utan att du behöver interagera med en annan dator.
* **Säker tunnel via Tailscale:** Vi installerar Tailscale på din Pi Zero W (fullt kompatibelt med ARMv6). Den tunnlar sig automatiskt och säkert in till din NAS så fort nätverket aktiveras.
* **Okrypterad Rsync (Ingen dubbelkryptering):** Eftersom Tailscale (WireGuard) redan krypterar nätverkstrafiken, kör vi en **okrypterad `rsync` direkt mot din NAS rsync-port** över Tailscale-IP:n. Detta sparar massor av CPU-resurser och batteri på din Pi då vi helt slipper SSH-kryptering ovanpå VPN-krypteringen.
* **Automatisk backup** Spara en version varje timme, varje dag, varannan dag, varje vecka, varje månad.
* **On-demand (ctrl+q eller liknande):**
1. Slå på WiFi (`rfkill unblock`).
2. Vänta på anslutning till Tailscale-nätverket.
3. Kör okrypterad Rsync-push till din NAS.
4. Slå av WiFi helt och hållet (`rfkill block`).

---

## Beslut och upptäckter ##
* **Kompilering och drivrutiner:** Vi ska integrera Waveshares `Makefile`-logik och säkerställa att rätt flaggor (`-D BCM`) skickas till kompilatorn. Detta aktiverade de nödvändiga SPI-drivrutinerna för BCM2835-biblioteket.

---

## Sammanfattning ##

Vi har beslutat oss för att använda

* OS: Raspberry Pi OS Bullseye (Debian 11)
* Hårdvara: Pi Zero W med IT8951-drivkort. Just nu monterad som en shield eller hat. Bör senare kopplas till PINs på RPi för att vi ska komma åt oanvända PINs. All hårdvara fungerar, just nu drivs den på batteri.
* Mjukvara: Waveshares bibliotek ligger här: ~/IT8951-ePaper/. Vårt arbetsområde, och vårt git-repo finns här: ~/IT8951-ePaper/Raspberry/
* Drivrutin: SPI via det inbyggda BCM2835-biblioteket.

## Identifierade tekniska utmaningar ##
* **Renderingens innehåll:** För närvarande ritas endast en "bounding box" (fyrkant). Vi har bekräftat att kommunikationen fungerar, men behöver nu förstå hur en tecken-bitmappen ska läsas och skickas till skärmen.
* **Datatyp och format:** Det råder en diskrepans mellan fontens lagringsformat (bitmapp) och vad IT8951-kontrollern förväntar sig i `Packed Pixel`-läget.

---
*Projektet upprättad 2026-07-20.*
