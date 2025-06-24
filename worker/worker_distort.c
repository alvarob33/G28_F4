#define _GNU_SOURCE

#include "worker_distort.h"
#include "enigma/enigmalib.h"
#include "harley/so_compression.h"

// Estructura para memoria compartida
typedef struct {
    int transfer_flag;  // 0=recibiendo, 1=enviando, 2=enviando
    long total_bytes_received;
} SharedData;


/***********************************************
*
* @Finalitat: Enviar al client Fleck la trama inicial de retorn de fitxer distorsionat amb
*             tamany i checksum, i validar la seva resposta.
* @Parametres:
*   in: socket_fd    = descriptor del socket amb Fleck.
*   in: fileSize     = cadena amb el nombre de bytes del fitxer.
*   in: fileMD5SUM   = cadena amb el MD5 sum del fitxer.
* @Retorn: 1 en èxit, -1 en cas d’error.
*
************************************************/
int start_send_back_distort(int socket_fd, char* fileSize, char* fileMD5SUM) {
    
    // Preparar y enviar la trama inicial de archivo distorsionado para Fleck
    unsigned char* data;
    asprintf((char**)&data, "%s&%s", fileSize, fileMD5SUM);
    // printF((char*)data);
    // printF("\n");
    
    unsigned char* tramaEnviar = crear_trama(TYPE_START_DISTORT_WORKER_FLECK, data, strlen((char*)data));
    if (write(socket_fd, tramaEnviar, BUFFER_SIZE) < 0) {
        perror("Error enviando respuesta al cliente");
        return -1;
    }
    free(data);
    free(tramaEnviar);


    // Leer la respuesta inicial de distorsión 
    unsigned char response[BUFFER_SIZE];
    int bytes_received = recv(socket_fd, response, BUFFER_SIZE, 0);
    
    TramaResult *result;
    if (bytes_received > 0) {
        // Procesar la trama
        result = leer_trama(response);
        if (result == NULL) {
            printF("Trama inválida recibida de Fleck.\n");
            if (result) free_tramaResult(result);
            return -1;
        }

        // Comprobar si responde con OK
        if (result->type == TYPE_START_DISTORT_WORKER_FLECK && strcmp(result->data, "CON_KO") != 0) {
            printF("Enviando archivo distorsionado de vuelta a Fleck.\n");

            if (result) free_tramaResult(result);
        } else {
            printF("Fleck ha enviado una trama inseperada en inicio envío archivo distorsionado.\n");
            if (result) free_tramaResult(result);
            return -1;
        }
    
        
    } else /*if (bytes_received == 0)*/ {
        printF("Error al recibir trama de Fleck.\n");
        return -1;
    }

    return 1;
} 

/***********************************************
*
* @Finalitat: Crear o obrir un segment de memòria  compartida per sincronitzar l’enviament/
*             recepció de fitxers.
* @Parametres:
*   in/out: shared    = punter a SharedData* result.
*   out:    fd_shared = descriptor de memòria compartida.
*   in:     filename  = identificador base per al segment.
*   in:     type      = 0: crear, 1: obrir.
* @Retorn: 0 en èxit, -1 en cas d’error.
*
************************************************/
int crear_abrir_mem_compartida(SharedData **shared, int* fd_shared, char* filename, int type) {
    char* shared_id = NULL;
    asprintf(&shared_id, "/%s", filename);  // ID debe empezar con '/' y no puede contener más '/'
    
    if (type == 0) {
        // Crear memoria compartida
        *fd_shared = shm_open(shared_id, O_CREAT | O_RDWR, 0666);
        if (*fd_shared == -1) {
            perror("shm_open (creación)");
            free(shared_id);
            return -1;
        }

        if (ftruncate(*fd_shared, sizeof(SharedData)) == -1) {
            perror("ftruncate");
            close(*fd_shared);
            shm_unlink(shared_id);
            free(shared_id);
            return -1;
        }

        *shared = mmap(NULL, sizeof(SharedData), PROT_READ | PROT_WRITE, MAP_SHARED, *fd_shared, 0);
        if (*shared == MAP_FAILED) {
            perror("mmap");
            close(*fd_shared);
            if (type == 0) shm_unlink(shared_id);
            free(shared_id);
            return -1;
        }

        (*shared)->total_bytes_received = 0;
        (*shared)->transfer_flag = 0;  // Inicialmente recibiendo
    } else {
        // Acceder a memoria compartida
        *fd_shared = shm_open(shared_id, O_RDWR, 0666);
        if (*fd_shared == -1) {
            perror("shm_open (acceso)");
            free(shared_id);
            return -1;
        }
        *shared = mmap(NULL, sizeof(SharedData), PROT_READ | PROT_WRITE, MAP_SHARED, *fd_shared, 0);
        if (*shared == MAP_FAILED) {
            perror("mmap");
            close(*fd_shared);
            if (type == 0) shm_unlink(shared_id);
            free(shared_id);
            return -1;
        }
    }

    free(shared_id);

    return 0;
}

/***********************************************
*
* @Finalitat: Enviar confirmació de recepció de fitxer distorsionat i esperar OK de Fleck.
* @Parametres:
*   in: socket_connection = socket de Fleck.
* @Retorn: 0 en èxit, -1 en cas d’error.
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

    // Esperar OK de Fleck
    unsigned char response[BUFFER_SIZE];
    int bytes_received = recv(socket_connection, response, BUFFER_SIZE, 0);
    
    if (bytes_received > 0) {
        // Procesar la trama
        TramaResult *result = leer_trama(response);
        if (result == NULL) {
            printF("Trama inválida recibida de Fleck.\n");
            if (result) free_tramaResult(result);
            return -1;
        }

        // Comprobar si responde con OK
        if (result->type == TYPE_END_DISTORT_FLECK_WORKER && strcmp(result->data, "CON_KO") != 0) {
            // printF("Fleck ha recibido la confirmación.\n");

            if (result) free_tramaResult(result);
        } else {
            return -1;
        }
    
        
    } else /*if (bytes_received == 0)*/ {
        // Conexión cerrada por Fleck
        return -1;
    }
    return 0;
}

/***********************************************
*
* @Finalitat: Esperar confirmació final de Fleck sobre la recepció del fitxer i respondre amb ACK.
* @Parametres:
*   in: socket_connection = socket de Fleck.
* @Retorn: 1 en èxit, -1 en cas d’error.
*
************************************************/
int wait_confirm_file_received(int socket_connection) {
    
    // Leer la respuesta final de distorsión 
    unsigned char response[BUFFER_SIZE];
    int bytes_received = recv(socket_connection, response, BUFFER_SIZE, 0);
    
    TramaResult *result;
    if (bytes_received > 0) {
        // Procesar la trama
        result = leer_trama(response);
        if (result == NULL) {
            printF("Trama inválida recibida de Fleck.\n");
            if (result) free_tramaResult(result);
            return -1;
        }

        // Comprobar si recxibimos con CHECK_OK
        if (result->type == TYPE_END_DISTORT_FLECK_WORKER && strcmp(result->data, CHECK_OK) == 0) {
            printF("Fleck ha recibido el archivo correctamente.\n");

            if (result) free_tramaResult(result);
        } else {
            printF("Fleck NO ha recibido el archivo correctamente (MD5 Invalido).\n");
            if (result) free_tramaResult(result);
            return -1;
        }

        // Enviar confirmación de recepción (ACK) a Fleck
        unsigned char *success_trama = crear_trama(TYPE_END_DISTORT_FLECK_WORKER, (unsigned char*)OK_MSG, strlen(OK_MSG));
        if (write(socket_connection, success_trama, BUFFER_SIZE) < 0) {
            perror("Error enviando confirmación de MD5");
            free(success_trama);

            return -1;
        }
        free(success_trama);
    
        
    } else {
        // Conexión cerrada por Worker
        return -1;
    }

    return 1;
}

/***********************************************
*
* @Finalitat: Controlar tot el flux de distorsió pel client Fleck: rebre, emmagatzemar,
*             distorsionar i reenviar.
* @Parametres:
*   in: arg = punter a ClientThread amb socket i estat.
* @Retorn: NULL al final o en error.
*
************************************************/
void* handle_fleck_connection(void* arg) {
    ClientThread* client = (ClientThread*)arg;

    int socket_connection = client->socket;
    unsigned char response[BUFFER_SIZE];
    int bytes_received = 0;
    TramaResult *result;
    int fd_file;
    char* distorted_file_path = NULL;
    
    // Punto Control
    if (!client->active) {
        close(socket_connection);
        return NULL;
    }

    // ---- 1. Recibir la solicitud inicial de distorsión ----

    bytes_received = recv(socket_connection, response, BUFFER_SIZE, 0);
    if (bytes_received <= 0) {
        perror("Error al recibir solicitud inicial");
        close(socket_connection);
        return NULL;
    }

    // Procesar la trama inicial
    result = leer_trama(response);
    if (!result || (result->type != TYPE_START_DISTORT_FLECK_WORKER && result->type != TYPE_RESUME_DISTORT_FLECK_WORKER)) {
        perror("Trama inicial inválida");
        if (result) free_tramaResult(result);
        close(socket_connection);
        return NULL;
    }

    // Parsear los datos de la trama inicial (username&filename&filesize&md5sum)
    char *username = strdup(strtok(result->data, "&"));
    char *filename = strdup(strtok(NULL, "&"));
    char *filesize_str = strdup(strtok(NULL, "&"));
    char *md5sum = strdup(strtok(NULL, "&"));
    char *distort_factor_str = strdup(strtok(NULL, "&"));
    
    if (!username || !filename || !filesize_str || !md5sum || !distort_factor_str) {
        perror("Formato de datos distorsion inicial inválido");

        free(username);
        free(filename);
        free(filesize_str);
        free(md5sum);
        free(distort_factor_str);
        free_tramaResult(result);
        close(socket_connection);
        return NULL;
    }
    
    long filesize = atol(filesize_str);
    int distort_factor = atoi(distort_factor_str);

    free(filesize_str);
    free(distort_factor_str);

    // Crear directorio para el usuario si no existe
    char* user_dir;
    asprintf(&user_dir, "uploads/%s", username);
    mkdir(user_dir, 0755);
    
    // Preparar path archivo de destino
    char *filepath = NULL;
    asprintf(&filepath, "%s/%s", user_dir, filename);
    
    free(username);
    free(user_dir);

    // Memoria compartida
    SharedData *shared = NULL;
    int fd_shared;
    crear_abrir_mem_compartida(&shared, &fd_shared, filename, (result->type == TYPE_START_DISTORT_FLECK_WORKER) ? 0 : 1);
    free(filename);

    // Enviar ACK de recepción inicial
    unsigned char *ack_trama = crear_trama(result->type, (unsigned char*)OK_MSG, strlen(OK_MSG));
    if (write(socket_connection, ack_trama, BUFFER_SIZE) < 0) {
        perror("Error enviando confirmación inicial");

        free(md5sum);
        free(filepath);
        close(socket_connection);
        return NULL;
    }
    free(ack_trama);
    
    // Punto Control
    if (!client->active) {
        free(md5sum);
        free(filepath);
        close(socket_connection);
        return NULL;
    }

    char* fileType = file_type(filepath);

    if (shared->transfer_flag == 0) {
        printF("Recibiendo archivo de Fleck.\n");
        
        // ---- Recibir archivo ----
        
        // 1. Abrir o crear el archivo donde se guardará la distorsión
        fd_file = open(filepath, O_WRONLY | O_CREAT | (result->type == TYPE_START_DISTORT_FLECK_WORKER ? O_TRUNC : O_APPEND), 0644);
        
        if (fd_file < 0) {
            perror("Error al abrir/crear archivo");
            free(md5sum);
            free(filepath);
            close(socket_connection);
            return NULL;
        }

        free_tramaResult(result);

        // Punto Control
        if (!client->active) {
            free(md5sum);
            free(filepath);
            close(socket_connection);
            return NULL;
        }

        // 2. Recibir el archivo en fragmentos y guardarlo
        while (shared->total_bytes_received < filesize) {

            bytes_received = recv(socket_connection, response, BUFFER_SIZE, 0);
            if (bytes_received != BUFFER_SIZE/*<= 0*/) {
                perror("Error al recibir fragmento de archivo, Fleck cerró la conexión.");
                free(md5sum);
                close(fd_file);
                free(filepath);
                close(socket_connection);
                return NULL;
            }

            // Procesar la trama de datos
            result = leer_trama(response);
            if (!result || result->type != TYPE_FILE_DATA) {
                perror("Trama de datos inválida");
                if (result) free_tramaResult(result);
                free(md5sum);
                close(fd_file);
                free(filepath);
                close(socket_connection);
                return NULL;
            }
            // printF(result->data);

            // WRITE data al archivo
            // printf("%d\n", result->data_length);
            size_t bytes_written = write(fd_file, result->data, result->data_length);
            if (bytes_written != (size_t)result->data_length) {
                perror("Error escribiendo en archivo");
                free_tramaResult(result);

                free(md5sum);
                close(fd_file);
                free(filepath);
                close(socket_connection);
                return NULL;
            }
            free_tramaResult(result);

            // Enviar confirmación de recepción (ACK)
            ack_trama = crear_trama(TYPE_FILE_DATA, (unsigned char*)OK_MSG, strlen(OK_MSG));
            if (write(socket_connection, ack_trama, BUFFER_SIZE) < 0) {
                perror("Error enviando confirmación de recepción");
                free(ack_trama);

                free(md5sum);
                close(fd_file);
                free(filepath);
                close(socket_connection);
                return NULL;
            }
            
            shared->total_bytes_received += bytes_written;
            free(ack_trama);            

            // Punto Control
            if (!client->active) {
                free(md5sum);
                close(fd_file);
                free(filepath);
                close(socket_connection);
                return NULL;
            }
        }

        // ---- Comprobar MD5 del archivo recibido ----

        char *calculated_md5 = calculate_md5sum(filepath);
        if (calculated_md5 == NULL) {
            perror("Error calculando MD5 del archivo recibido");
        }

        // Enviar trama al cliente en base al resultado del MD5
        if (!calculated_md5 || strcmp(calculated_md5, md5sum) != 0) {
            unsigned char *error_trama = crear_trama(TYPE_END_DISTORT_FLECK_WORKER, (unsigned char*)CHECK_KO, strlen(CHECK_KO));
            if (write(socket_connection, error_trama, BUFFER_SIZE) < 0) {
                perror("Error enviando mensaje de MD5 no coincidente");
            } else {
                printF("Enviado: MD5 del archivo recibido no coincide con el esperado\n");
            }
            free(error_trama);

            if (calculated_md5) free(calculated_md5);
            free(md5sum);
            close(fd_file);
            free(filepath);
            close(socket_connection);
            return NULL;
        }

        if (send_confirm_file_received(socket_connection) != 0) {
            perror("Error enviando confirmación de recepción del archivo con MD5SUM correcto");
            free(calculated_md5);
            free(md5sum);
            close(fd_file);
            free(filepath);
            close(socket_connection);
            return NULL;
        }
        
        free(md5sum);
        free(calculated_md5);

        close(fd_file);
        shared->transfer_flag = 1;

        printF("Archivo de Fleck recibido correctamente.\n");

        // Punto Control
        if (!client->active) {
            free(filepath);
            close(socket_connection);
            return NULL;
        }

    } else {
        free(md5sum);
        free_tramaResult(result);
    }


    if (shared->transfer_flag == 1) {

        // 3. Distorsionar archivo

        if (strcmp(fileType, MEDIA) == 0) {
            // MEDIA: AUDIO o IMAGE
            distorted_file_path = filepath;
            if (strcmp(wich_media(filepath), AUDIO) == 0) {
                printF("Distorsionando archivo de tipo AUDIO.\n");
                if (SO_compressAudio(filepath, distort_factor) != 0) {
                    printF("Error distorsionando archivo de audio.\n");
                    free(filepath);
                    close(socket_connection);
                    return NULL;
                }
            } else if (strcmp(wich_media(filepath), IMAGE) == 0) {
                printF("Distorsionando archivo de tipo IMAGE.\n");
                if (SO_compressImage(filepath, distort_factor) != 0) {
                    printF("Error distorsionando archivo de imagen.\n");
                    free(filepath);
                    close(socket_connection);
                    return NULL;
                }
            } else {
                printF("Tipo de archivo multimedia no soportado.\n");
                free(filepath);
                close(socket_connection);
                return NULL;
            }
        } else {
            // TEXT
            // Crear nombre del archivo de salida
            if (asprintf(&distorted_file_path, "%s_distorted", filepath) < 0) {
                printF("Filename generation failed\n");
                return NULL;
            }

            printF("Distorsionando archivo de tipo TEXT.\n");
            if (distort_file_text(filepath, distorted_file_path, distort_factor) != 0) {
                free(filepath);
                close(socket_connection);
                return NULL;
            }
            free(filepath);
        } 

        shared->transfer_flag = 2;  // Cambiar flag a enviando
        shared->total_bytes_received = 0;  // Reiniciar contador de bytes recibidos


        // Punto Control
        if (!client->active) {
            free(distorted_file_path);
            close(socket_connection);
            return NULL;
        }


    } else {

        if (strcmp(fileType, MEDIA) == 0) {
            distorted_file_path = filepath;
        } else {
            // Crear nombre del archivo de salida
            if (asprintf(&distorted_file_path, "%s_distorted", filepath) < 0) {
                printF("Filename generation failed\n");
                return NULL;
            }
            free(filepath);
        }
    }


    // ---- 4. Enviar archivo distorsionado de vuelta a Fleck ----

    filesize_str = get_string_file_size(distorted_file_path);
    
    md5sum = calculate_md5sum(distorted_file_path);
    if (md5sum == NULL) {
        perror("Error: Error en Fleck calculando el MD5SUM del archivo.\n");
        free(distorted_file_path);
        free(filesize_str);
        close(socket_connection);
        return NULL;
    }
    
    // Enviar trama inicial

    if (start_send_back_distort(socket_connection, filesize_str, md5sum) < 1) {
        perror("Error al enviar la solicitud de distorsión al Worker");
        free(distorted_file_path);
        free(filesize_str);
        free(md5sum);
        close(socket_connection);
        return NULL;
    }

    // Comenzar a enviar el archivo distorsionado en fragmentos
    // Abrir archivo distorsionado para lectura
    fd_file = open(distorted_file_path, O_RDONLY);
    if (fd_file < 0) {
        perror("Error al abrir archivo distorsionado");
        free(distorted_file_path);
        free(filesize_str);
        free(md5sum);
        close(socket_connection);
        return NULL;
    }

    // Posicionar el puntero de lectura en el byte donde se quedó
    if (lseek(fd_file, shared->total_bytes_received, SEEK_SET) == -1) {
        perror("Error posicionando puntero de archivo");
        close(fd_file);
        free(distorted_file_path);
        free(filesize_str);
        free(md5sum);
        close(socket_connection);
        return NULL;
    }

    // Enviar archivo en fragmentos
    unsigned char buffer[247];
    // long distorted_filesize = atol(filesize_str);

    int bytes_read;
    while ( (bytes_read = read(fd_file, buffer, sizeof(buffer))) > 0 ) {

        unsigned char* trama = crear_trama(TYPE_FILE_DATA, buffer, bytes_read);
        if (!trama) {
            perror("Error creando trama de datos");
            close(fd_file);
            free(distorted_file_path);
            free(filesize_str);
            free(md5sum);
            close(socket_connection);
            return NULL;
        }

        if (write(socket_connection, trama, BUFFER_SIZE) < 0) {
            perror("Error enviando fragmento de archivo");
            free(trama);
            close(fd_file);
            free(distorted_file_path);
            free(filesize_str);
            free(md5sum);
            close(socket_connection);
            return NULL;
        }
        free(trama);
        shared->total_bytes_received += bytes_read;

        // Esperar confirmación de recepción
        bytes_received = recv(socket_connection, response, BUFFER_SIZE, 0);
        if (bytes_received <= 0) {
            perror("Error recibiendo confirmación de recepción");
            close(fd_file);
            free(distorted_file_path);
            free(filesize_str);
            free(md5sum);
            close(socket_connection);
            return NULL;
        }

        result = leer_trama(response);
        if (!result || result->type != TYPE_FILE_DATA || strcmp(result->data, OK_MSG) != 0) {
            perror("Confirmación de recepción inválida");
            if (result) free_tramaResult(result);
            close(fd_file);
            free(distorted_file_path);
            free(filesize_str);
            free(md5sum);
            close(socket_connection);
            return NULL;
        }
        free_tramaResult(result);


        // DEBUGGING: Bajar velocidad de envío
        // sleep(3);

        // Punto Control
        if (!client->active) {
            free(md5sum);
            free(filepath);
            close(socket_connection);
            return NULL;
        }
        
        
    }

    close(fd_file);
    free(distorted_file_path);
    free(filesize_str);
    free(md5sum);

    // Recibir trama final de confirmación
    if (wait_confirm_file_received(socket_connection) < 1) {
        perror("Error al esperar confirmación de archivo recibido por Worker");
        close(socket_connection);
        return NULL;
    }

    printF("Distosión FINALIZADA correctamente.\n");

    close(socket_connection);
    return NULL;

}