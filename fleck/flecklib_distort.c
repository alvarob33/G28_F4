#include "flecklib_distort.h"
#include "../config/files.h"

/***********************************************
*
* @Finalitat: Preparar i enviar a Gotham una trama de petició de distorsió amb nom de fitxer i tipus de media.
* @Parametres:
*   in: filename      = nom del fitxer a distorsionar.
*   in: socket_gotham = descriptor del socket Gotham.
*   in: mediaType     = tipus de fitxer ("Media" o "Text").
* @Retorn: ---
*
************************************************/
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

/***********************************************
*
* @Finalitat: Rebre la resposta de Gotham a una petició de distorsió i convertir-la en TramaResult.
* @Parametres:
*   in: socket_gotham = descriptor del socket Gotham.
* @Retorn: Punter a TramaResult amb la resposta, o NULL si hi ha error.
*
************************************************/
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

/***********************************************
*
* @Finalitat: Emmagatzemar en el struct WorkerFleck la IP i port extrets de la trama rebuda per Gotham per distorsionar.
* @Parametres:
*   in:  result    = TramaResult amb "IP&Port".
*   in/out: worker = punter a WorkerFleck* que es crea.
*   in:  workerType = tipus de worker ("Media" o "Text").
* @Retorn: 1 en èxit, 0 en error.
*
************************************************/
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

/***********************************************
*
* @Finalitat: Enviar la petició de distorsió a Gotham i guardar informació del Worker asignat per Gotham en distortInfo
* @Parametres:
*   in:  socket_gotham = descriptor del socket Gotham.
*   in:  mediaType     = tipus de fitxer.
*   in/out: worker     = punter a WorkerFleck* on guardar resultats.
*   in/out: distortInfo= informació de la distorsió.
* @Retorn: 1 si s’assigna worker, -1 en cas de KO o error.
*
************************************************/
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

/***********************************************
*
* @Finalitat: Alliberar la memòria i tancar el socket d’un WorkerFleck quan ja no és necessari.
* @Parametres:
*   in/out: worker = punter a WorkerFleck* a eliminar.
* @Retorn: --- (posa *worker a NULL).
*
************************************************/
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

/***********************************************
*
* @Finalitat: Alliberar tots els camps de DistortInfo incloent WorkerFleck i la pròpia estructura.
* @Parametres:
*   in: distortInfo = punter a DistortInfo a eliminar.
* @Retorn: ---.
*
************************************************/
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
/***********************************************
*
* @Finalitat: Establir connexió TCP amb el WorkerFleck a la seva IP i port.
* @Parametres:
*   in: worker = punter a WorkerFleck amb IP i Port.
* @Retorn: 1 en èxit, -1 en error.
*
************************************************/
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

/***********************************************
*
* @Finalitat: Enviar al Worker la trama inicial de distorsió, incloent user, file, MD5, factor.
* @Parametres:
*   in: worker       = descriptor i info del worker.
*   in: distortInfo  = informació de la distorsió.
*   in: fileSize     = cadena amb size del fitxer.
*   in: fileMD5SUM   = MD5 sum del fitxer.
*   in: init_notContinue = 1 per start, 0 per resume.
* @Retorn: 1 en èxit, -1 en error.
*
************************************************/
int send_start_distort(WorkerFleck* worker, DistortInfo* distortInfo, char* fileSize, char* fileMD5SUM, int init_notContinue) {
    
    // Preparar y enviar la trama inicial de distorsión para Worker
    unsigned char* data;
    asprintf((char**)&data, "%s&%s&%s&%s&%s", distortInfo->username, distortInfo->filename, fileSize, fileMD5SUM, distortInfo->distortion_factor);
    // printF((char*)data);
    // printF("\n");
    
    unsigned char* tramaEnviar = crear_trama((init_notContinue) ? TYPE_START_DISTORT_FLECK_WORKER : TYPE_RESUME_DISTORT_FLECK_WORKER, data, strlen((char*)data));
    if (write(worker->socket_fd, tramaEnviar, BUFFER_SIZE) < 0) {
        perror("Error enviando respuesta al cliente");
        return -1;
    }
    free(data);
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
            if (result) free_tramaResult(result);
            return -1;
        }
    
        
    } else /*if (bytes_received == 0)*/ {
        // Conexión cerrada por Worker
        return -1;
    }

    return 1;
}

/***********************************************
*
* @Finalitat: Esperar confirmació de recepció de fitxer per part del Worker i enviar ACK.
* @Parametres:
*   in: worker = descriptor del socket del Worker.
* @Retorn: 1 en èxit, -1 en error.
*
************************************************/
int wait_confirm_file_received(WorkerFleck* worker) {
    
    // Leer la respuesta final de distorsión 
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

        // Comprobar si recxibimos con CHECK_OK
        if (result->type == TYPE_END_DISTORT_FLECK_WORKER && strcmp(result->data, CHECK_OK) == 0) {
            printF("Archivo enviado correctamente.\n");

            if (result) free_tramaResult(result);
        } else {
            printF("Worker NO ha recibido el archivo correctamente (MD5 Invalido).\n");
            if (result) free_tramaResult(result);
            return -1;
        }

        // Enviar confirmación de recepción (ACK) a Worker
        unsigned char *success_trama = crear_trama(TYPE_END_DISTORT_FLECK_WORKER, (unsigned char*)OK_MSG, strlen(OK_MSG));
        if (write(worker->socket_fd, success_trama, BUFFER_SIZE) < 0) {
            perror("Error enviando confirmación de MD5");
            free(success_trama);

            return -1;
        }
        free(success_trama);
    
        
    } else /*if (bytes_received == 0)*/ {
        // Conexión cerrada por Worker
        printF("Error al recibir trama de Worker.\n");
        return -1;
    }

    return 1;
}


/***********************************************
*
* @Finalitat: Rebre la trama inicial de distorsió del Worker (conté filesize&md5sum) i enviar ACK inicial.
* @Parametres:
*   in:  socket_connection = socket del Worker.
*   out: fileSize    = punter a cadena amb filesize.
*   out: md5sum      = punter a cadena amb md5sum.
* @Retorn: 1 en èxit, 0 si Worker tanca, -1 en error.
*
************************************************/
int receive_start_distort(int socket_connection, char** fileSize, char** md5sum) {
    unsigned char response[BUFFER_SIZE];
    
    int bytes_received = recv(socket_connection, response, BUFFER_SIZE, 0);
    if (bytes_received <= 0) {
        if (bytes_received == 0) {
            // CAIDA de Worker durante distorsión
            return 0;
        }
        perror("Error al recibir solicitud inicial");
        close(socket_connection);
        return -1;
    }

    // Procesar la trama inicial
    TramaResult *result = leer_trama(response);
    if (!result || (result->type != TYPE_START_DISTORT_WORKER_FLECK)) {
        perror("Trama inicial inválida");
        if (result) free_tramaResult(result);
        close(socket_connection);
        return -1;
    }

    // Guardar md5sum y filesize    // (filesize&md5sum)
    if (fileSize && md5sum) {
        *fileSize = strdup(strtok(result->data, "&"));
        *md5sum = strdup(strtok(NULL, "&"));

        if (!fileSize || !md5sum) {
            perror("Formato de datos trama distorsion inicial inválido");
            
            free(*fileSize);
            free(*md5sum);
            close(socket_connection);
            return -1;
        }
    }
    free_tramaResult(result);


    // Enviar ACK de recepción inicial
    unsigned char *ack_trama = crear_trama(TYPE_START_DISTORT_WORKER_FLECK, (unsigned char*)OK_MSG, strlen(OK_MSG));
    if (write(socket_connection, ack_trama, BUFFER_SIZE) < 0) {
        perror("Error enviando confirmación inicial");

        if (fileSize) free(*fileSize);
        if (md5sum) free(*md5sum);
        close(socket_connection);
        return -1;
    }
    free(ack_trama);

    return 1; 
}

/***********************************************
*
* @Finalitat: Gestionar la caiguda d’un Worker durant transmissió, sol·licitar-ne un de nou a Gotham,
*             reconnectar i continuar la distorsió.
* @Parametres:
*   in/out: distortInfo = informació de la distorsió.
*   in/out: worker     = punter a WorkerFleck* actual.
*   in/out: fileSize   = punter a cadena filesize.
*   in/out: fileMD5SUM = punter a cadena md5sum.
* @Retorn: 1 si es recupera, -1 si no hi ha workers disponibles.
*
************************************************/
int handle_caida_worker(DistortInfo* distortInfo, WorkerFleck** worker, char** fileSize, char** fileMD5SUM) {
    // ---- CAIDA de Worker en RX----
    printF("Cierre de conexión de Worker, buscando nuevo Worker disponible...\n");

    sleep(8); // Esperar un segundo para que gotham tenga tiempo de asignar un nuevo Worker

    freeWorkerFleck(distortInfo->worker_ptr);
    // Enviar petición de distort a Gotham y guardar informacion del Worker asignado por Gotham en distortInfo
    if (request_distort_gotham(distortInfo->socket_gotham, (*worker)->workerType, distortInfo->worker_ptr, distortInfo) > 0) {

        // Cerramos la conexión antigua
        close((*worker)->socket_fd);
        *worker = *distortInfo->worker_ptr; // Actualizar el worker con el nuevo Worker asignado por Gotham
        
        // Intentamos reconectar con el nuevo worker
        if (connect_with_worker(*worker) < 1) {
            perror("Error al reconectar con nuevo Worker");
            return -1;
        }

        // Volvemos a enviar el start_distort
        if (send_start_distort(*worker, distortInfo, *fileSize, *fileMD5SUM, 0) < 1) {
            perror("Error al reenviar solicitud de distorsión");
            return -1;
        }

        // Volvemos a recibir la trama inicial de distorsión
        if (receive_start_distort((*worker)->socket_fd, fileSize, fileMD5SUM) < 1) {
            perror("Error al recibir trama inicial de distorsión de vuelta");
            return -1;
        }

        printF("Success: Nuevo Worker encontrado.\n");

        return 1;

    } else {
        // No hay Workers disponibles
        perror("Error: Distorsión cancelada (No hay Workers disponibles).");
        return -1;
    }
}

/***********************************************
*
* @Finalitat: Enviar confirmació final de MD5 correcte al Worker i esperar la confirmació de recepció.
* @Parametres:
*   in: socket_connection = socket del Worker.
* @Retorn: 0 si tot va bé, -1 en error.
*
************************************************/
int send_confirm_file_received (int socket_connection) {
    // Enviar confirmación de que el archivo se recibió correctamente cxon MD5SUM correcto
    unsigned char *success_trama = crear_trama(TYPE_END_DISTORT_FLECK_WORKER, (unsigned char*)CHECK_OK, strlen(CHECK_OK));
    if (write(socket_connection, success_trama, BUFFER_SIZE) < 0) {
        perror("Error enviando confirmación de MD5");
        free(success_trama);

        return -1;
    }
    free(success_trama);

    // Esperar OK de Worker
    unsigned char response[BUFFER_SIZE];
    int bytes_received = recv(socket_connection, response, BUFFER_SIZE, 0);
    
    if (bytes_received > 0) {
        // Procesar la trama
        TramaResult *result = leer_trama(response);
        if (result == NULL) {
            printF("Trama inválida recibida de Worker.\n");
            if (result) free_tramaResult(result);
            return -1;
        }

        // Comprobar si responde con OK
        if (result->type == TYPE_END_DISTORT_FLECK_WORKER && strcmp(result->data, "CON_KO") != 0) {
            // printF("Worker ha recibido la confirmación.\n");

            if (result) free_tramaResult(result);
        } else {
            return -1;
        }
    
        
    } else /*if (bytes_received == 0)*/ {
        // Conexión cerrada por Worker
        return -1;
    }
    return 0;
}

// Función para manejar la solicitud de distorsión
/***********************************************
*
* @Finalitat: Hilo principal per gestionar tot el flux de distorsió: connexió, envoi, recepció,
*             recuperació de caigudes i tancament.
* @Parametres:
*   in: arg = punter a DistortInfo inicialitzat.
* @Retorn: NULL al finalitzar.
*
************************************************/
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

    long file_size = atol(fileSize);  // Tamaño total del archivo en bytes

    // ---- Enviar archivo a Worker ----
    // Enviar el archivo al Worker mediante tramas de 256 bytes por trama

    int fd = open(file_path, O_RDONLY);
    if (fd < 0) {
        perror("Error al abrir el archivo con open()");
        freeDistortInfo(distortInfo);
        return NULL;
    }


    long bytes_sent = 0;
    ssize_t bytes_read;

    unsigned char buffer[247]; // solo hay 247 bytes de data útil
    unsigned char response[BUFFER_SIZE];
    TramaResult *result;
    int bytes_received;

    worker->status = 0;
    while ((bytes_read = read(fd, buffer, sizeof(buffer))) > 0) {

        // Enviar trama con fragmento del archivo
        // printF((char*) buffer);
        unsigned char* trama = crear_trama(TYPE_FILE_DATA, buffer, bytes_read);
        if (trama == NULL) {
            perror("Error al crear trama de archivo");
            close(fd);
            freeDistortInfo(distortInfo);
            return NULL;
        }

        if (write(worker->socket_fd, trama, BUFFER_SIZE) != BUFFER_SIZE/*< 0*/) {
            perror("Error al enviar trama al Worker");
            free(trama);
            close(fd);
            freeDistortInfo(distortInfo);
            return NULL;
        }
        free(trama);

        // Comprobar si Worker lo recibió correctamente
        bytes_received = recv(worker->socket_fd, response, BUFFER_SIZE, 0);
        if (bytes_received <= 0) {

            // ---- CAIDA de Worker en TX ----
            
            printF("Cierre de conexión de Worker, buscando nuevo Worker disponible...\n");

            sleep(8); // Esperar un segundo para que gotham tenga tiempo de asignar un nuevo Worker

            freeWorkerFleck(distortInfo->worker_ptr);
            // Enviar petición de distort a Gotham y guardar informacion del Worker asignado por Gotham en distortInfo
            if (request_distort_gotham(distortInfo->socket_gotham, worker->workerType, distortInfo->worker_ptr, distortInfo) > 0) {

                // Cerramos la conexión antigua
                close(worker->socket_fd);   // No necesario
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

                printF("Success: Nuevo Worker encontrado.\n");

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
        worker->status = (int)((bytes_sent * 100) / (file_size*2));   // Por 2 porque se debe enviar y recibir
        // printF(itoa(worker->status));

        // DEBUGGING: Bajar velocidad de envío
        // usleep(100000);
    }

    // ---- Comprobar si se envió todo el archivo correctamente mediante MD5SUM----
    close(fd);

    if (wait_confirm_file_received(worker) < 1) {
        perror("Error al esperar confirmación de archivo recibido por Worker");
        freeDistortInfo(distortInfo);
        return NULL;
    }

    bytes_sent = 0;
    
    // ---- Recepción del archivo distorsionado ----

    // Recibir trama inicial envio archivo distorsionado
    free(fileSize);
    free(fileMD5SUM);
    int result_func = receive_start_distort(worker->socket_fd, &fileSize, &fileMD5SUM);
    if (result_func < 0) {
        perror("Error al recibir trama inicial de distorsión");
        freeDistortInfo(distortInfo);
        return NULL;

    } else if (result_func == 0) {
        
        // CAIDA de Worker mientras distorsionaba
        if (handle_caida_worker(distortInfo, &worker, &fileSize, &fileMD5SUM) < 1) {
            perror("Error al manejar la caída del Worker");
            freeDistortInfo(distortInfo);
            return NULL;
        }
    }

    char* distorted_file_path = NULL;
    asprintf(&distorted_file_path, "users%s/%s_distorted", distortInfo->user_dir, distortInfo->filename);

    printF("Recibiendo archivo distorsionado...\n");
    
    // Recibir el archivo distorsionado en fragmentos
    int fd_distorted = open(distorted_file_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd_distorted < 0) {
        perror("Error al crear archivo distorsionado");
        free(distorted_file_path);
        freeDistortInfo(distortInfo);
        return NULL;
    }
    
    long total_bytes_received = 0;
    long distorted_filesize = atol(fileSize);
    
    while (total_bytes_received < distorted_filesize) {
        bytes_received = recv(worker->socket_fd, response, BUFFER_SIZE, 0);
        if (bytes_received <= 0) {

            // ---- CAIDA de Worker en RX----
            if (handle_caida_worker(distortInfo, &worker, &fileSize, &fileMD5SUM) < 1) {
                perror("Error al manejar la caída del Worker");
                close(fd);
                freeDistortInfo(distortInfo);

                return NULL;
            }

            continue; 
        }

        result = leer_trama(response);
        if (!result || result->type != TYPE_FILE_DATA) {
            perror("Trama de datos distorsionados inválida");
            // printF(result->data);
            if (result) free_tramaResult(result);
            close(fd_distorted);
            free(distorted_file_path);
            freeDistortInfo(distortInfo);
            return NULL;
        }

        ssize_t bytes_written = write(fd_distorted, result->data, result->data_length);
        if (bytes_written != (ssize_t)result->data_length) {
            perror("Error escribiendo archivo distorsionado");
            free_tramaResult(result);
            close(fd_distorted);
            free(distorted_file_path);
            freeDistortInfo(distortInfo);
            return NULL;
        }
        free_tramaResult(result);

        // Enviar confirmación de recepción
        unsigned char* ack_trama = crear_trama(TYPE_FILE_DATA, (unsigned char*)OK_MSG, strlen(OK_MSG));
        if (write(worker->socket_fd, ack_trama, BUFFER_SIZE) < 0) {
            perror("Error enviando confirmación de recepción");
            free(ack_trama);
            close(fd_distorted);
            free(distorted_file_path);
            freeDistortInfo(distortInfo);
            return NULL;
        }
        free(ack_trama);

        total_bytes_received += bytes_written;
        worker->status = 50 + (int)((total_bytes_received)*50 / distorted_filesize); // 50-100%

        // DEBUGGING: Bajar velocidad de recepción
        usleep(100000);
    }

    close(fd_distorted);


    // ---- Comprobar MD5 del archivo recibido ----

    char *calculated_md5 = calculate_md5sum(distorted_file_path);
    if (calculated_md5 == NULL) {
        perror("Error calculando MD5 del archivo recibido");
    }

    // Enviar trama al cliente en base al resultado del MD5
    if (!calculated_md5 || strcmp(calculated_md5, fileMD5SUM) != 0) {
        unsigned char *error_trama = crear_trama(TYPE_END_DISTORT_FLECK_WORKER, (unsigned char*)CHECK_KO, strlen(CHECK_KO));
        if (write(worker->socket_fd, error_trama, BUFFER_SIZE) < 0) {
            perror("Error enviando mensaje de MD5 no coincidente");
        } else {
            printF("Enviado: MD5 del archivo recibido no coincide con el esperado\n");
        }
        free(error_trama);

        free(fileMD5SUM);
        if (calculated_md5) free(calculated_md5);
        free(distorted_file_path);
        free(fileSize);
        freeDistortInfo(distortInfo);
        return NULL;
    }

    free(fileMD5SUM);
    free(calculated_md5);
    free(distorted_file_path);
    free(fileSize);

    if (send_confirm_file_received(worker->socket_fd) != 0) {
        perror("Error enviando confirmación de recepción del archivo con MD5SUM correcto");
        freeDistortInfo(distortInfo);
        return NULL;
    }


    // ---- Final ----

    // Se finalizó la distorsión del archivo
    worker->status = 100;  // Suponemos que el trabajo se completó con éxito

    // Cerrar la conexión
    close(worker->socket_fd);
    printF("Success: Archivo distorsionado correctamente y conexión cerrada con Worker\n$ ");
    freeDistortInfo(distortInfo);
    return NULL;
}