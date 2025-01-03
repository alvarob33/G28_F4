#ifndef GOTHAMLIB_H
#define GOTHAMLIB_H

#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include "../config/config.h"
#include "../config/connections.h"


#define MAX_WORKERS 10


// Estructura para almacenar la configuración de Gotham
typedef struct {
    char* ip_fleck;   // Dirección IP del servidor para conectar con Fleck
    int port_fleck;   // Puerto para Fleck
    char* ip_workers; // Dirección IP del servidor para conectar con Harley/Enigma
    int port_workers; // Puerto para Harley/Enigma
} GothamConfig;

typedef struct {
    char* workerType;
    char* IP;
    char* Port;
    int socket_fd;
} Worker;

typedef struct {
    GothamConfig* config;           // Global para poder liberarse con SIGINT
    Server* server_fleck;
    Server* server_worker;

    Worker* workers = NULL;           // Array donde almacenaremos los Workers conectados a Gotham
    int num_workers;
    int enigma_pworker_index;    //Indice del worker(Enigma) principal dentro del array de 'workers' (-1 si no hay)
    int harley_pworker_index;    //Indice del worker(Harley) principal dentro del array de 'workers' (-1 si no hay)
    // Mutex para cuando se modifiquen o lean las variables globales relacionadas con workers
    pthread_mutex_t worker_mutex;

    //Lista de sockets de flecks
    int* fleck_sockets;
    int num_flecks;

    
} GlobalInfoGotham;

typedef struct {
    int socket_connection;
    GlobalInfoGotham* global_info;
} ThreadArgsGotham;


GothamConfig* GOTHAM_read_config(const char *config_file);
void GOTHAM_show_config(GothamConfig* config);

void* handle_fleck_connection(void* client_socket);
void* handle_worker_connection(void* client_socket);

void liberar_memoria_workers();

#endif