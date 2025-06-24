#ifndef CONNECTIONS_H
#define CONNECTIONS_H

#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <dirent.h>
#include <signal.h>
#include <arpa/inet.h>

#include "config.h"

#define MAX_CONNECTIONS 10
#define HEARTBEAT "HEARTBEAT"
#define HEARTBEAT_SLEEP_TIME 5
#define OK_MSG "OK"
#define CHECK_OK "CHECK_OK"
#define CHECK_KO "CHECK_KO"
#define BUFFER_SIZE 256

/* CONNECTION TYPEs */
#define TYPE_CONNECT_FLECK_GOTHAM 0x01          // Conexiones entre Fleck y Gotham
#define TYPE_CONNECT_WORKER_GOTHAM 0x02         // Conexiones entre Worker y Gotham
#define TYPE_DISTORT_FLECK_GOTHAM 0x10          // Peticiones distort entre Fleck y Gotham
#define TYPE_RESUME_DISTORT_FLECK_GOTHAM 0x11   // Resumir distorsión entre Fleck y Gotham
#define TYPE_START_DISTORT_FLECK_WORKER 0x03    // Empezar a enviar archivo para distorsionar (de Fleck a Worker)
#define TYPE_RESUME_DISTORT_FLECK_WORKER 0x13   // Continuar con distorsión (de Fleck a Worker)
#define TYPE_START_DISTORT_WORKER_FLECK 0x04    // Empezar a enviar archivo distorsionado, de Worker a Fleck
#define TYPE_FILE_DATA 0x05                     // Envío de archivos 
#define TYPE_END_DISTORT_FLECK_WORKER 0x06      // Envío de archivo finalizado
#define TYPE_DISCONNECTION 0x07                 // Desconexión de cualquier tipo
#define TYPE_PRINCIPAL_WORKER 0x08              // Asignación de un nuevo Worker principal
#define TYPE_ERROR 0x09                         // Error recibiendo la trama
#define TYPE_HEARTBEAT 0x12                     // Conexiones HEARTBEAT
#define TYPE_LOG 0x20


// Estructura para guardar información de un servidor
typedef struct {
    int server_fd;                  // File escriptor del socket del servidor
    struct sockaddr_in address;     // Dirección del servidor
    int port;                       // Puerto del servidor
    int max_connections;            // Número máximo de conexiones en espera
} Server;

// Estructura para encapsular el resultado de leer_trama
typedef struct {
    char type;
    char *timestamp;   // El timestamp en formato de string
    char *data;        // Los datos del mensaje
    int data_length;  // Longitud de los datos válidos (en bytes)
} TramaResult;

// Funciones para crear servidor
Server* create_server(char* ip_addr, int port, int backlog);
void start_server(Server *server);
void close_server(Server *server);
// Funcion para escuchar conexiones de un servidor
int accept_connection(Server *server);

// Funciones para trabajar con tramas
unsigned char* crear_trama(int TYPE, unsigned char* data, size_t data_length);
TramaResult* leer_trama(unsigned char *trama);  // Comprueba que el checksum sea correcto y devuelve la data del mensaje
// Libera la memoria de TramaResult
void free_tramaResult(TramaResult *result);

// Funciones heartbeat
void enviar_heartbeat_constantemente(int socket_fd);
void *responder_heartbeat_constantemente(void *arg);


#endif