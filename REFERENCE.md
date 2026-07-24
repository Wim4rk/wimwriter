# Referens #

## Filsystemet

```
Raspberry/
├── main.c
├── Makefile
├── PROJECT.md    
├── README.md
├── REFERENCE.md
├── SOFTWARE.md
├── firmware/
│   ├── display.c
│   ├── display.h
│   ├── keyboard.c
│   └── keyboard.h
├── fonts/
│   └── wim_font_courier.c
├── lib/
│   ├── Config/
│   │   ├── DEV_Config.c
│   │   ├── DEV_Config.h
│   │   ├── DEV_hardware_SPI.c
│   │   ├── DEV_hardware_SPI.h
│   │   ├── RPI_gpiod.c
│   │   └── RPI_gpiod.h
│   └── e-Paper/
│       ├── EPD_IT8951.c
│       └── EPD_IT8951.h
└── software/
    ├── editor.c
    ├── editor.h
    ├── file_io.c
    ├── file_io.h
    ├── sync.c
    └── sync.h

```

## Git ##

Publikt git-repo för detta projekt finns här: https://github.com/Wim4rk/wimwriter. Det är publikt.

Andra git-repon där e-bläckskärmar används. Till exempel:
* PaperTTY - Som jag fått att fungera på den här skärmen, dock med en Raspberry Py 5 - vilket är för strömhungrigt för vårt nuvarande projekt.
* ZeroWriter som använder esp32.

## Specar för e-bläckskärmen ##
https://www.waveshare.com/wiki/6inch_HD_e-Paper_HAT?srsltid=AfmBOorzHeeWp2Pp1WDUMXZvQR7rJ5uvXGIq74KlfZNBFP9B932APp6L
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

## Input ##

* Keyboarden läses på /dev/input/event0. Det är en TADA68 med möjlighet till QMK-programmering.
* Taktil tryckknapp (för säker avstängning).
* Vipp-brytare (för att bryta strömmen permanent).

## Svensk ASCII ##
Svensk ASCII är en 7-bits teckenkod som användes i Sverige innan Latin-1 (ISO-8859-1) fick genomslag. SUNET:s rekommendation att använda svensk 7-bits teckenkod i datorpost upphörde den 1 januari 1995.

Den kan påträffas i äldre utrustning eller applikationer, där resultatet kan bli att bokstäverna "å ä ö Å Ä Ö" visas som tecknen "} { | ] [ \" eller vice versa. Det är dessa tecken som ersatts i original-ASCII för att skapa den svenska varianten.

## Milestones

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

När vi nu går över till att använda ett eget 32x64-typsnitt, har vi ett förslag på hur vi kan rendera det:
* Vi sparar typsnittet helt uppackat. En bokstav tar då cirka 2 KB i anspråk istället för 256 bytes. Det tar marginellt mer plats i den färdiga binären, men vi kan mata datan rakt in i kontrollern utan processorkraft.

## Hantering av uppochnedvända tecken

För att själva tecknen (som matas över SPI) inte ska hamna uppochned på din nu roterade skärm har du två effektiva vägar att gå:

1. Hårdvarans rotationsflagga:
    Strukturen som initierar bildinläsningen till IT8951-kontrollern har i standardutförande en rotationsparameter (motsvarande IT8951_ROTATE_0). Genom att ändra denna till värdet för 180 grader instruerar du IT8951 att själv rotera dataströmmen internt innan den landar i skärmens minnesadress. Detta belastar inte din Raspberry Pi överhuvudtaget.

2. Föroterad cache (Absolut snabbast):
    Eftersom du redan har valt att använda Alternativ 1 och lagrar din font som en statisk, förkompilerad C-array för att kringgå processorkraft, är det allra smidigaste att uppdatera genereringen av ditt typsnitt. Om tecknen i filen roteras 180 grader redan innan kompilering, skickar din motor exakt samma datamängd till kontrollern i standardläget, precis som tidigare.
