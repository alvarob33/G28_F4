#ifndef STRUCTURES_H
#define STRUCTURES_H

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


// Estructura para almacenar información de distorsión para flecklib_distort.h
typedef struct {
    char* username;
    char *user_dir;
    char* filename;
    char* distortion_factor;
    WorkerFleck** worker_ptr;   // Puntero a WorkerFleck* para poder ponerlo en NULL

    int socket_gotham; // Socket de conexión con Gotham

} DistortInfo;

#endif