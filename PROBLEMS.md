## Latens

Vi börjar nå dit vi ska. Vi ska göra en branch i git och testa att se vad som händer när jag hårdkodar variabler kring typer och radavstånd?

Det finns tre tunga tekniska argument för att skrota F6 och hårdkoda layouten nu när du har hittat rätt, särskilt med tanke på dina krav för maskinen.
* Kritisk latensvinst (ARMv6-optimering): Din Pi Zero W använder en ARMv6-processor. Denna arkitektur saknar hårdvarustöd för division. När teckenbredd och radavstånd är dynamiska variabler tvingas processorn utföra mjukvarubaserad multiplikation och division varje gång en koordinat ska räknas ut, vilket kostar värdefulla CPU-cykler. Om du hårdkodar dimensionerna som konstanter (t.ex. #define FONT_W 32) kan C-kompilatorn optimera bort matten och ersätta den med extremt snabba bit-shift-operationer. Detta är en direkt vinst för ditt mål att eliminera all tänkbar latens under snabbt skrivande.
* Optimerat minnesavtryck: Möjligheten att byta font via F6 implementerades primärt för att du skulle kunna testa olika fonter. Om du låser fast en specifik storlek kan vi radera de andra font-arrayerna ur källkoden. Det minskar binärens storlek drastiskt, minskar inläsningstiden vid uppstart och frigör RAM som kan användas för att hantera längre textdokument.
* Förenklad "State"-hantering: Enligt dina specifikationer ska maskinen spara den aktiva fonten till minnet när en ändring sker via F6. Genom att hårdkoda layouten kan vi helt stryka denna logik. Maskinen behöver inte längre läsa in eller spara teckenstorlek vid uppstart och nedstängning, vilket innebär färre I/O-operationer mot SD-kortet och därmed en lägre risk för fördröjningar och korrupt data.

Här är en översikt över exakt vad som krävs för att låsa fast layouten. Genom att göra detta kan kompilatorn byta ut tunga divisioner mot extremt snabba operationer, vilket direkt gynnar ditt mål att uppnå lägsta möjliga latens på din ARMv6-processor.
1. Radera funktionalitet i editor.c (Logik och State)
2. Ta bort F6: Radera lyssnaren för F6 i din inmatningsloop, vars enda syfte just nu är att växla teckenstorlek.
3. Stryk State-hanteringen för fonten: Maskinen ska spara den aktiva fonten vid filbyte eller avstängning. All logik för att läsa in och spara detta värde till SD-kortet kan nu raderas, vilket sparar I/O-cykler. 
4. Uppdatera display.h (Från variabler till konstanter)Radera datastrukturen ActiveFont, variabeln current_font, samt din LineSpacing-enum helt och hållet.Ersätt dem med statiska makron. Med ett typsnitt på 24x43 pixlar och 1,5 i radavstånd (+21 pixlar) blir din totala radhöjd 64 pixlar. Hårdkoda detta tillsammans med dina fasta marginaler på 44 pixlar (topp) och 68 pixlar (botten, höger, vänster).  Med hårdkodade konstanter kommer kompilatorn att förberäkna skärmytan till exakt 54 kolumner och 15 rader redan innan programmet körs.
5. Förenkla display.c (Renderingsmotorn)Ta bort funktionerna set_active_font(), set_line_spacing() och calculate_layout_points().Skriv om init_glyph_cache() så att den läser direkt från arrayen wim_font_24x43 vid uppstart istället för att gå via en pekarstruktur.  Byt ut alla förekomster av current_font.width och liknande dynamiska variabler inuti renderingsfunktionerna till dina nya statiska makron.
6. Rensa projektkatalogen och MakefileRadera filerna för 16x28, 24x32 och andra överblivna fonter från din /fonts-katalog.Ta bort kompilering och länkning av dessa från din Makefile för att minska binärens storlek och frigöra RAM-minne.

## Filhantering

Jag lyckades uppdatera lite textfiler igår, men det betedde sig inte som förväntat. "Spara som" sparade bara filen, inte innehållet. När jag sparade därefter sparades varje tecken två gånger: "HHaarr  jjaagg  iinnggeett  ssppaarraatt  hhrr..  DDeett  rr  iinnttee  bbrraa.." Där finns inga svenska tecken heller.

Debugga och bygg vidare. Spara dokument på ett logiskt ställe. ~/Dokument/writer kanske?

## Höger shift

Bokstäver med shift har strulat lite. Undersök och förbättra.

## Backspace och radrytning

Ctrl + Backspace över radbrytning: Det fungerar inte att hoppa över rader. Vanlig backspace fungerar som förväntat.

*Dokumentet uppdaterat 2026-08-01*
