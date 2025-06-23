#ifndef WORKER_DISTORT_H
#define WORKER_DISTORT_H

#include <stdio.h>
#include <sys/mman.h>
#include <sys/stat.h>    // para mkdir

#include "../../config/config.h"
#include "../../config/connections.h"
#include "../../config/files.h"


// Función para manejar la conexión del cliente 
void* handle_fleck_connection(void* client_socket);

#endif