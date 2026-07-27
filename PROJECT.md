# Projekt wimwriter: en minimalistisk digital skrivmaskin

---

## Projektetspecifikationer

### 1. Prioriteringar (Kärnfokus)

För att projektet ska vara intressant och framgångsrikt måste vi kompromisslöst prioritera följande:

1. **Lägsta möjliga latens:** Tangenttryck till skärmrespons måste kännas omedelbart. Skärmen ska helst hänga med även under snabba "bursts" i skrivandet (upp till 80 ord i minuten / ~6.7 tecken per sekund).
2. **Extrem strömsnålhet:** WiFi och onödiga processer ska hållas helt avstängda under skrivfasen. Endast rå inmatning och skärmdrivning får belasta den enkärniga ARMv6-processorn.
3. **Enkel filhantering** Möjlighet att växla mellan olika textdokument.
4. **Säkerhet för data:** All text ska synkas säkert och självständigt mot en NAS eller till Dropbox. Osparade dokument ska dumpas i en swapfil på SD-kortet. Om användaren växlar fil ska föregående dokument sparas. Tänk skrivmaskin: om ordet är satt på papper så sitter det på papperet.

---

### 2. Hårdvarukonfiguration

Efter utvärdering har vi spikat följande hårdvaruuppsättning, med fokus på att använda de delar jag redan äger och att maximera drifttiden:

* **Processor:** Raspberry Pi Zero W (v1). Vald framför Zero 2 W eftersom den har en extremt låg strömförbrukning (ARMv6-arkitektur).
* **Skärm:** 6-tums e-bläcksskärm HD ansluten via en dedikerad *IT8951 Driver HAT*, som i sin tur pratar med din Pi via *SPI*. 
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
* **Helskärms-refresh:** En fullständig uppdatering (flash) finns tillgänglig via F5.
* **Skrivposition (jump)** För att uppdatera så lite av skärmen som möjligt skall skrivprompten få nå botten av skrivytan innan texten hoppar upp till den övre tredjedelen av skärmen och prompten följer med. Vi behöver prova ut hur många rader texten ska hoppa.

#### Skrivprogram

Gränssnittet skall vara minimalt. I stort sett bara textytan. Piltangenterna ska kunna stega horisontellt och vertikalt för enklaste navigering. Funktionsknapparna skall nyttjas för att utföra specifika åtgärder; öppna- och skapa dokument, spara, synkronisera och liknande.

Möjligen vore en statusrad längst ner på skärmen användbar för ordräkning, annan statistik eller andra uppgifter.

#### Nätverk & Synk (Tailscale + NAS/Filserver)

* **Helt självständig enhet:** Skrivmaskinen sköter allt själv på kommando, utan att jag behöver använda en annan dator.
* **Säker tunnel via Tailscale:** Vi installerar Tailscale på Pi Zero W (fullt kompatibelt med ARMv6). Den tunnlar sig automatiskt och säkert in till din NAS så fort nätverket aktiveras.
* **Okrypterad Rsync (Ingen dubbelkryptering):** Eftersom Tailscale (WireGuard) redan krypterar nätverkstrafiken, kör vi en **okrypterad `rsync` direkt mot din NAS rsync-port** över Tailscale-IP:n.
* **Automatisk backup** Dokumentsäkerhet, swap-filer, backup-filer. Ta fram en bra strategi.
* **On-demand save/sync(funktions-knapp):**
 1. Slå på WiFi (`rfkill unblock`).
 2. Vänta på anslutning till Tailscale-nätverket.
 3. Kör okrypterad Rsync-push till din NAS.
 4. Slå av WiFi helt och hållet (`rfkill block`).

---

## Beslut och upptäckter ##
* **Kompilering och drivrutiner:** Vi måste säkerställa att rätt flaggor (`-D BCM`) finns i vår Makefil. Detta aktiverade de nödvändiga SPI-drivrutinerna för BCM2835-biblioteket.

---

## Framtida modifikationer och förbättringar

Det vore fördelaktigt för batteritiden att bygga ett integrerat tangentbord som inte kräver en separat USB-anslutning. Därför sparar vi på fysiska PINs på Raspberryn så gott vi kan.

---
*Projektet upprättad 2026-07-20.*

*Senaste uppdateringen 2026-07-27*
