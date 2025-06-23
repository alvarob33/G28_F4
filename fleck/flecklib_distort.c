

#include "flecklib_distort.h"
#include "../config/files.h"

void sendDistortGotham(char* filename, int socket_gotham, char* mediaType) {
    // Preparar trama de distorsión para Gotham
    char* data;
    asprintf(&data, "%s&%s", mediaType, filename);
    unsigned char* trama = crear_trama(TYPE_DISTORT_FLECK_GOTHAM, (unsigned char*)data, strlen(data));

    // Enviar trama de distorsión a Gotham
    if (write(socket_gotham, trama, BUFFER_SIZE) < 0) {
        perror("Error enviando solicitud de distorsión a Gotham");
    } else {
        printF("Solicitud de distorsión enviada a Gotham.\n");
    }

    free(trama);
    free(data);
}

TramaResult* receiveDistortGotham(int socket_gotham) {
    // Recibir respuesta de Gotham
    unsigned char buffer[BUFFER_SIZE];
    int bytes_read = recv(socket_gotham, buffer, BUFFER_SIZE, 0);
    
    if (bytes_read <= 0) {
        if (bytes_read == 0) {
            printF("Gotham ha cerrado la conexión.\n");
        } else {
            perror("Error leyendo mensaje de Gotham.\n");
        }
        close(socket_gotham);
        return NULL;
    }
    buffer[bytes_read] = '\0'; // Asegurar que está null-terminado

    return leer_trama(buffer);
}

// Función para almacenar en sruct Worker la información de la trama recibida por Gotham para distorsión
int store_new_worker(TramaResult* result, WorkerFleck** worker, char* workerType) {
    
    // Crear nuevo worker dinámico
    if (*worker == NULL) {
        *worker = (WorkerFleck *)malloc(sizeof(WorkerFleck));
        if (*worker == NULL) {
            perror("Failed to allocate memory for new worker");
            return 0;
        }
    } else {
        perror("There is a worker of the same type connected.\n");
        return 0;
    }

    // Procesar data con el formato <IP>&<port>
    (*worker)->IP = strdup(strtok(result->data, "&"));
    (*worker)->Port = strdup(strtok(NULL, "&"));
    (*worker)->workerType = workerType;
    (*worker)->socket_fd = -1;     // No definido todavía
    
    free_tramaResult(result);

    // check and print worker info
    if ((*worker)->workerType == NULL || (*worker)->IP == NULL || (*worker)->Port == NULL) {
        printF("Error: Formato de datos inválido.\n");
        return 0;
    }
    
    return 1;
}

// Enviar petición de distort a Gotham y guardar informacion del Worker asignado por Gotham en distortInfo
int request_distort_gotham(int socket_gotham, char* mediaType, WorkerFleck** worker, DistortInfo* distortInfo) {
    // Enviar petición de distort a Gotham (y guardar mediaType del archivo)
    sendDistortGotham(distortInfo->filename, socket_gotham, mediaType);
    //Leer respuesta de Gotham como trama
    TramaResult* result = receiveDistortGotham(socket_gotham);
    if (result == NULL) {
        perror("Error leyendo trama.\n");
        return -1;
    }

    char* buffer;
    // Comprobar si la trama es un mensaje DISTORT
    if (result->type == TYPE_DISTORT_FLECK_GOTHAM)
    {
        // Si no hay Workers de nuestro tipo disponibles salir
        if (strcmp(result->data, "DISTORT_KO") == 0) {
            asprintf(&buffer, "No hay Workers de %s disponibles.\n", mediaType);
            printF(buffer);
            free(buffer);
            free_tramaResult(result);
            return -1;
        } // Si el media no fue reconocido por Gotham salir
        else if (strcmp(result->data, "MEDIA_KO") == 0)
        {
            asprintf(&buffer, "Media type '%s' no reconocido.\n", mediaType);
            printF(buffer);
            free(buffer);
            free_tramaResult(result);
            return -1;
        }
        

        // Si hay Worker disponible
        ///
        
        // Guardar info Worker
        if (store_new_worker(result, worker, mediaType) < 1) {
            perror("Error al guardar el WorkerFleck");
            return -1;
        }
        distortInfo->worker_ptr = worker;

        return 1;

    } else {
        perror("Error: El mensaje recibido de Gotham es inesperado.\n");
        free_tramaResult(result);
        return -1;
    }
}

void freeWorkerFleck(WorkerFleck** worker) {
    // Liberar la memoria de WorkerFleck* si worker no es NULL
    if ((*worker) != NULL) {
        if ((*worker)->IP != NULL) {
            free((*worker)->IP);
        }
        if ((*worker)->Port != NULL) {
            free((*worker)->Port);
        }
        // No se hace FREE porque se asignó con memoria estática
        // if ((*worker)->*workerType != NULL) {
        //     free((*worker)->*workerType);
        // }

        // Cerrar conexión socket con Worker
        if ((*worker)->socket_fd >= 0) {
            close((*worker)->socket_fd);
            (*worker)->socket_fd = -1; // Marcar como cerrado
        }

        // Liberar la memoria de la estructura WorkerFleck y asignarla como NULL
        if (*worker) free(*worker);
        *worker = NULL;
    }
}

void freeDistortInfo(DistortInfo* distortInfo) {
    if (distortInfo == NULL) {
        return;  // Si el puntero es NULL, no hacemos nada
    }
    
    if (distortInfo->username != NULL) {
        free(distortInfo->username);
    }
    if (distortInfo->filename != NULL) {
        free(distortInfo->filename);
    }
    if (distortInfo->distortion_factor != NULL) {
        free(distortInfo->distortion_factor);
    }

    // Liberar la memoria de WorkerFleck* si worker_ptr no es NULL
    freeWorkerFleck(distortInfo->worker_ptr);

    // Finalmente, liberar la estructura DistortInfo en sí misma
    free(distortInfo);
}


// ---- Conectar con servidor Worker ----
int connect_with_worker(WorkerFleck* worker) {
    
    // DEBUGGING:
    printf("Conectando a Worker en %s:%s...\n", worker->IP, worker->Port);

    
    // Crear socket de conexión con Worker
    worker->socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (worker->socket_fd < 0) {
        perror("Error al crear el socket");
        return -1;
    }

    // Configurar la dirección del servidor (Worker)
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr)); // Poner a 0 toda la estructura
    server_addr.sin_family = AF_INET;            // Familia de direcciones: IPv4
    server_addr.sin_port = atoi(worker->Port); // Puerto del Worker (convertido a formato de red)

    // Convertir la IP de string a formato binario y configurarla
    if (inet_pton(AF_INET, worker->IP, &server_addr.sin_addr) <= 0) {
        perror("Error al convertir la IP");
        return -1;
    }

    // Conectar al servidor Worker
    if (connect(worker->socket_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Error al conectar con Worker");
        return -1;
    }

    return 1;
}

int send_start_distort(WorkerFleck* worker, DistortInfo* distortInfo, char* fileSize, char* fileMD5SUM, int init_notContinue) {
    
    // Preparar y enviar la trama inicial de distorsión para Worker
    unsigned char* data;
    asprintf((char**)&data, "%s&%s&%s&%s&%s", distortInfo->username, distortInfo->filename, fileSize, fileMD5SUM, distortInfo->distortion_factor);
    printF((char*)data);
    printF("\n");
    
    unsigned char* tramaEnviar = crear_trama((init_notContinue) ? TYPE_START_DISTORT_FLECK_WORKER : TYPE_RESUME_DISTORT_FLECK_WORKER, data, strlen((char*)data));
    if (write(worker->socket_fd, tramaEnviar, BUFFER_SIZE) < 0) {
        perror("Error enviando respuesta al cliente");
        return -1;
    }
    free(tramaEnviar);


    // Leer la respuesta inicial de distorsión 
    unsigned char response[BUFFER_SIZE];
    int bytes_received = recv(worker->socket_fd, response, BUFFER_SIZE, 0);
    
    TramaResult *result;
    if (bytes_received > 0) {
        // Procesar la trama
        result = leer_trama(response);
        if (result == NULL) {
            printF("Trama inválida recibida de Worker.\n");
            if (result) free_tramaResult(result);
            return -1;
        }

        // Comprobar si responde con OK
        if ((result->type == TYPE_START_DISTORT_FLECK_WORKER && strcmp(result->data, "CON_KO") != 0 && init_notContinue) ||
            (result->type == TYPE_RESUME_DISTORT_FLECK_WORKER && strcmp(result->data, "CON_KO") != 0 && !init_notContinue)) {
            printF("Worker ha aceptado la solicitud de distorsión.\n");

            if (result) free_tramaResult(result);
        } else {
            return -1;
        }
    
        
    } else /*if (bytes_received == 0)*/ {
        // Conexión cerrada por Worker
        return -1;
    }

    return 1;
}

// Función para manejar la solicitud de distorsión
void* handle_distort_worker(void* arg) {
    DistortInfo* distortInfo = (DistortInfo*)arg; // Puntero a Worker* para igualarlo a NULL al final
    WorkerFleck* worker = *distortInfo->worker_ptr;


    if (distortInfo->worker_ptr == NULL ) {
        perror("WorkerFleck** es NULL");
        freeDistortInfo(distortInfo);
        return NULL;
    }
    if (*distortInfo->worker_ptr == NULL) {
        perror("WorkerFleck* es NULL");
        freeDistortInfo(distortInfo);
        return NULL;
    }

    // ---- Conectar con servidor Worker ----
    if (connect_with_worker(worker) < 1) {
        perror("Error al conectar con Worker");
        freeDistortInfo(distortInfo);
        return NULL;
    }

    // ---- Enviar la solicitud de distorsión a Worker ----
    
    // Formar path
    char file_path[256];
    snprintf(file_path, sizeof(file_path), "users%s/%s", distortInfo->user_dir, distortInfo->filename);
    // printF(file_path);

    // Obtener filesize
    char* fileSize = get_string_file_size(file_path);

    // Calcular MD5SUM
    char* fileMD5SUM = calculate_md5sum(file_path);
    if (fileMD5SUM == NULL) {
        perror("Error: Error en Fleck calculando el MD5SUM del archivo.\n");
        freeDistortInfo(distortInfo);
        return NULL;
    }
    
    if (send_start_distort(worker, distortInfo, fileSize, fileMD5SUM, 1) < 1) {
        perror("Error al enviar la solicitud de distorsión al Worker");
        free(fileSize);
        free(fileMD5SUM);
        freeDistortInfo(distortInfo);
        return NULL;
    }
    

    // ---- Enviar archivo a Worker ----
    // Enviar el archivo al Worker mediante tramas de 256 bytes por trama

    int fd;
    if (strcmp(worker->workerType, MEDIA) == 0) {
        fd = open(file_path, O_RDONLY);
    } else {
        fd = open(file_path, O_RDONLY | O_BINARY);
    }
    if (fd < 0) {
        perror("Error al abrir el archivo con open()");
        freeDistortInfo(distortInfo);
        return NULL;
    }

    long file_size = atol(fileSize);  // Tamaño total del archivo en bytes

    long bytes_sent = 0;
    ssize_t bytes_read;

    unsigned char buffer[247]; // solo hay 247 bytes de data útil
    unsigned char response[BUFFER_SIZE];
    TramaResult *result;

    worker->status = 0;
    while ((bytes_read = read(fd, buffer, sizeof(buffer))) > 0) {
        // Enviar trama con fragmento del archivo
        printF((char*) buffer);
        unsigned char* trama = crear_trama(TYPE_FILE_DATA, buffer, bytes_read);
        if (trama == NULL) {
            perror("Error al crear trama de archivo");
            close(fd);
            freeDistortInfo(distortInfo);
            return NULL;
        }

        if (write(worker->socket_fd, trama, BUFFER_SIZE) < 0) {
            perror("Error al enviar trama al Worker");
            free(trama);
            close(fd);
            freeDistortInfo(distortInfo);
            return NULL;
        }
        free(trama);

        // Comprobar si Worker lo recibió correctamente
        int bytes_received = recv(worker->socket_fd, response, BUFFER_SIZE, 0);
        if (bytes_received <= 0) {

            // ---- CAIDA de Worker ----
            perror("Cierre de conexión de Worker, al recibir confirmación");

            freeWorkerFleck(distortInfo->worker_ptr);
            // Enviar petición de distort a Gotham y guardar informacion del Worker asignado por Gotham en distortInfo
            if (request_distort_gotham(distortInfo->socket_gotham, worker->workerType, distortInfo->worker_ptr, distortInfo) > 0) {

                // Cerramos la conexión antigua
                close(worker->socket_fd);
                worker = *distortInfo->worker_ptr; // Actualizar el worker con el nuevo Worker asignado por Gotham
                
                // Intentamos reconectar con el nuevo worker
                if (connect_with_worker(worker) < 1) {
                    perror("Error al reconectar con nuevo Worker");
                    close(fd);
                    freeDistortInfo(distortInfo);
                    return NULL;
                }
                
                // Volvemos a enviar el start_distort
                if (send_start_distort(worker, distortInfo, fileSize, fileMD5SUM, 0) < 1) {
                    perror("Error al reenviar solicitud de distorsión");
                    close(fd);
                    freeDistortInfo(distortInfo);
                    return NULL;
                }

                // Retrocede el puntero del archivo para volver a leer y enviar el mismo fragmento (ya que no se envió por la caida)
                lseek(fd, -bytes_read, SEEK_CUR);

                continue;

            } else {
                // No hay Workers disponibles
                perror("Error: Distorsión cancelada (No hay Workers disponibles).");
                close(fd);
                freeDistortInfo(distortInfo);
                return NULL;
            }

        }

        result = leer_trama(response);
        if (!result || result->type != TYPE_FILE_DATA || strcmp(result->data, OK_MSG) != 0) {
            perror("Error: Trama de Worker inesperada (se esperaba OK_MSG)");
            if (result) free_tramaResult(result);
            close(fd);
            freeDistortInfo(distortInfo);
            return NULL;
        }
        free_tramaResult(result);

        bytes_sent += bytes_read;
        worker->status = (int)((bytes_sent * 100) / file_size*2);   // Por 2 porque se debe enviar y recibir
        // printF(itoa(worker->status));

        // DEBUGGING: Bajar velocidad de envío
        sleep(5);
    }
    printF("Archivo enviado.\n");

    // Recepción del archivo distorsionado

    // Se finalizó la distorsión del archivo
    worker->status = 100;  // Suponemos que el trabajo se completó con éxito

    // Cerrar la conexión
    close(worker->socket_fd);
    printf("Conexión cerrada con el Worker %s:%s\n$ ", worker->IP, worker->Port);
    freeDistortInfo(distortInfo);
    return NULL;
}


