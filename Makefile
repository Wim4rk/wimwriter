# Kompilator och prestandaflaggor för ARMv6 (Pi Zero W)
CC = gcc
CFLAGS = -O3 -Wall -g -D BCM2835_SPI -D USE_BCM2835

# Sökvägar till Waveshares moduler och headers
DIR_Config = -I./lib/Config 
DIR_Fonts = -I./lib/Fonts
DIR_Graphics = -I./lib/GUI
DIR_IT8951 = -I./lib/IT8951

# Inkluderingskataloger
INCLUDES = -I$(DIR_Config) -I$(DIR_Fonts) -I$(DIR_Graphics) -I$(DIR_IT8951)

# Bibliotek som krävs för SPI, trådar och matematik
LIBS = -lbcm2835 -lpthread -lm

# Källfiler inklusive font24.c
SRCS = main.c \
       $(DIR_Config)/DEV_Config.c \
       $(DIR_Graphics)/GUI_Paint.c \
       $(DIR_IT8951)/IT8951.c \
       $(DIR_Fonts)/font24.c

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
