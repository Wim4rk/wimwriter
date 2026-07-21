# Referens #

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
