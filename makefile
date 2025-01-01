CC = gcc
CFLAGS = -c
LDFLAGS = -pthread -Wall -Wextra

# Definimos las rutas de las carpetas
SRC_DIRS = gotham fleck worker worker/harley worker/enigma
INCLUDES = $(patsubst %,-I%,$(SRC_DIRS))

# Especificamos las rutas de los archivos fuente (Únicamente utilizado para el clean)
SOURCES = config/config.c config/connections.c\
          gotham/gotham.c gotham/gothamlib.c \
          fleck/fleck.c fleck/flecklib.c fleck/flecklib_distort.c \
          worker/worker.c worker/harley/harley.c worker/enigma/enigma.c \
          worker/harley/harleylib.c worker/enigma/enigmalib.c 

# Convertimos los archivos fuente a archivos objeto (Únicamente utilizado para el clean)
OBJECTS = $(SOURCES:.c=.o)

EXECUTABLES = gotham.exe fleck.exe harley.exe enigma.exe
  
all: $(EXECUTABLES)

# Regla para compilar cada archivo objeto
%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) $< -o $@

gotham.exe: config/config.o config/connections.o gotham/gothamlib.o gotham/gotham.o
	$(CC) $^ -o $@ $(LDFLAGS)

fleck.exe: config/config.o config/connections.o fleck/flecklib_distort.o fleck/flecklib.o fleck/fleck.o
	$(CC) $^ -o $@ $(LDFLAGS)

harley.exe: config/config.o config/connections.o worker/worker.o worker/harley/harleylib.o worker/harley/harley.o
	$(CC) $^ -o $@ $(LDFLAGS)

enigma.exe: config/config.o config/connections.o worker/worker.o worker/enigma/enigmalib.o worker/enigma/enigma.o
	$(CC) $^ -o $@ $(LDFLAGS)

clean:
	rm -f $(OBJECTS) $(EXECUTABLES)

.PHONY: all clean