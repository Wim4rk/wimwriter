# Specifikationer för skrivmiljön

## Minimalistisk skrivyta

I inmatningsläget är skrivytan helt ren förutom den text jag har matat in. Inga statusrader eller annat UI-element. Prompten syns endast vid inaktivitet eller uppstart/filbyte. Prompten startar alltid på sjätte raden. Om det redan finns text ovanför så ser vi därmed de senaste fem raderna, om dokumentet är tomt för vi en vit marginal ovanför den text som matas in.

## Statusrad

En statusrad dyker upp *när den behövs*. Den visar relevant information som till exempel filnamn vid filbyte.

## Skrivytans storlek

Vår display är 1448x1072 pixlar. Varje tecken är 32x64 pixlar.
* Statusradens höjd: teckenhöjd (64 pixlar) plus en skiljeline (4 pixlar) ger 68 pixlar. Den nedre marginalen är alltså 68 pixlar hög.'
* Marginaler höger och vänster: 68 pixlar var. Det lämnar 1312 pixlar. Dessa delas upp i 41 kollumner.
* Toppmarginalen kompromissas ner till 44 pixlar vilket ger oss möjlighet till 15 rader.

## Radbrytning

Vi tillämpar word wrapping i A2-läge. Sen väntar vi in en naturlig paus eller annan större skärmuppdatering (jump) innan vi städar upp eventuell ghosting med DU-läget.

## Jump

När skrivprompten når skivytans slut genomför vi ett "jump". Istället för att skrolla i vanlig bemärkelse så hoppar texten upp så att den bara fyller skärmens första fem rader. Skrivprompten fortsätter sitt jobb på rad sex.

## Spara och back-up

* Kontinuerlig lagring: all text dumpas till en dold temp-fil.
* Manuell spara: Uppdaterar huvudfilen på SD-kortet på ett snyggt och städat vis.
* Löpande back-up: tidigare versioner av dokumentet skall namnges och sparas undan på SD-kortet. Det gör det möjligt för mig att återställa tidigare versioner om jag inte gillar vart mitt skrivande är på väg. Detta kan hanteras över SSH och behöver inte ta upp utrymme i vårt GUI.

## Funktionsknappar

Använd funktionsknapparna enligt fastställd praxis.Tänk CUA-standard (Common User Access) av IBM 1987.

| Knapp  | CUA / DOS | Wimwriter |
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

Spara nu (Tvingar en manuell skrivning till SD-kortet). Om filen inte finns på SD-kortet, motsvarar F2 "Spara som". Wimwriter föreslår automatiskt ett lämpligt filnamn - \[tidpunkt\].txt, men om skribenten matar in något så anpassar Wimwriter filnamnet efter deras val.

### F3 - Öppna

* Filbyte. F3 växlar wimwriter till nästa fil i katalogen. Sorteringen borde vara att man växlar till den senaste fil man arbetat i. I statusraden visas den aktuella filens namn tills jag börjar skriva.
* Katalogbyte. Shift + F3 cyklar mellan kataloger och öppnar den senaste fil man jobbat med i respektive katalog. I statusraden visas den aktuella katalogen med filnamn: roman_utkast/kapitel_4.txt.

Esc avbryter och återgår till senast bearbetade fil.

### F4 - Nytt dokument

Rensa bara skärmen, ge mig en ny skrivyta. Möjligtvis en notering i statusraden om att detta är en ny fil? Esc avbryter och återgår till senast bearbetade fil.

### F5 - Uppdatera skärmen

Fullständig uppdatering av skärmen, INIT(Mode 0).

### F9 - Synkronisera

* F9 (Standard Synk): Kör det skript du redan definierat. WiFi slås på, ansluter till Tailscale, rsync kopierar över nya/ändrade filer okrypterat till din NAS, och WiFi stängs av. Inget raderas lokalt. Skrivning kan fortsätta under synkronisering.
* Shift + F9: (Arkivera och Radera): Detta triggar den destruktiva arkiveringen. Bekräftelse i gränssnittet: När Ctrl + F9 trycks ned, pausas editorn och statusraden kräver en bekräftelse: Arkivera till NAS och radera lokalt? (J/N). Säker rsync-radering: Om du trycker 'J', aktiveras din nätverksrutin (WiFi on, vänta på Tailscale). Vi använder sedan kommandot rsync men lägger till flaggan --remove-source-files.  

### F10 - WiFi på / av

En *manual override* som låter dig slå på/av WiFi, till exempel för administration över SSH. När WiFi är på skall statusraden vara igång hela tiden och visa "WiFi".

## Spara state

Vi behöver hålla reda på ett par saker:
* Vilka filer har jag senast jobbat med, så att F3 kan cykla mellan dem?
* Var står skrivprompten?
* Hur länge har användaren varit inaktiv?
* När skapades senaste backup?

## Navigera

Vi behöver se till att pilarna, page up/down, home och end fungerar som förväntat. Även med mod-tangenter (Ctrl + Home/End, Ctrl + Pil H/V). 

## Redigera text

Kravet på lägsta möjliga latens lättas i samband med redigering. Det kan kännas naturligt även om det går lite långsammare vid markering o. dyl. Det är heller inte maskinens främsta syfte varför vi kan kompromissa med detta med gott samvete.

Om jag ställer prompten någon annanstans än i slutet av texten (för att redigera) så ska texten *efter* prompten suddas bort från skärmen medan jag skriver. När jag sedan använder andra tangenter (pilar, Home, End, Funktionstangenter) eller blir inaktiv så ska all text renderas igen med hjälp av DU (Direct Update.)

Alla tangenter ska fungera som förväntat, inklusive insert, backspace och delete (Även Ctrl + Backspace/Delete).

## Markera text

Markering av text ska fungera som förväntat. Piltangenterna + shift-knappen. Markerad text ska distingeras med en understrykning. Genom att använda DU-läget kan vi utöka eller minska markeringen utan alltför myket ghosting.

* Borttagning av markering med Backspace/Delete: Tecknen tas bort från minnesstrukturen. Texten som låg efter markeringen flyttas tillbaka, och skärmen ritar omedelbart upp det nya stycket med en tyst städning i DU-läget.
* Ctrl + X/C ska fungera som förväntat, där markering tas bort och text kopieras till någon form av urklipp.
* Ctrl + V ska fungera som förväntat, där text från urklipp klistras in på markeringens plats. Uppdateras i DU-läget.

## Urklipp

Kort livstid: Behöver inte överleva en omstart. Det kan bara finnas ett aktuellt urklipp.

---
*Dokumentet skapat 2026-07-24*
