# Att göra

1. Filhantering (Kritiska justeringar)

    [ ] Konfigurera om get_full_path och/eller filväljaren så att den aktuella sökvägen (katalogen) inkluderas när en fil skickas till inläsningsbufferten.

    [ ] Åtgärda problemet med time(NULL) när systemet är offline. Alternativ: Installera en fysisk RTC-modul på GPIO (t.ex. DS3231) eller bygg en rutin som hämtar senaste tid från NAS:en vid uppstart.

2. Editor (Navigering)

    [ ] Implementera KEY_LEFT och KEY_RIGHT i STATE_EDITING. Koppla dessa till datamodellens gap-buffert och skärmens utritning.

    [ ] Implementera KEY_UP och KEY_DOWN (kräver en matematisk uträkning av radens längd i minnet).

    [ ] Implementera modifier-tangenter: Ctrl + End, Ctrl + Home, samt Ctrl + Pilar.

3. Display & Utritning (Redigering)

    [ ] Skapa rutinen för stitching vid infogande/radering mitt i texten: Om prompten står mitt i ett stycke och en tangent trycks ned, måste all efterföljande text förskjutas i RAM och skärmen ritas om tyst (lämpligen i A2- eller DU-läge) utan att hela skärmen blinkar till.

4. Hårdvara

    [ ] Fysisk inkoppling av Master Power mellan batteriet och EN-pinnen på Adafruit PowerBoost.

## Editor

* Visa bara filnamn i status-raden när en fil öppnas eller sparas. Inte hela sökvägen.

* Jag behöver kunna stega prompten och rätta enstaka tecken. Ctrl + End för att nå slutet av filen. Home för att nå radens början. Ctrl + pilar höger och vänster.

## Hårdvara

* Starta skrivmaskinen direkt in i editorn. Slå av WiFi.
* Löd master power (en på batteriladdaren)

*Dokumentet uppdaterat 2026-08-20*
