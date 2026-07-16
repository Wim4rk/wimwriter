Här är en komplett och strukturerad sammanfattning av vårt gemensamma beslutsläge för din digitala skrivmaskin. Detta dokument fungerar som vår gemensamma karta inför nästa fas.

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

1. **Lägsta möjliga latens (< 80 ms):** Tangenttryck till skärmrespons måste kännas omedelbart. Skärmen ska hänga med även under snabba "bursts" i skrivandet (upp till 80 ord i minuten / ~6.7 tecken per sekund).
2. **Extrem strömsnålhet:** WiFi och onödiga processer ska hållas helt avstängda under skrivfasen. Endast rå inmatning och skärmdrivning får belasta den enkärniga ARMv6-processorn.
3. **Säkerhet för data:** All text ska skrivas kontinuerligt till SD-kortet för att förhindra förlust vid plötsligt spänningsfall, samt synkas säkert och självständigt mot din NAS.

---

## 3. Mjukvaru- och Arkitekturbeslut

För att uppnå våra mål och undvika flaskhalsar på ARMv6-arkitekturen har vi tagit följande strategiska beslut för mjukvaran:

### Inmatning & Logik (C-baserat "Bare-Metal"-tänk)

* Vi förkastar Python (och bibliotek som Pillow) för själva renderingsloopen. Python är för långsamt för att beräkna bounding boxes och skjuta bilddata på en 1 GHz ARMv6-processor utan märkbar latens.
* Vi bygger en **C-baserad renderingsmotor** som körs direkt mot Waveshares officiella C-bibliotek (bcm2835-baserat) för att styra IT8951 direkt via SPI.
* **Glyph Caching:** Vid uppstart renderar C-programmet ditt önskade typsnitt (motsvarande storlek 10–12 i Word, vilket på HD-skärmen motsvarar cirka 45–50 pixlar höga bokstäver) och sparar dem som monokroma ($32 \times 64$px) bitmapps-arrayer direkt i RAM-minnet (`font.h`).
* När en tangent trycks ned läser `evdev` av detta. Programmet gör en blixtsnabb `memcpy` av rätt bokstavs-bitmapp och skickar endast den lilla förändrade rutan (damage box, ca 256 bytes) över SPI.

### Skärmstyrning (IT8951-optimering)

* **Skrivläge (A2-mode):** Under aktivt skrivande körs skärmen i det monokroma, asynkrona **A2 (Animation)-läget**. Detta minimerar latensen till under 50 ms.
* **Tyst städning (DU-mode):** Vid naturliga pauser, som vid tryck på `Enter` (ny rad), uppdateras den aktuella raden eller skärmen i det skarpare **DU (Direct Update)-läget** för att rensa bort eftersläpningar (ghosting).
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


### Minnesanteckningar ###

Vi har beslutat oss för att använda

* OS: Raspberry Pi OS Bullseye (Debian 11)
* Hårdvara: Pi Zero W med IT8951-drivkort. Just nu monterad som en shield eller hat. Bör senare kopplas till PINs på RPi för att vi ska komma åt oanvända PINs.
* Mjukvara: Waveshares nya bibliotek i ~/IT8951-ePaper/Raspberry.
* Drivrutin: Standard-SPI via det inbyggda BCM2835-biblioteket (kompilerat med enbart sudo make -j4).

### Git ###

Publikt git-repo finns: https://github.com/Wim4rk/wimwriter
Sent i processen ska vi göra detta offentligt så att andra som vill bygga en writerdeck får fler idéer och lösningar.

### Nuvarande läge ###
* Vi har anslutit skärmen via drivkortet som en HAT.
* Vi har kört Waveshares egna test.
* Vi har skapat glypher för vår glyph-cache och inlett programmering för att visa dem.
## Tangentbords-lyssnaren (Linux Input Subsystem): ##
* Vi har kodat en robust while(read(...)) loop som lyssnar direkt på /dev/input/event0. Den fångar hårdvarukoder (Keycodes) för tangenttryckningar och filtrerar bort "key release" (ev.value == 1).
## Tangentbords-översättaren (Scancode till ASCII): ##
* Vi byggde en rå översättningstabell (if/else-block) som mappar Linux scancodes (t.ex. 30 för tangenten 'A') till tecken. Vi har även förberett för de svenska tecknen Å, Ä, Ö genom att mappa dem till specifika index där de förväntas ligga i fontmatrisen.
## Font-uppackaren (1-bpp till Gråskala): ##
* Vi har knäckt logiken för hur Waveshares inbyggda typsnitt (Font20) är packade. Fonten är lagrad som en monokrom (1-bit-per-pixel) bitmapp där varje rad tar upp 2 bytes (16 bitar). Vår C-kod läser dessa bitar med bit-skiftning (<< x & 0x8000) och översätter dem till pixlar. Terminal-debuggen bevisade att denna uppackning är 100 % perfekt.

## Hårdvarumanipulationen (Waveshare-hacket):##
* Vi gick in i lib/e-Paper/EPD_IT8951.c och tog bort static-nyckelordet från centrala funktioner för att din egen kod ska kunna prata direkt med IT8951-kontrollerns register och minnesbanker utan omvägar. Vi ändrade även initieringsparametrarna för att rotera skärmen.

### Analys ###
När vi testade att skriva ut bokstaven a i terminalen ritades den upp perfekt. När den skickas till e-bläck-skärmen blir det en "storm av pixlar" (pixelkaos).IT8951-kontrollern är extremt kräsen med hur pixlar är organiserade i det fysiska minnet (VRAM) jämfört med hur de skickas över SPI-bussen. När skärmen dessutom är hårdvaruroterad, ändras minneslayouten i chippet radikalt. En "rad" i chippets ögon är inte längre samma sak som en rad i vår array.När vi skickar en liten isolerad ruta på $14 \times 20$ pixlar med funktionen EPD_IT8951_HostAreaPackedPixelWrite_8bp, försöker chippets DMA-motor att placera dessa pixlar i en roterad matris. Det resulterar i att bitar och bytes hamnar helt förskjutna, vilket skapar bruset.

### Möjliga strategier ###
För att få slut på pixelkaoset en gång för alla och faktiskt se bokstäver på skärmen, har vi två strategiska vägar att gå när vi plockar upp tråden igen:
* Strategi A (Framebuffert-metoden): Vi slutar ladda upp små mikro-buffertar per bokstav. Vi ritar istället i en stor framebuffert i Pi:ns RAM som matchar hela skärmens storlek ($1448 \times 1072$). Sedan hittar vi den exakta Waveshare-funktionen som skickar hela skärmen på rätt sätt, vilket gör att vi slipper IT8951:s delvisa rotations-kaos.
* Strategi B (1-bpp läget): Eftersom fonten redan är i 1-bpp (svartvitt), kan vi ställa om skärmen till det supersnabba 1-bpp-läget (Pixel_Format = 0). Det kräver att vi packar om våra pixlar till bitar, men det sparar enormt mycket SPI-bandbredd och brukar vara betydligt mindre känsligt för stride-fel i kontrollern.

### Specar för e-bläckskärmen ###
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
