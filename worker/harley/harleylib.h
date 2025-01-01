#ifndef HARLEYLIB_H
#define HARLEYLIB_H

#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

#include "../../config/config.h"
#include "../../config/connections.h"

// Función para manejar la conexión del cliente 
void* handle_fleck_connection(void* client_socket);

#endif