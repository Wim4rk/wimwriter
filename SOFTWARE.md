# Systemarkitektur

Text hanteras i en textfil i bakgrunden. Skärmen är en display. Textfilen uppdaterar skärmen, skärmen kan inte uppdatera textfilen.

* **Modellen - dokumentbufferten**. Hanterar hela dokumentet oberoende av skärmens dimensioner. När jag matar in text via tangentbordet fånger epoll upp detta och uppdaterar modellen omedelbart. Eftersom vi tänker tillåta viss redigering inuti texten, kan en datastruktur som en *gap buffer* vara en lämplig väg att undersöka.
*  **Vyn - skärmbuffet**. Fungerar enbart som ett temporärt fönster över en specifik del av modellen.
*  **IT8951 och SPI**. När texten läggs till i modellen utvärderas vyn. Endast den yta som faktiskt förändrats (damage box) överförs via SPI till IT8951-kontrollern.

---
# Specifikationer för skrivmiljön

## Minimalistisk skrivyta

I inmatningsläget är skrivytan helt ren förutom den text jag har matat in. Inga statusrader eller andra UI-element. Prompten syns endast vid inaktivitet eller uppstart/filbyte. När en fil öppnas skall den tidigare texten uppta skärmens övre del och lämna ett litet antal rader tomma för att fortsätta skriva, antalet bestäms av JUMP_LINES-variabeln.

Det vi har att kämpa med är skärmens latens och långsamma uppdatering. Kompromisser må göras för att uppnå snabb och responsiv inmatning.

## Statusrad

En statusrad dyker upp *när den behövs*. Den visar relevant information som till exempel filnamn vid filbyte. Den ska också försvinna så snart den inte behövs.

## Jump

När skrivprompten når skrivytans slut genomför vi ett "jump". Istället för att skrolla i vanlig bemärkelse så hoppar texten upp så att den fyller skärmens översta rader. Skrivprompten fortsätter sitt jobb på nästa rad. Beteendet styrs med variabeln JUMP_LINES.

## Radbrytning

Vi tillämpar word wrapping i A2-läge. Vi låter ordet hoppa ner, och raderar den gamla positionen.

## Spara och back-up

* Kontinuerlig lagring: all text dumpas till en dold temp-fil.
* Manuell spara: Uppdaterar huvudfilen på SD-kortet på ett snyggt och städat vis.
* Löpande back-up: tidigare versioner av dokumentet skall namnges och sparas undan på SD-kortet. Det gör det möjligt för mig att återställa tidigare versioner om jag inte gillar vart mitt skrivande är på väg. Detta kan hanteras över SSH och behöver inte ta upp utrymme i vårt GUI.

## Funktionsknappar

Använd funktionsknapparna enligt fastställd praxis.Tänk CUA-standard (Common User Access) av IBM 1987.

| Knapp  | Lommando  | Beskrivning |
| :----- | :-------- | :-------- 
| F1     | Hjälp     | Enkel ruta visar funktionsknapparnas uppgift  |
| F2     | Spara     | Spara nu (Tvingar en manuell skrivning till SD-kortet) |
| F3     | Öppna     | Öppna dokument/kataloger (Cycklar mellan textfiler) |
| F4     | Ny fil    | Nytt dokument |
| F5     | Uppd disp | Tvingar fullständig uppdatering av skärmen |
| F6     | TBD       | (Reserverad för framtida statistik, ordräkning m.m.) |
| F7     | TBD       | (Reserverad för framtida statistik, ordräkning m.m.) |
| F8     | TBD       | (Reserverad för framtida statistik, ordräkning m.m.) |
| F9     | Synk.     | Synkronisera enligt program/rutin  |
| F10    | WiFi      | WiFi på / av |
| F11    | TBD       | (Reserverad för framtida statistik, ordräkning m.m.) |
| F12    | TBD       | (Reserverad för framtida statistik, ordräkning m.m.) |


### F1 - Hjälp

Visar en enkel ruta med funktionsknapparnas uppgift. Esc avbryter och återgår till föregående skärm.

### F2 - Spara

Spara nu (Tvingar en manuell skrivning till SD-kortet). Om filen inte finns på SD-kortet, motsvarar F2 "Spara som". Wimwriter föreslår automatiskt ett lämpligt filnamn: "wimwriter - \[tidpunkt\].txt", men om skribenten matar in något så anpassar Wimwriter filnamnet efter deras val.

### F3 - Öppna

* Filbyte. F3 växlar wimwriter till nästa fil i katalogen. Filerna sorteras efter senast öppnad. I statusraden visas den aktuella filens namn tills jag börjar skriva.

* Esc avbryter och återgår till senast bearbetade fil.

Det återstår att se om bläckskärmen klarar av det här på ett tillfredsställande vis? Reservplan 1: F3 ritar upp en enkel filhanterare, där användaren kan navigera och välja filer och kataloger. Reservplan 2: En mycket enkel LCD-skärm ansluts och kan då användas som statusrad.

### F4 - Nytt dokument

Sparar öppen fil, rensar skärmen och ger en ny skrivyta. F2/F3 kommer prompta användaren att spara filen om den inte är tom.

### F5 - Uppdatera skärmen

Fullständig uppdatering av skärmen, INIT(Mode 0) eller GC16?. Skärmen rensas och dess innehåll återställs så fort som möjligt.

### F9 - Synkronisera

* F9 (Standard Synk): Kör det skript du redan definierat. WiFi slås på, ansluter till Tailscale, rsync jämför och synkroniserar filer. Skrivning kan fortsätta under synkronisering.
* Shift + F9: (Arkivera och Radera): Detta triggar den destruktiva arkiveringen. Bekräftelse i gränssnittet: När Ctrl + F9 trycks ned, pausas editorn och statusraden kräver en bekräftelse: Arkivera till NAS och radera lokalt? (J/N). Säker rsync-radering: Om du trycker 'J', aktiveras din nätverksrutin (WiFi on, vänta på Tailscale). Vi använder sedan kommandot rsync men lägger till flaggan --remove-source-files.  

### F10 - WiFi på / av

En *manual override* som låter dig slå på/av WiFi, till exempel för administration över SSH. När WiFi är på skall statusraden vara igång hela tiden och visa "WiFi".

## Filhantering

**F2 Sparar fil.** Om ingen fil är skapad, skapa en ny fil och fråga efter filnamnet i statusraden (sparas som).
**F3 Växla fil.** Växlar mellan dokument i tids-ordning. F3 en gång sparar aktuellt dokument (eller spara som, om ingen fil är skapad) och hoppar till senaste dokumentet, två gånger till näst senaste osv. Enter öppnar filen på skärmen för redigering. Esc återgår till aktuell fil.
**F4 Ny fil.** Tömmer skrivytan så att användaren kan börja skriva en ny fil.

## Spara state

* Dynamiska data som skrivpromptens exakta position, när senaste backup gjordes, samt exakt vad som finns i skärmbufferten sparas undan när maskinen stängs av, eller filen växlas.

## Svenska tecken i utdata

### Hexadecimala Escape Sequences

För att använda svenska tecken i gränssnittet utan att introducera en beräkningstung UTF-8-parser, kan du "lura" kompilatorn. Genom att infoga de exakta hexadecimala värdena från din Latin-1-uppsättning direkt in i strängarna, garanterar du att renderingsmotorn bara får en enda, korrekt byte per tecken.

Exempelvis:

const char *lines[] = {
    "F1  - Denna hj\xE4lpruta",     // \xE4 = ä
    "F2  - Spara manuellt",
    "F3  - \xD6ppna / Byt fil",     // \xD6 = Ö
    "F4  - Ny fil",
    "F5  - Uppdatera sk\xE4rm",     // \xE4 = ä
    "F9  - Synkronisera mot NAS",
    "F10 - WiFi P\xE5/Av",          // \xE5 = å
    "",
    "Esc - \xC5terg\xE5"            // \xC5 = Å, \xE5 = å
};

---

## Nedprioriterade funktioner

Detta är saker vi väntar med tills allt annat fungerar

* **Navigera** Vi behöver se till att pilarna, page up/down, home och end fungerar som förväntat. Även med mod-tangenter (Ctrl + Home/End, Ctrl + Pil H/V). 

* **Redigera text** Om jag ställer prompten någon annanstans än i slutet av texten (för att redigera) så ska texten *efter* prompten suddas bort från skärmen medan jag skriver. När jag sedan använder andra tangenter (pilar, Home, End, Funktionstangenter) eller blir inaktiv så ska all text renderas igen. Här behöver vi tillämpa stitching igen.

Alla tangenter ska fungera som förväntat, inklusive insert, backspace och delete (Även Ctrl + Backspace/Delete).

Detta är saker vi implementerar *om det visar sig möjligt*.

* **Markering av text** Markering av text ska fungera som förväntat. Piltangenterna + shift-knappen. Markerad text ska distingeras med en understrykning. Genom att använda DU-läget kan vi utöka eller minska markeringen utan alltför myket ghosting.
* **Radera markering** Borttagning av markering med Backspace/Delete: Tecknen tas bort från minnesstrukturen. Texten som låg efter markeringen flyttas tillbaka, och skärmen ritar omedelbart upp det nya stycket med en tyst städning i DU-läget.

* **Urklipp** Kort livstid: Behöver inte överleva en omstart. Till att börja med endast ett urklipp i buffert.
- Ctrl + X/C ska fungera som förväntat, där markering tas bort och text kopieras till någon form av urklipp.
- Ctrl + V ska fungera som förväntat, där text från urklipp klistras in på markeringens plats. Uppdateras i DU-läget.

* **Katalogbyte**. Shift + F3 cyklar mellan kataloger och öppnar den senaste fil man jobbat med i respektive katalog. I statusraden visas den aktuella katalogen med filnamn: roman_utkast/kapitel_4.txt.

---
*Dokumentet skapat 2026-07-24*

*Dokumentet senast uppdaterat 2026-07-31*
