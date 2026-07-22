# Projekt wimwriter: en minimalistisk digital skrivmaskin

## Prio 1: Det synbarligen oöverstigliga problemet som måste lösas

Innan något annat kan göras måste vi kunna rendera text på skärmen. Vi behöver lösa **glyph-caching**.

Det finns för närvarande en olöst teknisk utmaning gällande datatyp och format, där styrenhetens Packed Pixel-läge förväntar sig ett annat format än det bitmappsformat som de medföljande fonterna lagras i.

### Resultat av felsökning:

Det vi ser i Waveshares källkod är att deras implementation av 1-bpp (svartvitt) är ett fulhack som krockar med vår minneshantering.Här är de kritiska insikterna från din sökning i källkoden:

IT8951_1BPP existerar inte: Din sökning efter detta macro returnerade helt tomt. Biblioteket förlitar sig uteslutande på 2BPP, 4BPP och 8BPP.

**Den inofficiella lösningen:** Titta på bibliotekets egna kommentarer: //Use 8bpp to set 1bpp. När du anropar 1-bpp-funktionen ställer biblioteket i själva verket in hårdvaran på 8-bitars färgdjup (IT8951_8BPP).

**Minnesmatematiken:** I funktionen castas din buffert till 16-bitars ord (UWORD*), och bredden beräknas som Area_Img_Info->Area_W/2. Om en ruta är 32 pixlar bred, skickar kontrollern alltså 16 UWORDs över SPI, vilket är exakt 32 bytes.

### Kärnproblemet:
Waveshares "1-bpp"-funktion förväntar sig en uppackad buffert där 1 pixel = 1 hel byte (exempelvis 0x00 för svart och 0xFF för vitt). Vår nuvarande cache packar 8 pixlar i en enda byte, vilket rimmar väl med strategin om en extremt lättdriven virtuell skärm i RAM. Men när vi skickar denna kompakta buffert till Waveshares drivrutin, tolkar den varje packad byte som en enda pixel och fortsätter sedan att läsa data långt utanför buffertens minnesområde.

### Påbörjat försök till lösning:
Vi har skapat ett eget teckensnitt. Det omfattar Index 0x00 (' ') till 0x7F (' ').

När vi nu går över till att använda ett eget 32x64-typsnitt, har vi ett par olika förslag på hur vi kan rendea det:
* Alternativ 1 (Snabbast körning): Vi sparar typsnittet helt uppackat. En bokstav tar då cirka 2 KB i anspråk istället för 256 bytes. Det tar marginellt mer plats i den färdiga binären, men vi kan mata datan rakt in i kontrollern utan processorkraft.
* Alternativ 2 (Minst minnesavtryck): Vi behåller idén om en 1-bpp-array (där 1 bit = 1 pixel) för att spara minne och underlätta framtida framebuffer-synkronisering. Sedan skriver vi ett förslag till en egen liten uppackningsfunktion i C som blixtsnabbt översätter bits till bytes i en tillfällig buffert precis innan SPI-överföringen sker.

I enlighet med projektsprioriteringarna nedan så skall vi i första hand fokusera på så låg latens som möjligt.

---

## Projektetspecifikationer

### 1. Projektprioriteringar (Kärnfokus)

För att projektet ska vara intressant och framgångsrikt måste vi kompromisslöst prioritera följande:

1. **Lägsta möjliga latens:** Tangenttryck till skärmrespons måste kännas omedelbart. Skärmen ska helst hänga med även under snabba "bursts" i skrivandet (upp till 80 ord i minuten / ~6.7 tecken per sekund).
2. **Extrem strömsnålhet:** WiFi och onödiga processer ska hållas helt avstängda under skrivfasen. Endast rå inmatning och skärmdrivning får belasta den enkärniga ARMv6-processorn.
3. **Säkerhet för data:** All text ska skrivas kontinuerligt till SD-kortet för att förhindra förlust vid plötsligt spänningsfall, samt synkas säkert och självständigt mot en NAS eller till Dropbox.
4. **Enkel filhantering** Möjlighet att växla mellan olika textdokument.

---

### 2. Hårdvarukonfiguration

Efter utvärdering har vi spikat följande hårdvaruuppsättning, med fokus på att använda de delar jag redan äger och att maximera drifttiden:

* **Processor:** Raspberry Pi Zero W (v1). Vald framför Zero 2 W eftersom den har en extremt låg strömförbrukning (ARMv6-arkitektur).
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

### 3. Beslut kring Mjukvara och Arkitektur

För att uppnå våra mål och undvika flaskhalsar på ARMv6-arkitekturen har vi tagit följande strategiska beslut för mjukvaran:

#### Inmatning & Logik (C-baserat "Bare-Metal"-tänk)

* Vi bygger en **C-baserad renderingsmotor** som körs direkt mot Waveshares officiella C-bibliotek (bcm2835-baserat) för att styra IT8951 direkt via SPI. Python (och bibliotek som Pillow) är för tunga för att uppnå minimal latens.
* **Glyph Caching:** Vid uppstart renderar C-programmet ett typsnitt och sparar dem som monokroma ($32 \times 64$px) bitmapps-arrayer direkt i RAM-minnet.
* När en tangent trycks ned ska `evdev` läsa av detta. Programmet ska göra en blixtsnabb `memcpy` av rätt bokstavs-bitmapp och skicka endast den lilla förändrade rutan (damage box, ca 256 bytes) över SPI.

#### Skärmstyrning (IT8951-optimering)

* **Skrivläge (A2-mode):** Under aktivt skrivande körs skärmen i det monokroma, asynkrona **A2 (Animation)-läget**.
* **Tyst städning (DU-mode):** Vid naturliga pauser, som vid inaktivitet eller när skrivprompten når skärmens slut, ska den aktuella skärmen uppdateras i det skarpare **DU (Direct Update)-läget** för att rensa bort eftersläpningar (ghosting).
* **Helskärms-refresh:** En fullständig nollställning (flash) görs endast vid stora händelser (t.ex. sidbyte, eller *jump* enligt nedan).
* **Skrivposition (jump)** För att uppdatera så lite av skärmen som möjligt skall skrivprompten få nå botten av skrivytan innan texten hoppar upp till den övre tredjedelen av skärmen och prompten följer med. Texten hoppar två tredjedelar av skrivytans höjd. Konventionell scroll leder till att för mycket av skärmen uppdateras för ofta.

#### Skrivprogram

Gränssnittet skall vara minimalt. I stort sett bara textytan. Piltangenterna ska kunna stega horisontellt och vertikalt för enklaste navigering. Funktionsknapparna skall nyttjas för att utföra specifika åtgärder; öppna- och skapa dokument, spara, synkronisera och liknande.

Möjligen vore en statusrad längst ner på skärmen användbar för ordräkning, annan statistik eller andra uppgifter.

#### Nätverk & Synk (Tailscale + NAS)

* **Helt självständig enhet:** Skrivmaskinen sköter allt själv på kommando, utan att jag behöver använda en annan dator.
* **Säker tunnel via Tailscale:** Vi installerar Tailscale på din Pi Zero W (fullt kompatibelt med ARMv6). Den tunnlar sig automatiskt och säkert in till din NAS så fort nätverket aktiveras.
* **Okrypterad Rsync (Ingen dubbelkryptering):** Eftersom Tailscale (WireGuard) redan krypterar nätverkstrafiken, kör vi en **okrypterad `rsync` direkt mot din NAS rsync-port** över Tailscale-IP:n. Detta sparar massor av CPU-resurser och batteri på din Pi då vi helt slipper SSH-kryptering ovanpå VPN-krypteringen.
* **Automatisk backup** Spara en version varje timme, varje dag, varannan dag, varje vecka, varje månad.
* **On-demand (funktions-knapp):**
1. Slå på WiFi (`rfkill unblock`).
2. Vänta på anslutning till Tailscale-nätverket.
3. Kör okrypterad Rsync-push till din NAS.
4. Slå av WiFi helt och hållet (`rfkill block`).

---

## Beslut och upptäckter ##
* **Kompilering och drivrutiner:** Vi ska integrera Waveshares `Makefile`-logik och säkerställa att rätt flaggor (`-D BCM`) skickas till kompilatorn. Detta aktiverade de nödvändiga SPI-drivrutinerna för BCM2835-biblioteket.

---
*Projektet upprättad 2026-07-20.*

*Senaste uppdateringen 2026-07-22*
