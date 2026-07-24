# ==========================================
# Makefile för wimwriter (IT8951 Bare-Metal)
# ==========================================

# Sökvägar till Waveshares drivrutinsbibliotek
DIR_CONFIG = ./lib/Config
DIR_EPD    = ./lib/e-Paper

# Sökvägar till GUI och gamla typsnitt (kan ofta inkluderas ifall
# EPD_IT8951.c har inbyggda beroenden till dessa, även om vi kringgår dem)
# DIR_GUI    = ./lib/GUI
# DIR_FONTS  = ./lib/Fonts

# Flaggor: BCM-stöd och exakta sökvägar till alla mappar med header-filer
SRC_C = main.c \
        $(DIR_CONFIG)/DEV_Config.c \
        $(DIR_EPD)/EPD_IT8951.c

# Byt ut .c mot .o för objektfilerna
OBJ_O = $(SRC_C:.c=.o)

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

# Inkluderingssökvägar för header-filer
INCLUDES = -I./firmware \
           -I./lib/Config \
           -I./lib/e-Paper \
           -I./software

# Alla C-källkodsfiler
SRCS = main.c \
       firmware/display.c \
       firmware/keyboard.c \
       fonts/wim_font_courier.c \
       lib/Config/DEV_Config.c \
       lib/Config/DEV_hardware_SPI.c \
       lib/Config/RPI_gpiod.c \
       lib/e-Paper/EPD_IT8951.c \
       software/editor.c \
       software/file_io.c \
       software/sync.c

# Generera namn på objektfiler (.o) baserat på källfilerna (.c)
OBJS = $(SRCS:.c=.o)

# Det färdiga programmets namn
TARGET = wimwriter

# Standardmål när du bara skriver 'make'
all: $(TARGET)

# Länkning av binären
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $^ $(LDFLAGS)

# Kompilering av individuella .c till .o
%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# Rensningskommando (kör 'make clean' för att ta bort gamla byggfiler)
clean:
	@echo "Städar upp objektfiler och binär..."
	rm -f $(OBJ_O) $(TARGET)
