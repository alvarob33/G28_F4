#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>

#include "../worker/worker.h"
#include "gothamlib.h"

// extern Worker* workers;
// extern int num_workers;
// extern int enigma_pworker_index;
// extern int harley_pworker_index;
// extern pthread_mutex_t worker_mutex;


// Función para leer el archivo de configuración
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

// Se libera la memoria dinámica de un Worker y se cierra las conexión de este
void liberar_memoria_worker(Worker worker) {

    free(worker.workerType);
    free(worker.IP);     
    free(worker.Port);  
    
    if (worker.socket_fd > 0) {
        close(worker.socket_fd);    // Cerrar socket Gotham-Worker
    }

}

// Se libera la memoria de los structs
void liberar_memoria_workers(GlobalInfo globalInfo) {
    if (globalInfo->workers == NULL) {
        printF("Workers es null\n");
        return;
    }

    for (int i = 0; i < globalInfo->num_workers; i++) {
        liberar_memoria_worker(globalInfo->workers[i]);
    }

    free(globalInfo->workers);  // Liberar el array dinámico de workers
    printF("Memoria de los workers liberada correctamente.\n");
}


void* handle_fleck_connection(void* fleck_socket) {
    int socket_fd = *(int*)fleck_socket;
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
                printf("Usuario conectado: %s, IP: %s, Puerto: %s\n", username, ip, port);

                // Responder con OK
                unsigned char *response = crear_trama(TYPE_CONNECT_FLECK_GOTHAM, "");  // DATA vacío
                if (write(socket_fd, response, BUFFER_SIZE) < 0) {
                    perror("Error enviando respuesta OK a Fleck");
                }
                free(response);
                // printF("Respuesta de OK enviada a Fleck.\n");


            } else {
                // Responder con CON_KO si el formato es incorrecto
                unsigned char *response = crear_trama(TYPE_CONNECT_FLECK_GOTHAM, "CON_KO");
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
            
            // Parsear los datos: <mediaType>&<fileName> (duplicándolos para poder liberar memoria de result)
            char *mediaType = strdup(strtok(result->data, "&"));
            char *fileName = strdup(strtok(NULL, "&"));

            free_tramaResult(result); // Liberar la trama procesada

            // Comprobar si hay Workers para el archivo solicitado
            if ((strcmp(mediaType, MEDIA) == 0 && harley_pworker_index < 0) || (strcmp(mediaType, TEXT) == 0 && enigma_pworker_index < 0))
            {
                // Responder con DISTORT_KO
                unsigned char *response = crear_trama(TYPE_DISTORT_FLECK_GOTHAM, "DISTORT_KO"); 
                if (write(socket_fd, response, BUFFER_SIZE) < 0) {
                    perror("Error enviando respuesta DISTORT_KO a Fleck");
                }
                free(response);
                printF("Sin Workers disponibles. Respuesta de DISTORT_KO enviada a Fleck.\n");

                // char* buffer;
                // asprintf(&buffer, " %s %d %d ", mediaType, harley_pworker_index, num_workers);
                // printF(buffer);
                // free(buffer);

                continue;
            }

            // Enviar datos Worker
            if (strcmp(mediaType, MEDIA) == 0)
            {
                // Responder con datos Worker Harley principal
                char* data;
                asprintf(&data, "%s&%s", workers[harley_pworker_index].IP, workers[harley_pworker_index].Port);
                unsigned char *response = crear_trama(TYPE_DISTORT_FLECK_GOTHAM, data); 
                free(data);

                if (write(socket_fd, response, BUFFER_SIZE) < 0) {
                    perror("Error enviando respuesta DISTORT a Fleck");
                }
                free(response);
                printF("Worker Harley pricipal enviado a Fleck.\n");
            } else if (strcmp(mediaType, TEXT) == 0)
            {
                // Responder con datos Worker Enigma principal
                char* data;
                asprintf(&data, "%s&%s", workers[enigma_pworker_index].IP, workers[enigma_pworker_index].Port);
                unsigned char *response = crear_trama(TYPE_DISTORT_FLECK_GOTHAM, data); 
                free(data);

                if (write(socket_fd, response, BUFFER_SIZE) < 0) {
                    perror("Error enviando respuesta DISTORT a Fleck");
                }
                free(response);
                printF("Worker Enigma pricipal enviado a Fleck.\n");
            } else {
                // Responder con MEDIA_KO
                unsigned char *response = crear_trama(TYPE_DISTORT_FLECK_GOTHAM, "MEDIA_KO"); 
                if (write(socket_fd, response, BUFFER_SIZE) < 0) {
                    perror("Error enviando respuesta MEDIA_KO a Fleck");
                }
                free(response);

                char* buffer;
                asprintf(&buffer, "Media type '%s' no reconocido. Respuesta de MEDIA_KO enviada a Fleck.\n", mediaType);
                printF(buffer);
                free(buffer);
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




/* HACE BIEN EL CHECKSUM PERO SE CIERRA EL SERVIDOR INMEDIATAMENTE
void* handle_fleck_connection(void* client_socket) {
    int socket_fd = *(int*)client_socket;
    unsigned char buffer[BUFFER_SIZE];
    int bytes_read;

    while ((bytes_read = recv(socket_fd, buffer, sizeof(buffer), 0)) > 0) {
        TramaResult *result = leer_trama(buffer);
        if (result == NULL || result->data == NULL) {
            printF("Trama inválida recibida de Fleck.\n");
            if (result) free_tramaResult(result);
            continue;
        }

        printf("Trama recibida: TYPE=0x%02x, DATA=%s\n", buffer[0], &buffer[3]);


        // Validar el TYPE de la trama
        if (buffer[0] != 0x01) {  // Si el TYPE no es 0x01
            printF("Tipo de trama inválido recibido de Fleck.\n");
            free_tramaResult(result);
            continue;
        }

        // Comando CONNECT
        printF("Comando CONNECT recibido de Fleck.\n");

        // Hacer una copia de los datos para trabajar con strtok
        char *data_copy = strdup(result->data);
        if (data_copy == NULL) {
            perror("Error al duplicar los datos de la trama");
            free_tramaResult(result);
            continue;
        }

        // Parsear los datos: <username>&<IP>&<Port>
        char *username = strtok(data_copy, "&");
        char *ip = strtok(NULL, "&");
        char *port = strtok(NULL, "&");

        if (username && ip && port) {
            // Procesar datos válidos
            printf("Usuario conectado: %s, IP: %s, Puerto: %s\n", username, ip, port);

            // Responder con OK
            unsigned char *response = crear_trama(0x01, "");  // DATA vacío
            if (send(socket_fd, response, BUFFER_SIZE, 0) < 0) {
                perror("Error enviando respuesta OK a Fleck");
            } else {
                printF("Respuesta de OK enviada a Fleck.\n");
            }
            free(response);

            // Cerrar el socket después de procesar correctamente
            close(socket_fd);
            printF("Socket cerrado tras procesar trama CONNECT.\n");

            // Liberar recursos
            free(data_copy);
            free_tramaResult(result);

        } else {
            // Manejar datos inválidos
            unsigned char *response = crear_trama(0x01, "CON_KO");
            if (send(socket_fd, response, BUFFER_SIZE, 0) < 0) {
                perror("Error enviando respuesta CON_KO a Fleck");
            } else {
                printF("Formato de conexión inválido. Respuesta CON_KO enviada.\n");
            }
            free(response);

            // Liberar recursos
            free(data_copy);
            free_tramaResult(result);

            // Cerrar el socket después de un error en los datos
            close(socket_fd);
            printF("Socket cerrado tras recibir datos inválidos.\n");

            return NULL;  // Terminar la conexión
        }

        // Si el bucle termina normalmente por `bytes_read == 0`
        if (bytes_read == 0) {
            printF("Fleck desconectado.\n");
        } else if (bytes_read < 0) {
            // Manejar error en recv
            perror("Error al recibir datos de Fleck");
        }

        // Cerrar socket si no se cerró previamente
        close(socket_fd);
        printF("Socket cerrado tras terminar el procesamiento.\n");

        return NULL;
    }
}
*/

int find_worker_bySocket(int socket_fd) {
    for (int i = 0; i < num_workers; i++) {
        if (workers[i].socket_fd == socket_fd) {
            return i; // Índice encontrado
        }
    }
    return -1; // No se encontró el Worker
}

int store_new_worker(TramaResult* result) {
    
    // Comprobar que no se supere número máximo de Workers
    if (num_workers >= MAX_WORKERS) {
        pthread_mutex_unlock(&worker_mutex);
        printF("Error: No se pudo agregar el worker. Límite de workers alcanzado.\n");
        return 0;
    }
    
    // Crear nuevo worker dinámico
    if (workers == NULL)
    {
        workers = (Worker *)malloc(sizeof(Worker));
        if (workers == NULL) {
            pthread_mutex_unlock(&worker_mutex);
            perror("Failed to allocate memory for new worker");
            return 0;
        }

    } else {
        workers = realloc(workers, (num_workers + 1) * sizeof(Worker));
        if (workers == NULL) {
            pthread_mutex_unlock(&worker_mutex);
            perror("Failed to reallocate memory for workers array");
            return 0;
        }
    }

    // Procesar data con el formato <workerType>&<IP>&<Port>
    workers[num_workers].workerType = strdup(strtok(result->data, "&"));
    workers[num_workers].IP = strdup(strtok(NULL, "&"));
    workers[num_workers].Port = strdup(strtok(NULL, "&"));
    free_tramaResult(result);

    // check and print worker info
    if (workers[num_workers].workerType == NULL || workers[num_workers].IP == NULL || workers[num_workers].Port == NULL) {
        printF("Error: Formato de datos inválido.\n");
        return 0;
    }

    char* aux;
    asprintf(&aux, "New worker added: workerType=%s, IP=%s, Port=%s\n",
           workers[num_workers].workerType, workers[num_workers].IP, workers[num_workers].Port);
    printF(aux);

    num_workers++;
    
    return 1;
}

void remove_worker(int socket_fd) {
    pthread_mutex_lock(&worker_mutex);

    int index = find_worker_bySocket(socket_fd);
    if (index < 0) {
        perror("Error al buscar Worker mediante su socket.");
        pthread_mutex_unlock(&worker_mutex);
        return;
    }

    liberar_memoria_worker(workers[index]);
    // Mover los elementos restantes hacia adelante para rellenar hueco
    for (int i = index; i < num_workers - 1; i++) {
        workers[i] = workers[i + 1];
    }

    num_workers--;
    workers = realloc(workers, num_workers * sizeof(Worker));
    if (workers == NULL && num_workers > 0) {
        perror("Error al realocar memoria para workers.");
        pthread_mutex_unlock(&worker_mutex);
        return;
    }

    // Comprobar si era un Worker principal, y en dicho caso asignar a uno nuevo
    if (index == enigma_pworker_index) {
        enigma_pworker_index = -1;  // Borrar el índice de Enigma Principal Worker

        // Buscar un nuevo worker de tipo "Text"
        for (int i = 0; i < num_workers; i++) {
            if (strcmp(workers[i].workerType, "Text") == 0) {

                // WORKER ENCONTRADO
                enigma_pworker_index = i;

                // Crear trama informando que es el nuevo Principal Worker
                unsigned char* trama;
                trama = crear_trama(TYPE_PRINCIPAL_WORKER, "");
                if (trama == NULL) {
                    printF("Error en malloc para trama\n");
                    pthread_mutex_unlock(&worker_mutex);
                    return;
                }

                // Enviar la trama a Worker
                if (write(workers[i].socket_fd, trama, BUFFER_SIZE) < 0) {
                    printF("Error enviando la trama de conexión a Gotham\n");
                    free(trama);
                    pthread_mutex_unlock(&worker_mutex);
                    return;
                }
                free(trama);

                // Mostrar mensaje indicando que encontramos un nuevo Principal Worker
                char* buffer;
                asprintf(&buffer, "Nuevo pworker de tipo 'Text' encontrado en el índice %d.\n", enigma_pworker_index);
                printF(buffer);
                free(buffer);
                break;
            }
        }

        if (enigma_pworker_index == -1) {
            printF("No hay Workers de tipo 'Text' para asignar como Principal Worker.\n");
        }
    } else if (index == harley_pworker_index) {
        harley_pworker_index = -1;  // Borrar el índice de Harley Principal Worker

        // Buscar un nuevo worker de tipo "Media"
        for (int i = 0; i < num_workers; i++) {
            if (strcmp(workers[i].workerType, "Media") == 0) {
                
                // WORKER ENCONTRADO
                harley_pworker_index = i;

                // Crear trama informando que es el nuevo Principal Worker
                unsigned char* trama;
                trama = crear_trama(TYPE_PRINCIPAL_WORKER, "");
                if (trama == NULL) {
                    printF("Error en malloc para trama\n");
                    pthread_mutex_unlock(&worker_mutex);
                    return;
                }

                // Enviar la trama a Worker
                if (write(workers[i].socket_fd, trama, BUFFER_SIZE) < 0) {
                    printF("Error enviando la trama de conexión a Gotham\n");
                    free(trama);
                    pthread_mutex_unlock(&worker_mutex);
                    return;
                }
                free(trama);

                // Mostrar mensaje indicando que encontramos un nuevo Principal Worker
                char* buffer;
                asprintf(&buffer, "Nuevo pworker de tipo 'Media' encontrado en el índice %d.\n", harley_pworker_index);
                printF(buffer);
                free(buffer);
                break;
            }
        }
        if (harley_pworker_index == -1) {
            printF("No hay Workers de tipo 'Media' para asignar como Principal Worker.\n");
        }
    }

    // printf("Worker en índice %d eliminado correctamente.\n", index);
    pthread_mutex_unlock(&worker_mutex);
}


void *handle_worker_connection(void *arg) {
    int socket_connection = *(int *)arg;
    unsigned char buffer[256]; // Buffer to receive data
    int bytes_read;

    // Esperar mensaje de Worker
    bytes_read = read(socket_connection, buffer, BUFFER_SIZE);
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
    pthread_mutex_lock(&worker_mutex);
    int index_worker = num_workers;     // Indice del worker con el que estamos trabajando
    if (store_new_worker(result) == 0) {
        free_tramaResult(result);
        close(socket_connection);
    }
    workers[index_worker].socket_fd = socket_connection;   // Guardar socket del Worker
    
    /* Comprobar si se debe asignar como worker principal */
    unsigned char *trama;
    // Comprobar tipo de Worker
    if (strcmp(workers[index_worker].workerType, "Text") == 0) {
        if (enigma_pworker_index == -1) {
            enigma_pworker_index = index_worker;
            // Se le indica que es el worker principal en la trama
            trama = crear_trama(TYPE_PRINCIPAL_WORKER, "");
        } else {
            // Se le indica que no es el worker principal en la trama
            trama = crear_trama(TYPE_CONNECT_WORKER_GOTHAM, "");
        }

    } else if (strcmp(workers[index_worker].workerType, "Media") == 0) {
        if (harley_pworker_index == -1) {
            harley_pworker_index = index_worker;
            // Se le indica que es el worker principal en la trama
            trama = crear_trama(TYPE_PRINCIPAL_WORKER, "");
        } else {
            // Se le indica que no es el worker principal en la trama
            trama = crear_trama(TYPE_CONNECT_WORKER_GOTHAM, "");
        }
    } else {
        printF("Not known type\n");
    }
    pthread_mutex_unlock(&worker_mutex);

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
    remove_worker(socket_connection);   // Se indica Socket en vez de index porque el index puede variar si se elimina otro Worker antes

    return NULL;
}



