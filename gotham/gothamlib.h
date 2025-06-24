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
    char* Port;         // Puerto del servidor de Worker (utilizado para recibir conexiones de Flecks)
    int socket_fd;
} Worker;

typedef struct {
    GothamConfig* config;           // Global para poder liberarse con SIGINT
    Server* server_fleck;
    Server* server_worker;

    // WORKER
    Worker* workers;           // Array donde almacenaremos los Workers conectados a Gotham
    int num_workers;
    int enigma_pworker_index;    // Indice del worker(Enigma) principal dentro del array de 'workers' (-1 si no hay)
    int harley_pworker_index;    // Indice del worker(Harley) principal dentro del array de 'workers' (-1 si no hay)
    // Mutex para cuando se modifiquen o lean las variables globales relacionadas con workers
    pthread_mutex_t worker_mutex;

    // FLECK
    int* fleck_sockets;         //Lista de sockets de flecks
    int num_flecks;
    // Mutex para cuando se modifiquen o lean las variables globales relacionadas con workers
    pthread_mutex_t fleck_mutex;

    // Threads de Workers y Fleck
    pthread_t workers_server_thread;
    pthread_t fleck_server_thread;

    // Array de subthreads
    pthread_t* subthreads;      // Threads generados por cada conexión Worker o Fleck
    int num_subthreads;
    pthread_mutex_t subthreads_mutex;

    // Logs
    int log_fd;                // FD del pipe hacia Arkham

} GlobalInfoGotham;

typedef struct {
    int socket_connection;
    GlobalInfoGotham* global_info;
} ThreadArgsGotham;


GothamConfig* GOTHAM_read_config(const char *config_file);
void GOTHAM_show_config(GothamConfig* config);

void liberar_memoria_workers(GlobalInfoGotham* globalInfo);
void liberar_memoria_flecks(GlobalInfoGotham* globalInfo);
void cancel_and_wait_threads(GlobalInfoGotham* globalInfo);

void* handle_fleck_connection(void* client_socket);
void* handle_worker_connection(void* client_socket);

void log_event(GlobalInfoGotham *g, const char *fmt, ...);


#endif