CC = gcc
CFLAGS = -c
LDFLAGS = -pthread -Wall -Wextra
LDLIBS = -lm

# Definimos las rutas de las carpetas
SRC_DIRS = gotham fleck worker worker/harley worker/enigma arkham
INCLUDES = $(patsubst %,-I%,$(SRC_DIRS))

# Especificamos las rutas de los archivos fuente (Únicamente utilizado para el clean)
SOURCES = config/config.c config/connections.c\
          config/files.c \
          gotham/gotham.c gotham/gothamlib.c \
          fleck/fleck.c fleck/flecklib.c fleck/flecklib_distort.c \
          worker/worker.c worker/harley/harley.c worker/enigma/enigma.c \
          worker/harley/harleylib.c worker/enigma/enigmalib.c worker/worker_distort.c\
		  arkham/arkham.c

# Convertimos los archivos fuente a archivos objeto (Únicamente utilizado para el clean)
OBJECTS = $(SOURCES:.c=.o)

EXECUTABLES = gotham.exe fleck.exe harley.exe enigma.exe arkham.exe
  
all: $(EXECUTABLES)

# Regla para compilar cada archivo objeto
%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) $< -o $@ $(LDFLAGS)

gotham.exe: config/config.o config/connections.o config/files.o gotham/gothamlib.o gotham/gotham.o 
	$(CC) $(INCLUDES) $^ -o $@ $(LDFLAGS)

fleck.exe: config/config.o config/connections.o config/files.o fleck/flecklib_distort.o fleck/flecklib.o fleck/fleck.o
	$(CC) $(INCLUDES) $^ -o $@ $(LDFLAGS)

enigma.exe: config/config.o config/connections.o config/files.o worker/enigma/enigmalib.o worker/harley/so_compression.o worker/worker_distort.o worker/worker.o worker/enigma/enigma.o
	$(CC) $(INCLUDES) $^ -o $@ $(LDFLAGS) $(LDLIBS)

harley.exe: config/config.o config/connections.o config/files.o worker/enigma/enigmalib.o worker/harley/so_compression.o worker/worker_distort.o worker/worker.o worker/harley/harley.o
	$(CC) $(INCLUDES) $^ -o $@ $(LDFLAGS) $(LDLIBS)

arkham.exe: config/connections.o config/config.o arkham/arkham.o
	$(CC) $(INCLUDES) $^ -o $@ $(LDFLAGS)

clean:
	rm -f $(OBJECTS) $(EXECUTABLES)

.PHONY: all clean