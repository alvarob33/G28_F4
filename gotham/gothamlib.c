#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>
#include <stdarg.h>

#include "../worker/worker.h"
#include "gothamlib.h"

// ARCHIVO CONFIGURACIÓN

/***********************************************
*
* @Finalitat: Llegir i parsejar el fitxer de configuració de Gotham.
* @Paràmetres: in: config_file = ruta al fitxer de configuració.
* @Retorn: Punter a GothamConfig amb la configuració carregada o NULL en cas d’error.
*
************************************************/
GothamConfig* GOTHAM_read_config(const char *config_file) {
    char* buffer;

    int fd = open(config_file, O_RDONLY);
    if (fd < 0) {
        perror("Error abriendo el archivo de configuración");
        return NULL;
    }

    GothamConfig* config = (GothamConfig*) malloc(sizeof(GothamConfig));
    if (config == NULL) {
        perror("Error en malloc");
        close(fd);
        return NULL;
    }

    // Leer la IP para Fleck
    config->ip_fleck = read_until(fd, '\n');
    if (config->ip_fleck == NULL) {
        perror("Error leyendo la IP de Fleck");
        free(config);
        close(fd);
        return NULL;
    }

    // Leer el puerto para Fleck
    buffer = read_until(fd, '\n');
    if (buffer == NULL) {
        perror("Error leyendo el puerto de Fleck");
        free(config->ip_fleck); // Liberar la IP leída previamente
        free(config);
        close(fd);
        return NULL;
    }
    config->port_fleck = atoi(buffer); // Convertir string a entero
    free(buffer); // Liberar el buffer del puerto

    // Leer la IP para Harley/Enigma
    config->ip_workers = read_until(fd, '\n');
    if (config->ip_workers == NULL) {
        perror("Error leyendo la IP de Harley/Enigma");
        free(config->ip_fleck); // Liberar la memoria previamente asignada
        free(config);
        close(fd);
        return NULL;
    }

    // Leer el puerto para Harley/Enigma
    buffer = read_until(fd, '\n');
    if (buffer == NULL) {
        perror("Error leyendo el puerto de Harley/Enigma");
        free(config->ip_fleck);
        free(config->ip_workers); // Liberar la memoria previamente asignada
        free(config);
        close(fd);
        return NULL;
    }
    config->port_workers = atoi(buffer); // Convertir string a entero
    free(buffer); // Liberar el buffer del puerto

    close(fd);
    return config; // Devolver la configuración
}

/***********************************************
*
* @Finalitat: Mostrar per pantalla la configuració llegida de Gotham.
* @Paràmetres: in: config = punter a GothamConfig amb els valors a mostrar.
* @Retorn: ----
*
************************************************/
void GOTHAM_show_config(GothamConfig* config) {
    // Mostrar la configuración leída
    char* buffer;
    printF("Gotham Config:\n");
    asprintf(&buffer, "IP Fleck: %s\n", config->ip_fleck);
    printF(buffer);
    free(buffer);
    asprintf(&buffer, "Puerto Fleck: %d\n", config->port_fleck);
    printF(buffer);
    free(buffer);
    asprintf(&buffer, "IP Workers (Harley/Enigma): %s\n", config->ip_workers);
    printF(buffer);
    free(buffer);
    asprintf(&buffer, "Puerto Workers (Harley/Enigma): %d\n\n", config->port_workers);
    printF(buffer);
    free(buffer);
}

// LIBERAR MEMORIA

/***********************************************
*
* @Finalitat: Alliberar la memòria d’un Worker i tancar el seu socket.
* @Paràmetres: in: worker = còpia de l’estructura Worker a alliberar.
* @Retorn: ----
*
************************************************/
void liberar_memoria_worker(Worker worker) {

    free(worker.workerType);
    free(worker.IP);     
    free(worker.Port);  
    
    if (worker.socket_fd > 0) {
        close(worker.socket_fd);    // Cerrar socket Gotham-Worker
    }

}

/***********************************************
*
* @Finalitat: Alliberar la memòria de tots els Workers registrats a Gotham.
* @Paràmetres: in: globalInfo = punter a l’estat global de Gotham.
* @Retorn: ----
*
************************************************/
void liberar_memoria_workers(GlobalInfoGotham* globalInfo) {
    if (globalInfo->workers == NULL) {
        // printF("No hay Workers conectados\n");
        return;
    }

    for (int i = 0; i < globalInfo->num_workers; i++) {
        liberar_memoria_worker(globalInfo->workers[i]);
    }

    free(globalInfo->workers); 
}

/***********************************************
*
* @Finalitat: Tancar sockets i alliberar l’array de connexions de Flecks.
* @Paràmetres: in: globalInfo = punter a l’estat global de Gotham.
* @Retorn: ----
*
************************************************/
void liberar_memoria_flecks(GlobalInfoGotham* globalInfo) {
    if (globalInfo->fleck_sockets == NULL) {
        // printF("No hay Flecks conectados\n");
        return;
    }

    for (int i = 0; i < globalInfo->num_flecks; i++) {
        if (globalInfo->fleck_sockets[i] > 0) {
            close(globalInfo->fleck_sockets[i]);    // Cerrar socket Gotham-Fleck
        }
    }

    free(globalInfo->fleck_sockets);  // Liberar el array de sockets de Flecks
}

/***********************************************
*
* @Finalitat: Cancel·lar tots els threads actius i esperar la seva finalització,
*             incloent-hi els threads de servidor de Workers i Flecks.
* @Paràmetres: in: globalInfo = punter a l’estat global de Gotham.
* @Retorn: ----
*
************************************************/
void cancel_and_wait_threads(GlobalInfoGotham* globalInfo) {

    // Cerrar y liberar subthreads
    for (int i = 0; i < globalInfo->num_subthreads; i++) {
        if (pthread_cancel(globalInfo->subthreads[i]) != 0) {
            perror("Error al cancelar el thread");
        }
        
        // Esperamos a que el thread termine
        if (pthread_join(globalInfo->subthreads[i], NULL) != 0) {
            // perror("Error al esperar el thread");
        }
    }
    
    free(globalInfo->subthreads);
    globalInfo->num_subthreads = 0; 


    // Cerrar threads de Servidor_Workers y Servidor_Flecks
    pthread_cancel(globalInfo->workers_server_thread);
    pthread_cancel(globalInfo->fleck_server_thread);
    
    pthread_join(globalInfo->fleck_server_thread, NULL);
    pthread_join(globalInfo->workers_server_thread, NULL);
}

/***********************************************
*
* @Finalitat: Gestionar la connexió d’un Fleck entrant:
*             - Llegir trames del socket
*             - Processar els comandaments CONNECT i DISTORT
*             - Enviar respostes adequades a Fleck
* @Paràmetres: in: void_args = punter a ThreadArgsGotham amb socket i info global.
* @Retorn: NULL en finalitzar la connexió.
*
************************************************/
void* handle_fleck_connection(void* void_args) {
    ThreadArgsGotham* args = (ThreadArgsGotham *)void_args;
    GlobalInfoGotham* globalInfo = args->global_info;
    int socket_fd = args->socket_connection;
    free(void_args);    // Liberamos porque solo lo utilizamos para pasar parámetros al thread


    unsigned char buffer[BUFFER_SIZE];
    int bytes_read;

    // Leer constantemente las tramas de Fleck (hasta que desconecte)
    while ((bytes_read = recv(socket_fd, buffer, BUFFER_SIZE, 0)) > 0) {
        // Procesar la trama
        TramaResult *result = leer_trama(buffer);
        if (result == NULL || result->data == NULL) {
            printF("Trama inválida recibida de Fleck.\n");
            if (result) free_tramaResult(result);
            continue;
        }
        //PRINTF DEBBUGGING
        // printf("Trama recibida: TYPE=0x%02x, DATA=%s\n", buffer[0], &buffer[3]);


        /* Validar el TYPE de la trama */
        if (result->type == TYPE_CONNECT_FLECK_GOTHAM) 
        {  
            // Comando CONNECT
            printF("Comando CONNECT recibido de Fleck.\n");

            // Parsear los datos: <username>&<IP>&<Port> (duplicándolos para poder liberar memoria de result)
            char *username = strdup(strtok(result->data, "&"));
            char *ip = strdup(strtok(NULL, "&"));
            char *port = strdup(strtok(NULL, "&"));

            if (username && ip && port) {
                //PRINTF
                char *mensaje = NULL;
                asprintf(&mensaje, "Fleck conectado: %s, IP: %s, Puerto: %s\n", username, ip, port);
                printf(mensaje);
                log_event(globalInfo, mensaje);
                free(mensaje);

                // Responder con OK
                unsigned char *response = crear_trama(TYPE_CONNECT_FLECK_GOTHAM, (unsigned char*)"", strlen(""));  // DATA vacío
                if (write(socket_fd, response, BUFFER_SIZE) < 0) {
                    perror("Error enviando respuesta OK a Fleck");
                }
                free(response);
                // printF("Respuesta de OK enviada a Fleck.\n");


            } else {
                // Responder con CON_KO si el formato es incorrecto
                unsigned char *response = crear_trama(TYPE_CONNECT_FLECK_GOTHAM, (unsigned char*)"CON_KO", strlen("CON_KO"));
                if (write(socket_fd, response, BUFFER_SIZE) < 0) {
                    perror("Error enviando respuesta CON_KO a Fleck");
                }
                free(response);
                printF("Formato de conexión inválido. Respuesta CON_KO enviada.\n");
            }
        } else if (result->type == TYPE_DISTORT_FLECK_GOTHAM) 
        {
            // Comando DISTORT
            printF("Comando DISTORT recibido de Fleck.\n");
            log_event(globalInfo, "Comando DISTORT recibido de Fleck.");
            
            // Parsear los datos: <mediaType>&<fileName> (duplicándolos para poder liberar memoria de result)
            char *mediaType = strdup(strtok(result->data, "&"));
            char *fileName = strdup(strtok(NULL, "&"));

            free_tramaResult(result); // Liberar la trama procesada

            // Comprobar si hay Workers para el archivo solicitado
            if ((strcmp(mediaType, MEDIA) == 0 && globalInfo->harley_pworker_index < 0) || (strcmp(mediaType, TEXT) == 0 && globalInfo->enigma_pworker_index < 0))
            {
                // Responder con DISTORT_KO
                unsigned char *response = crear_trama(TYPE_DISTORT_FLECK_GOTHAM, (unsigned char*)"DISTORT_KO", strlen("DISTORT_KO")); 
                if (write(socket_fd, response, BUFFER_SIZE) < 0) {
                    perror("Error enviando respuesta DISTORT_KO a Fleck");
                }
                free(response);
                printF("Sin Workers disponibles. Respuesta de DISTORT_KO enviada a Fleck.\n");

                continue;
            }

            // Enviar datos Worker
            if (strcmp(mediaType, MEDIA) == 0)
            {
                // Responder con datos Worker Harley principal
                char* data;
                asprintf(&data, "%s&%s", globalInfo->workers[globalInfo->harley_pworker_index].IP, globalInfo->workers[globalInfo->harley_pworker_index].Port);
                unsigned char *response = crear_trama(TYPE_DISTORT_FLECK_GOTHAM, (unsigned char*)data, strlen(data)); 
                free(data);

                if (write(socket_fd, response, BUFFER_SIZE) < 0) {
                    perror("Error enviando respuesta DISTORT a Fleck");
                }
                free(response);
                printF("Worker Harley pricipal enviado a Fleck.\n");
                log_event(globalInfo, "Respuesta con Worker Harley pricipal enviado a Fleck.");
            } else if (strcmp(mediaType, TEXT) == 0)
            {
                // Responder con datos Worker Enigma principal
                char* data;
                asprintf(&data, "%s&%s", globalInfo->workers[globalInfo->enigma_pworker_index].IP, globalInfo->workers[globalInfo->enigma_pworker_index].Port);
                unsigned char *response = crear_trama(TYPE_DISTORT_FLECK_GOTHAM, (unsigned char*)data, strlen(data)); 
                free(data);

                if (write(socket_fd, response, BUFFER_SIZE) < 0) {
                    perror("Error enviando respuesta DISTORT a Fleck");
                }
                free(response);
                printF("Worker Enigma pricipal enviado a Fleck.\n");
                log_event(globalInfo, "Respuesta con Worker Enigma pricipal enviado a Fleck.");
            } else {
                // Responder con MEDIA_KO
                unsigned char *response = crear_trama(TYPE_DISTORT_FLECK_GOTHAM, (unsigned char*)"MEDIA_KO", strlen("MEDIA_KO")); 
                if (write(socket_fd, response, BUFFER_SIZE) < 0) {
                    perror("Error enviando respuesta MEDIA_KO a Fleck");
                }
                free(response);

                char* buffer;
                asprintf(&buffer, "Media type '%s' no reconocido. Respuesta de MEDIA_KO enviada a Fleck.\n", mediaType);
                printF(buffer);
                free(buffer);
                log_event(globalInfo, "Media del comando DISTORT de Fleck no reconocida.");
            }
            free(mediaType);
            free(fileName);
            
        }
        
    }

    if (bytes_read == 0) {
        printF("Fleck desconectado.\n");
    } else if (bytes_read < 0) {
        perror("Error al recibir datos de Fleck");
    }

    close(socket_fd);
    return NULL;
}

/***********************************************
*
* @Finalitat: Cercar l’índex d’un Worker donat el seu descriptor de socket.
* @Paràmetres: in: globalInfo = punter a l’estat global de Gotham.
*             in: socket_fd = descriptor de socket del Worker.
* @Retorn: Índex a l’array de workers o -1 si no es troba.
*
************************************************/
int find_worker_bySocket(GlobalInfoGotham* globalInfo, int socket_fd) {
    for (int i = 0; i < globalInfo->num_workers; i++) {
        if (globalInfo->workers[i].socket_fd == socket_fd) {
            return i; // Índice encontrado
        }
    }
    return -1; // No se encontró el Worker
}

/***********************************************
*
* @Finalitat: Emmagatzemar un nou Worker a la llista global:
*             - Reassignar memòria
*             - Parsejar les dades de la trama rebuda
*             - Actualitzar comptadors i notificar l’esdeveniment
* @Paràmetres: in: globalInfo = punter a l’estat global de Gotham.
*             in: result = TramaResult amb les dades del Worker.
* @Retorn: 1 si té èxit, 0 en cas d’error.
*
************************************************/
int store_new_worker(GlobalInfoGotham* globalInfo, TramaResult* result) {
    
    // Comprobar que no se supere número máximo de Workers
    if (globalInfo->num_workers >= MAX_WORKERS) {
        pthread_mutex_unlock(&globalInfo->worker_mutex);
        printF("Error: No se pudo agregar el worker. Límite de workers alcanzado.\n");
        log_event(globalInfo, "Error: No se pudo agregar el worker. Límite de workers alcanzado.\n");
        return 0;
    }
    
    // Crear nuevo worker dinámico
    if (globalInfo->workers == NULL)
    {
        globalInfo->workers = (Worker *)malloc(sizeof(Worker));
        if (globalInfo->workers == NULL) {
            pthread_mutex_unlock(&globalInfo->worker_mutex);
            perror("Failed to allocate memory for new worker");
            return 0;
        }

    } else {
        Worker* temp = realloc(globalInfo->workers, (globalInfo->num_workers + 1) * sizeof(Worker));
        if (globalInfo->workers == NULL) {
            free(globalInfo->workers);
            pthread_mutex_unlock(&globalInfo->worker_mutex);
            perror("Failed to reallocate memory for workers array");
            return 0;
        }
        globalInfo->workers = temp;
    }

    // Procesar data con el formato <workerType>&<IP>&<Port>
    globalInfo->workers[globalInfo->num_workers].workerType = strdup(strtok(result->data, "&"));
    globalInfo->workers[globalInfo->num_workers].IP = strdup(strtok(NULL, "&"));
    globalInfo->workers[globalInfo->num_workers].Port = strdup(strtok(NULL, "&"));
    free_tramaResult(result);

    // check and print worker info
    if (globalInfo->workers[globalInfo->num_workers].workerType == NULL || globalInfo->workers[globalInfo->num_workers].IP == NULL || globalInfo->workers[globalInfo->num_workers].Port == NULL) {
        printF("Error: Formato de datos inválido.\n");
        return 0;
    }

    char* aux;
    asprintf(&aux, "New worker added: workerType=%s, IP=%s, Port=%s\n",
           globalInfo->workers[globalInfo->num_workers].workerType, globalInfo->workers[globalInfo->num_workers].IP, globalInfo->workers[globalInfo->num_workers].Port);
    printF(aux);
    log_event(globalInfo, aux);
    free(aux);

    globalInfo->num_workers++;
    
    return 1;
}

/***********************************************
*
* @Finalitat: Eliminar un Worker de la llista global:
*             - Tancar el seu socket i alliberar memòria
*             - Reajustar l’array i reassignar memòria
*             - Assignar un nou principal si cal
* @Paràmetres: in: globalInfo = punter a l’estat global de Gotham.
*             in: socket_fd = descriptor de socket del Worker a eliminar.
* @Retorn: ----
*
************************************************/
void remove_worker(GlobalInfoGotham* globalInfo, int socket_fd) {
    pthread_mutex_lock(&globalInfo->worker_mutex);

    int index = find_worker_bySocket(globalInfo, socket_fd);
    if (index < 0) {
        perror("Error al buscar Worker mediante su socket.");
        pthread_mutex_unlock(&globalInfo->worker_mutex);
        return;
    }

    liberar_memoria_worker(globalInfo->workers[index]);
    // Mover los elementos restantes hacia adelante para rellenar hueco
    for (int i = index; i < globalInfo->num_workers - 1; i++) {
        globalInfo->workers[i] = globalInfo->workers[i + 1];
    }

    globalInfo->num_workers--;
    Worker* temp = realloc(globalInfo->workers, globalInfo->num_workers * sizeof(Worker));
    if (globalInfo->workers == NULL && globalInfo->num_workers > 0) {
        free(globalInfo->workers);
        perror("Error al realocar memoria para workers.");
        pthread_mutex_unlock(&globalInfo->worker_mutex);
        return;
    }
    globalInfo->workers = temp;

    // Comprobar si era un Worker principal, y en dicho caso asignar a uno nuevo
    if (index == globalInfo->enigma_pworker_index) {
        globalInfo->enigma_pworker_index = -1;  // Borrar el índice de Enigma Principal Worker

        // Buscar un nuevo worker de tipo "Text"
        for (int i = 0; i < globalInfo->num_workers; i++) {
            if (strcmp(globalInfo->workers[i].workerType, "Text") == 0) {

                // WORKER ENCONTRADO
                globalInfo->enigma_pworker_index = i;

                // Crear trama informando que es el nuevo Principal Worker
                unsigned char* trama;
                trama = crear_trama(TYPE_PRINCIPAL_WORKER, (unsigned char*)"", strlen(""));
                if (trama == NULL) {
                    printF("Error en malloc para trama\n");
                    pthread_mutex_unlock(&globalInfo->worker_mutex);
                    return;
                }

                // Enviar la trama a Worker
                if (write(globalInfo->workers[i].socket_fd, trama, BUFFER_SIZE) < 0) {
                    printF("Error enviando la trama de conexión a Gotham\n");
                    free(trama);
                    pthread_mutex_unlock(&globalInfo->worker_mutex);
                    return;
                }
                free(trama);

                // Mostrar mensaje indicando que encontramos un nuevo Principal Worker
                char* buffer;
                asprintf(&buffer, "Nuevo Principal Worker de tipo 'Text' encontrado en el índice %d.\n", globalInfo->enigma_pworker_index);
                log_event(globalInfo, buffer);
                printF(buffer);
                free(buffer);
                break;
            }
        }

        if (globalInfo->enigma_pworker_index == -1) {
            printF("No hay Workers de tipo 'Text' para asignar como Principal Worker.\n");
            log_event(globalInfo, "No hay Workers de tipo 'Text' para asignar como Principal Worker.");
            
        }
    } else if (index == globalInfo->harley_pworker_index) {
        globalInfo->harley_pworker_index = -1;  // Borrar el índice de Harley Principal Worker

        // Buscar un nuevo worker de tipo "Media"
        for (int i = 0; i < globalInfo->num_workers; i++) {
            if (strcmp(globalInfo->workers[i].workerType, "Media") == 0) {
                
                // WORKER ENCONTRADO
                globalInfo->harley_pworker_index = i;

                // Crear trama informando que es el nuevo Principal Worker
                unsigned char* trama;
                trama = crear_trama(TYPE_PRINCIPAL_WORKER, (unsigned char*)"", strlen(""));
                if (trama == NULL) {
                    printF("Error en malloc para trama\n");
                    pthread_mutex_unlock(&globalInfo->worker_mutex);
                    return;
                }

                // Enviar la trama a Worker
                if (write(globalInfo->workers[i].socket_fd, trama, BUFFER_SIZE) < 0) {
                    printF("Error enviando la trama de conexión a Gotham\n");
                    free(trama);
                    pthread_mutex_unlock(&globalInfo->worker_mutex);
                    return;
                }
                free(trama);

                // Mostrar mensaje indicando que encontramos un nuevo Principal Worker
                char* buffer;
                asprintf(&buffer, "Nuevo pworker de tipo 'Media' encontrado en el índice %d.\n", globalInfo->harley_pworker_index);
                log_event(globalInfo, buffer);
                printF(buffer);
                free(buffer);
                break;
            }
        }
        if (globalInfo->harley_pworker_index == -1) {
            printF("No hay Workers de tipo 'Media' para asignar como Principal Worker.\n");
            log_event(globalInfo, "No hay Workers de tipo 'Text' para asignar como Principal Worker.");
        }
    }

    // printf("Worker en índice %d eliminado correctamente.\n", index);
    pthread_mutex_unlock(&globalInfo->worker_mutex);
}

/***********************************************
*
* @Finalitat: Gestionar la connexió d’un Worker entrant:
*             - Llegir la trama inicial de connexió
*             - Emmagatzemar i respondre si és principal o secundari
*             - Mantenir heartbeats fins a la desconnexió
*             - Eliminar el Worker en acabar
* @Paràmetres: in: void_args = punter a ThreadArgsGotham amb socket i info global.
* @Retorn: NULL en finalitzar la connexió.
*
************************************************/
void *handle_worker_connection(void *void_args) {
    ThreadArgsGotham* args = (ThreadArgsGotham *)void_args;
    GlobalInfoGotham* globalInfo = args->global_info;
    int socket_connection = args->socket_connection;
    free(void_args);    // Liberamos porque solo lo utilizamos para pasar parámetros al thread


    unsigned char buffer[BUFFER_SIZE]; 
    // Esperar mensaje de Worker
    int bytes_read = recv(socket_connection, buffer, BUFFER_SIZE, 0);
    if (bytes_read <= 0) {
        perror("Error leyendo data de worker");
        close(socket_connection);
        return NULL;
    }

    TramaResult* result = leer_trama(buffer);
    if (result == NULL)
    {
        perror("Error con la trama enviada por Worker.");
        close(socket_connection);
        return NULL;
    }

    // Parsear la informacion del nuevo Worker dentro de nuestro array global de workers
    pthread_mutex_lock(&globalInfo->worker_mutex);
    int index_worker = globalInfo->num_workers;     // Indice del worker con el que estamos trabajando
    if (store_new_worker(globalInfo, result) == 0) {
        free_tramaResult(result);
        close(socket_connection);
    }
    globalInfo->workers[index_worker].socket_fd = socket_connection;   // Guardar socket del Worker
    
    /* Comprobar si se debe asignar como worker principal */
    unsigned char *trama;
    // Comprobar tipo de Worker
    if (strcmp(globalInfo->workers[index_worker].workerType, "Text") == 0) {
        if (globalInfo->enigma_pworker_index == -1) {
            globalInfo->enigma_pworker_index = index_worker;
            // Se le indica que es el worker principal en la trama
            trama = crear_trama(TYPE_PRINCIPAL_WORKER, (unsigned char*)"", strlen(""));
            // log_event(globalInfo, "Nuevo Worker asignado como Principal.");
        } else {
            // Se le indica que no es el worker principal en la trama
            trama = crear_trama(TYPE_CONNECT_WORKER_GOTHAM, (unsigned char*)"", strlen(""));
        }

    } else if (strcmp(globalInfo->workers[index_worker].workerType, "Media") == 0) {
        if (globalInfo->harley_pworker_index == -1) {
            globalInfo->harley_pworker_index = index_worker;
            // Se le indica que es el worker principal en la trama
            trama = crear_trama(TYPE_PRINCIPAL_WORKER, (unsigned char*)"", strlen(""));
            // log_event(globalInfo, "Nuevo Worker asignado como Principal.");
        } else {
            // Se le indica que no es el worker principal en la trama
            trama = crear_trama(TYPE_CONNECT_WORKER_GOTHAM, (unsigned char*)"", strlen(""));
        }
    } else {
        printF("Not known type\n");
    }
    pthread_mutex_unlock(&globalInfo->worker_mutex);

    if (trama == NULL) {
        printF("Error en malloc para trama\n");
        free_tramaResult(result);
        close(socket_connection);
        return NULL;
    }

    // Enviar a Worker confirmación de que hemos guardado su información 
    // Enviar la trama a Worker
    if (write(socket_connection, trama, BUFFER_SIZE) < 0) {
        printF("Error enviando la trama de conexión a Gotham\n");
        free(trama);
        free_tramaResult(result);
        close(socket_connection);
        return NULL;
    }
    free(trama);

    
    // Mantenerse respondiendo heartbeats constantemente
    enviar_heartbeat_constantemente(socket_connection);

    // Si acaba HEARTBEAT es porque se cerró la conexión y debemos limpiar el worker de la lista
    remove_worker(globalInfo, socket_connection);   // Se indica Socket en vez de index porque el index puede variar si se elimina otro Worker antes

    return NULL;
}

/***********************************************
*
* @Finalitat: Registrar un esdeveniment de sistema a Arkham:
*             - Formatejar missatge amb printf-like
*             - Empaquetar en una trama TYPE_LOG
*             - Escriure la trama completa al pipe d’Arkham
* @Paràmetres: in: g   = punter a l’estat global de Gotham.
*             in: fmt = cadena de format amb arguments variables.
* @Retorn: ----
*
************************************************/
void log_event(GlobalInfoGotham *g, const char *fmt, ...) {
    char msg[256];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(msg, sizeof(msg), fmt, ap);
    va_end(ap);

    unsigned char *frame = crear_trama(TYPE_LOG, (unsigned char*)msg, strlen(msg));
    if (!frame) {
        perror("Error creando trama de log");
        return;
    }

    // Verificamos que el file descriptor sea válido
    if (g->log_fd <= 0) {
        perror("File descriptor de log inválido");
        free(frame);
        return;
    }
    
    // Siempre escribimos BUFFER_SIZE bytes para que Arkham lea tramas completas
    ssize_t written = write(g->log_fd, frame, BUFFER_SIZE);
    if (written != BUFFER_SIZE) {
        perror("Error escribiendo en pipe de Arkham");
    }
    free(frame);
}