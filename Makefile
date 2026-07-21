# Kompilator och prestandaflaggor för ARMv6 (Pi Zero W)
CC = gcc
CFLAGS = -O3 -Wall -g -D BCM2835_SPI -D USE_BCM2835

# Katalogdeklarationer
DIR_Config   = ./lib/Config
DIR_EPD      = ./lib/e-Paper
DIR_FONTS    = ./lib/Fonts
DIR_GUI      = ./lib/GUI
DIR_Examples = ./examples
DIR_BIN      = ./bin

# Inkluderingskataloger
INCLUDES = -I$(DIR_Config) -I$(DIR_EPD) -I$(DIR_FONTS) -I$(DIR_GUI)

# Bibliotek som krävs för SPI, trådar och matematik
LIBS = -lbcm2835 -lpthread -lm

# Källfiler
SRCS = main.c \
       $(DIR_Config)/DEV_Config.c \
       $(DIR_GUI)/GUI_Paint.c \
       $(DIR_EPD)/EPD_IT8951.c \
       $(DIR_FONTS)/font24.c

# Objektfiler
OBJS = $(SRCS:.c=.o)

# Mål för att bygga binärfilen
TARGET = wimwriter

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $(TARGET) $(LIBS)

%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)
