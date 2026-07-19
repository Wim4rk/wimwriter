# Förslag: Mjukvaruroterad Framebuffer i 1-bpp #
Istället för att förlita sig på IT8951:s inbyggda hårdvarurotation för små uppdateringar, kan vi flytta komplexiteten till Pi Zeron och spara bandbredd.

## 1. En virtuell skärm i RAM (Strategi A) ##
Skapa en fullstor framebuffer i RAM-minnet på Pi Zeron. Eftersom vi arbetar i 1-bpp (svartvitt) tar en skärm på $1448 \times 1072$ pixlar enbart cirka 194 KB RAM ((1448 * 1072) / 8). Detta är extremt lättdrivet även för ARMv6.

## 2. Hantera all rotation i C-koden ##
Stäng av hårdvarurotationen i IT8951 helt och hållet, så att chippet förväntar sig pixlar i sin "native" orientering. När ett tecken ska ritas på skärmen:
* C-programmet räknar ut var på den roterade skärmen bokstaven ska hamna.
* Tecknets glyf roteras i mjukvara och skrivs in i den lokala RAM-bufferten. (Att skifta och skriva bitar i RAM tar bara några mikrosekunder).

## 3. Skicka byte-alignerade block (Strategi B) ##
För att uppdatera e-bläcksskärmen blixtsnabbt skickar vi inte hela skärmen (vilket vore för långsamt), utan vi gör en partiell uppdatering.
* Vi beräknar en "damage box" (den rektangel som ändrats) utifrån RAM-bufferten.
* __Kritisk detalj:__ Vi tvingar denna bounding box att vara alignerad till jämna bytes (eller till och med 16-bitars ord) i X-led enligt controllerns native-orientering. Om rutan är $14 \times 20$ pixlar, expanderar vi den till exempelvis $16 \times 20$ eller $32 \times 32$ pixlar innan vi skickar den.
* Vi skickar denna expanderade, perfekt alignerade 1-bpp-ruta till IT8951.

### Fördelar med detta utkast ###
* __Ingen stride-förskjutning:__ Eftersom vi skickar jämna bytes till en oroterad kontroller, slipper vi brus och förvrängda tecken.
* __Maximal hastighet:__ Vi skickar bara de pixlar som har ändrats (partiell uppdatering i A2-läge) och vi gör det i 1-bpp, vilket skär ner SPI-trafiken till en åttondel jämfört med 8-bpp gråskala.
* __Kontroll:__ Vi vet exakt vad som finns på skärmen eftersom RAM-bufferten fungerar som "sanningen". Det gör det enklare att senare implementera en blinkande markör eller radera text.

## Strategi för att undvika "ghosting" ##

När du kör i A2-läge (som är optimalt för din skrivmaskinsvision) ackumuleras "spökskuggor" snabbare än i GC16. I C kan du enkelt implementera en enkel räknare i din huvudloop:

* Lättvikts-uppdatering: Varje tangenttryck triggar en IT8951_Display_Area (Partial Refresh) i A2-läge.

* Fullständig uppdatering: När char_count % 500 == 0 (eller vid en specifik knapptryckning), anropar du funktionen för IT8951_Clear_Screen följt av en fullständig ominitiering av hela ytan.
