# Kompilator och flaggor
CC = gcc
CFLAGS = -Wall -O2 -D BCM
LDFLAGS = -lbcm2835

# Inkluderingssökvägar
INCLUDES = -Ilib/e-Paper -Ilib/Config

# Källkodsfiler
SRCS = main.c \
       lib/e-Paper/EPD_IT8951.c \
       lib/Config/DEV_Config.c \
       lib/Fonts/font20.c

# Objektfiler (ersätter .c med .o)
OBJS = $(SRCS:.c=.o)

# Målnamn
TARGET = wimwriter

# Standardmål
all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS) $(LDFLAGS)

# Regel för att kompilera .c till .o
%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# Rensning
clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean
