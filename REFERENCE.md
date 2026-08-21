# Referens #

## Nedstängning via terminalen

Eftersom vi redan har kopplat signalhanteraren handle_sigint och handle_shutdown till operativsystemets SIGINT, kan du trigga exakt samma säkra nedstängningsrutin (som sparar, committar, pushar och rensar skärmen) från en SSH-session.

Gör så här när du vill stänga av maskinen:
* Tryck F10 på skrivmaskinen för att aktivera WiFi.
* Logga in via SSH.
* Kör kommandot pkill -SIGINT wimwriter (byt ut wimwriter mot namnet på din kompilerade binärfil).

Detta skickar avbrottssignalen till C-programmet. Programmet kommer då att fånga signalen, utföra editor_shutdown() och slutligen stänga av Raspberry Pi-enheten helt via systemanropet sudo poweroff.

Du kan därefter slå av din master power-brytare. Vi kan justera tidsfördröjningarna för nätverkskontrollen framöver om det visar sig att Tailscale behöver mer eller mindre tid på sig att etablera anslutningen.

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
