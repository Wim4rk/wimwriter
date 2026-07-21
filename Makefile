# Kompilator och prestandaflaggor för ARMv6 (Raspberry Pi Zero W)
CC = gcc
CFLAGS = -O3 -Wall -g -D BCM2835_SPI -D USE_BCM2835

# Katalogsökvägar för projektet
DIR_Config   = ./lib/Config
DIR_EPD      = ./lib/e-Paper
DIR_FONTS    = ./lib/Fonts
DIR_GUI      = ./lib/GUI

# Inkluderingssökvägar (-I. tillåter relativ sökväg från roten)
INCLUDES = -I. -I$(DIR_Config) -I$(DIR_EPD) -I$(DIR_FONTS) -I$(DIR_GUI)

# Länkningsbibliotek (BCM2835 för SPI, pthread för trådar, m för matematik)
LIBS = -lbcm2835 -lpthread -lm

# Källfiler som ingår i kompileringen
SRCS = main.c \
       $(DIR_Config)/DEV_Config.c \
       $(DIR_GUI)/GUI_Paint.c \
       $(DIR_EPD)/EPD_IT8951.c \
       $(DIR_FONTS)/font24.c

# Genererade objektfiler
OBJS = $(SRCS:.c=.o)

# Mål för binärfilen
TARGET = wimwriter

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $(TARGET) $(LIBS)

%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)
