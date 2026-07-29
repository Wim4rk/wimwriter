# Specifikationer för skrivmiljön

## Minimalistisk skrivyta

I inmatningsläget är skrivytan helt ren förutom den text jag har matat in. Inga statusrader eller andra UI-element. Prompten syns endast vid inaktivitet eller uppstart/filbyte. När en fil öppnas skall den tidigare texten uppta skärmens övre del och lämna ett litet antal rader tomma för att fortsätta skriva, antalet bestäms av JUMP_LINES-variabeln.

Det vi har att kämpa med är skärmens latens och långsamma uppdatering. Kompromisser må göras för att uppnå snabb och responsiv inmatning.

## Statusrad

En statusrad dyker upp *när den behövs*. Den visar relevant information som till exempel filnamn vid filbyte.

## Skrivytans storlek

Vår display är 1448x1072 pixlar. Varje tecken är 32x64 pixlar.
* Statusradens höjd: teckenhöjd (64 pixlar) plus en skiljeline (4 pixlar) ger 68 pixlar. Den nedre marginalen är alltså 68 pixlar hög.'
* Marginaler höger och vänster: 68 pixlar var. Det lämnar 1312 pixlar. Dessa delas upp i 41 kollumner.
* Toppmarginalen kompromissas ner till 44 pixlar vilket ger oss möjlighet till 15 rader.


## Jump

När skrivprompten når skrivytans slut genomför vi ett "jump". Istället för att skrolla i vanlig bemärkelse så hoppar texten upp så att den fyller skärmens översta rader. Skrivprompten fortsätter sitt jobb på nästa rad. Beteendet styrs med variabeln JUMP_LINES.

## Radbrytning

Vi tillämpar word wrapping i A2-läge. Vi låter ordet hoppa ner, och raderar den gamla positionen. Om detta stör skrivflytet kan vi testa att inte radera förrän en naturlig paus uppstår, eller om funktionen Jump exekveras.

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
| F6     | Fontsize  | Reserverad för ändring av fontstorlek |
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
* Katalogbyte. Shift + F3 cyklar mellan kataloger och öppnar den senaste fil man jobbat med i respektive katalog. I statusraden visas den aktuella katalogen med filnamn: roman_utkast/kapitel_4.txt.
* Esc avbryter och återgår till senast bearbetade fil.

Det återstår att se om bläckskärmen klarar av det här på ett tillfredsställande vis? Reservplan 1: F3 ritar upp en enkel filhanterare, där användaren kan navigera och välja filer och kataloger. Reservplan 2: En mycket enkel LCD-skärm ansluts och kan då användas som statusrad.

### F4 - Nytt dokument

Rensa bara skärmen, ge mig en ny skrivyta. Möjligtvis en notering i statusraden om att detta är en ny fil? Esc avbryter och återgår till senast bearbetade fil.

### F5 - Uppdatera skärmen

Fullständig uppdatering av skärmen, INIT(Mode 0). Skärmen rensas och dess innehåll återställs så fort som möjligt.

### F9 - Synkronisera

* F9 (Standard Synk): Kör det skript du redan definierat. WiFi slås på, ansluter till Tailscale, rsync kopierar över nya/ändrade filer okrypterat till din NAS, och WiFi stängs av. Inget raderas lokalt. Skrivning kan fortsätta under synkronisering.
* Shift + F9: (Arkivera och Radera): Detta triggar den destruktiva arkiveringen. Bekräftelse i gränssnittet: När Ctrl + F9 trycks ned, pausas editorn och statusraden kräver en bekräftelse: Arkivera till NAS och radera lokalt? (J/N). Säker rsync-radering: Om du trycker 'J', aktiveras din nätverksrutin (WiFi on, vänta på Tailscale). Vi använder sedan kommandot rsync men lägger till flaggan --remove-source-files.  

### F10 - WiFi på / av

En *manual override* som låter dig slå på/av WiFi, till exempel för administration över SSH. När WiFi är på skall statusraden vara igång hela tiden och visa "WiFi".

## Spara state

* Spara vid ändring (F3, F6): Variabler som sällan ändras, såsom aktiv font och filhistorik, sparas direkt när växlingen sker. Eftersom dessa händelser ändå kräver en uppdatering av skärmen och en naturlig paus i skrivandet, kommer systemet inte att blockeras under aktiv inmatning. 
* Spara via GPIO (Avstängning): Dynamiska data som skrivpromptens exakta position, inaktivitetstimer, när senaste backup gjordes, samt exakt vad som finns i skärmbufferten sparas undan när maskinen stängs av. Eftersom GPIO-knappen redan är avsedd att trigga ditt "Safe Shutdown"-skript och en asynkron synkronisering, är detta det logiska tillfället att säkra arbetsmiljöns sista status innan EN-stiftet bryter strömmen.  

## Navigera

Vi behöver se till att pilarna, page up/down, home och end fungerar som förväntat. Även med mod-tangenter (Ctrl + Home/End, Ctrl + Pil H/V). 

## Redigera text

Om jag ställer prompten någon annanstans än i slutet av texten (för att redigera) så ska texten *efter* prompten suddas bort från skärmen medan jag skriver. När jag sedan använder andra tangenter (pilar, Home, End, Funktionstangenter) eller blir inaktiv så ska all text renderas igen. Här behöver vi tillämpa stitching igen.

Alla tangenter ska fungera som förväntat, inklusive insert, backspace och delete (Även Ctrl + Backspace/Delete).
---
Förslag på systemarkitekturDokumentbufferten (Modellen): Detta lager håller hela filens text i arbetsminnet. En datastruktur likt en dubbellänkad lista över rader, eller en "gap buffer", är ofta fördelaktig. När en tangent trycks ned uppdateras denna struktur först.Skärmbufferten (Vyn): En statisk 2D-array motsvarande skärmens 41 kolumner och 15 rader. Den agerar enbart fönster mot dokumentbufferten. Detta lager är nödvändigt för att snabbt kunna fastställa koordinater och skicka korrekta "damage boxes" till IT8951-kontrollern utan att behöva iterera över hela dokumentet.  Temp-filen (Säkerheten): Den löpande lagringen. All inmatning dumpas omedelbart till en dold temp-fil på SD-kortet.  

Händelseförlopp vid tangenttryckFör att maskinen ska kännas omedelbar och klara skurar av tangenttryckningar upp emot 80 ord i minuten, bör arbetsflödet vid inmatning hållas linjärt och asynkront:  Steg 1 (RAM): evdev plockar upp tecknet och det förs in i dokumentbufferten.Steg 2 (Vy): Skärmbufferten uppdateras med tecknet på aktuell markörposition.Steg 3 (SPI): Rendera tecknet. Rätt monokroma bitmapp hämtas från minnet och en specifik skärmuppdatering skickas över SPI för att ritas ut i A2-läget.  Steg 4 (Disk): Appendera tecknet till den dolda temp-filen på SD-kortet. För att undvika att SD-kortets skrivfördröjning blockerar nästa tangenttryckning kan denna operation med fördel göras via en icke-blockerande process eller buffras tillfälligt.

---

## Nedprioriterade funktioner

Detta är saker vi implementerar *om det visar sig möjligt*.

* **Markering av text** Markering av text ska fungera som förväntat. Piltangenterna + shift-knappen. Markerad text ska distingeras med en understrykning. Genom att använda DU-läget kan vi utöka eller minska markeringen utan alltför myket ghosting.
* **Radera markering** Borttagning av markering med Backspace/Delete: Tecknen tas bort från minnesstrukturen. Texten som låg efter markeringen flyttas tillbaka, och skärmen ritar omedelbart upp det nya stycket med en tyst städning i DU-läget.

* **Urklipp** Kort livstid: Behöver inte överleva en omstart. Till att börja med endast ett urklipp i buffert.
- Ctrl + X/C ska fungera som förväntat, där markering tas bort och text kopieras till någon form av urklipp.
- Ctrl + V ska fungera som förväntat, där text från urklipp klistras in på markeringens plats. Uppdateras i DU-läget.

---
*Dokumentet skapat 2026-07-24*

*Dokumentet senast uppdaterat 2026-07-26*
