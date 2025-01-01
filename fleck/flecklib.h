#ifndef FLECKLIB_H
#define FLECKLIB_H

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>

// Estructura para almacenar la configuración de Fleck
typedef struct {
    char *username;   // Nombre de usuario 
    char *user_dir;   // Directorio de usuario 
    char *gotham_ip;  // Dirección IP de Gotham 
    int gotham_port;  // Puerto de Gotham
} FleckConfig;

// Struct para almacenar Worker info (no se pueda llamar Worker porque ya se llama así en gothamlib.h)
typedef struct {
    char* IP;   // IP de Worker
    char* Port;  // Puerto de Worker
    char* workerType;
    int socket_fd;

    int status; // Estado de la distorsión en marcha [0-100%]
} WorkerFleck;


FleckConfig* FLECK_read_config(const char *config_file);

void FLECK_handle_menu(FleckConfig *config);

int FLECK_connect_to_gotham(FleckConfig *config);

void FLECK_signal_handler();

#endif