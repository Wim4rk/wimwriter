# ==========================================
# Makefile för wimwriter (IT8951 Bare-Metal)
# ==========================================a

CC = gcc
TARGET = wimwriter

# Sökvägar till Waveshares drivrutinsbibliotek
DIR_CONFIG = ./lib/Config
DIR_EPD    = ./lib/e-Paper

# Kompilatorflaggor
CFLAGS = -Wall -Wno-format-truncation -O3 -D BCM -DUSE_BCM2835_LIB

LDFLAGS = -lbcm2835 -lm -lrt -lpthread

# Nödvändiga bibliotek (bcm2835 för hårdvara, math, realtid, pthreads)
LIB = -lbcm2835 -lm -lrt -lpthread

# Inkluderingssökvägar för header-filer
INCLUDES = -I./firmware \
           -I./lib/Config \
           -I./lib/e-Paper \
           -I./software \
       	   -I./fonts

# Alla C-källkodsfiler
SRCS = main.c \
       fonts/wim_font_24x43.c \
       firmware/display.c \
       firmware/fast_spi.c \
       firmware/keyboard.c \
       lib/Config/DEV_Config.c \
       lib/e-Paper/EPD_IT8951.c \
       software/editor.c \
       software/file_io.c \
       software/sync.c\
       software/model.c

# Generera namn på objektfiler (.o) baserat på källfilerna (.c)
OBJS = $(SRCS:.c=.o)

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
	rm -f $(OBJS) $(TARGET)
