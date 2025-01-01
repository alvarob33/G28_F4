#ifndef FLECKLIB_DISTORT_H
#define FLECKLIB_DISTORT_H


#define _GNU_SOURCE

#include <sys/types.h>
#include <sys/wait.h>   // waitpid

#include "../config/config.h"
#include "../config/connections.h"
#include "../gotham/gothamlib.h"
#include "flecklib.h"


#define READ_FILE_BUFFER_SIZE 4096 // Tamaño del buffer para leer el archivo

typedef struct {
    char* username;
    char* filename;
    char* distortion_factor;
    WorkerFleck** worker_ptr;   // Puntero a WorkerFleck* para poder ponerlo en NULL

} DistortInfo;


void sendDistortGotham(char* filename, int socket_gotham, char* mediaType);
TramaResult* receiveDistortGotham(int socket_gotham);
int store_new_worker(TramaResult* result, WorkerFleck** worker, char* workerType);
void freeDistortInfo(DistortInfo* distortInfo);
void* handle_distort_worker(void* arg);



#endif