#ifndef ENIGMALIB_H
#define ENIGMALIB_H

#include <stdio.h>

#include "../../config/config.h"
#include "../../config/connections.h"

// Función para manejar la conexión del cliente 
void* handle_fleck_connection(void* client_socket);

#endif