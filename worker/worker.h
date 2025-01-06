#ifndef WORKER_H
#define WORKER_H

#include <stdio.h>
#include <pthread.h>
#include "../config/connections.h"
#include "../config/config.h"

typedef struct {
    char *ip_gotham;     // Dirección IP para Gotham (dinámico)
    int port_gotham;     // Puerto para Gotham
    char *ip_fleck;      // Dirección IP para Fleck (dinámico)
    int port_fleck;      // Puerto para Fleck
    char *worker_dir;    // Directorio de trabajo para Enigma/Harley (dinámico)
    char *worker_type;   // Tipo de worker ("Media" o "Text") (dinámico)
} Enigma_HarleyConfig;

Enigma_HarleyConfig* WORKER_read_config(const char *config_file);
void WORKER_print_config(Enigma_HarleyConfig* config);

void cancel_and_wait_threads(pthread_t* subthreads, int num_subthreads);

int WORKER_connect_to_gotham(Enigma_HarleyConfig *config, int* isPrincipalWorker);
void* responder_gotham(void *arg);
int WORKER_disconnect_from_gotham(int sock_fd, Enigma_HarleyConfig *config);

#endif