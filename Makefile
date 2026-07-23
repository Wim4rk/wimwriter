# ==========================================
# Makefile för wimwriter (IT8951 Bare-Metal)
# ==========================================

# Sökvägar till Waveshares drivrutinsbibliotek
DIR_CONFIG = ./lib/Config
DIR_EPD    = ./lib/e-Paper

# Sökvägar till GUI och gamla typsnitt (kan ofta inkluderas ifall
# EPD_IT8951.c har inbyggda beroenden till dessa, även om vi kringgår dem)
DIR_GUI    = ./lib/GUI
DIR_FONTS  = ./lib/Fonts

# Samla alla .c-filer i rotdatalogen (inkl. main.c) samt i lib-katalogerna
SRC_C = $(wildcard *.c ${DIR_CONFIG}/*.c ${DIR_EPD}/*.c ${DIR_GUI}/*.c ${DIR_FONTS}/*.c)

# Byt ut .c mot .o för objektfilerna
OBJ_O = $(patsubst %.c,%.o,${SRC_C})

# Namnet på den färdiga kompileringen
TARGET = wimwriter

# Kompilator
CC = gcc

# Kompilatorflaggor:
# -Wall: Visa alla varningar
# -O3: Maximal hastighetsoptimering (viktigt för ARMv6-processorn)
# -D BCM & -DUSE_BCM2835_LIB: Säger till DEV_Config.c att vi använder bcm2835 för SPI/GPIO
CFLAGS = -Wall -O3 -D BCM -DUSE_BCM2835_LIB

# Nödvändiga bibliotek (bcm2835 för hårdvara, math, realtid, pthreads)
LIB = -lbcm2835 -lm -lrt -lpthread

# Huvudmål: Länka samman allt till den färdiga binären
$(TARGET): $(OBJ_O)
	@echo "Länkar samman $(TARGET)..."
	$(CC) $(CFLAGS) $(OBJ_O) -o $@ $(LIB)

# Kompilera varje .c-fil till en .o-fil
%.o: %.c
	@echo "Kompilerar $<..."
	$(CC) $(CFLAGS) -c $< -o $@

# Rensningskommando (kör 'make clean' för att ta bort gamla byggfiler)
clean:
	@echo "Städar upp objektfiler och binär..."
	rm -f $(OBJ_O) $(TARGET)
