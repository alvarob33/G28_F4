#ifndef WORKER_DISTORT_H
#define WORKER_DISTORT_H

#include <stdio.h>
#include <sys/mman.h>
#include <sys/stat.h>    // para mkdir

#include "../../config/config.h"
#include "../../config/connections.h"
#include "../../config/files.h"

#define O_BINARY 0      // Para archivos binarios (en sistema linux no se detecta)


// Estructura para manejar las conexiones de los Flecks
typedef struct {
    int socket;
    pthread_t thread_id;
    int active;
} ClientThread;


// Función para manejar la conexión del cliente 
void* handle_fleck_connection(void* client_socket);

#endif