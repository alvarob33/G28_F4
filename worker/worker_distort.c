#define _GNU_SOURCE

#include "worker_distort.h"
#include "enigma/enigmalib.h"
#include "harley/harleylib.h"

// Estructura para memoria compartida
typedef struct {
    int transfer_flag;  // 0=recibiendo, 1=enviando
    long total_bytes_received;
} SharedData;

// Función para manejar la distorsion del cliente
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
    char* shared_id;
    asprintf(&shared_id, "/%s", filename);  // ID debe empezar con '/' y no puede contener más '/'
    free(filename);
    if (result->type == TYPE_START_DISTORT_FLECK_WORKER) {
        // Crear memoria compartida
        fd_shared = shm_open(shared_id, O_CREAT | O_RDWR, 0666);
        ftruncate(fd_shared, sizeof(SharedData));
        shared = mmap(NULL, sizeof(SharedData), PROT_READ | PROT_WRITE, MAP_SHARED, fd_shared, 0);
        shared->total_bytes_received = 0;
        shared->transfer_flag = 0;  // Inicialmente recibiendo
    } else {
        // Acceder a memoria compartida
        fd_shared = shm_open(shared_id, O_RDWR, 0666);
        shared = mmap(NULL, sizeof(SharedData), PROT_READ | PROT_WRITE, MAP_SHARED, fd_shared, 0);
    }

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
        printF("Recibiendo archivo DISTORT de Fleck.\n");
        
        // ---- Recibir archivo ----
        
        // 1. Abrir o crear el archivo donde se guardará la distorsión
        if (result->type == TYPE_START_DISTORT_FLECK_WORKER) {
            if (strcmp(fileType, MEDIA) == 0) {
                // Modo binario para write (O_TRUNC + O_BINARY)
                fd_file = open(filepath, O_WRONLY | O_CREAT | O_TRUNC | O_BINARY, 0644);
            } else {
                // Modo texto (sobrescribir)
                fd_file = open(filepath, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            }
        } else {
            if (strcmp(fileType, MEDIA) == 0) {
                // Modo binario para append (O_APPEND + O_BINARY)
                fd_file = open(filepath, O_WRONLY | O_CREAT | O_APPEND | O_BINARY, 0644);
            } else {
                // Modo texto (append)
                fd_file = open(filepath, O_WRONLY | O_CREAT | O_APPEND, 0644);
            }
        }
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
            if (bytes_received <= 0) {
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
            printF(result->data);

            // WRITE data al archivo
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
            free(md5sum);
            close(fd_file);
            free(filepath);
            close(socket_connection);
            return NULL;
        }

        // Enviar trama al cliente en base al resultado del MD5
        if (strcmp(calculated_md5, md5sum) != 0) {
            unsigned char *error_trama = crear_trama(TYPE_END_DISTORT_FLECK_WORKER, (unsigned char*)CHECK_KO, strlen(CHECK_KO));
            if (write(socket_connection, error_trama, BUFFER_SIZE) < 0) {
                perror("Error enviando mensaje de MD5 no coincidente");
            } else {
                printF("Enviado: MD5 del archivo recibido no coincide con el esperado\n");
            }
            free(error_trama);

            free(calculated_md5);
            free(md5sum);
            close(fd_file);
            free(filepath);
            close(socket_connection);
            return NULL;
        }

        // Enviar confirmación de que el archivo se recibió correctamente cxon MD5SUM correcto
        unsigned char *success_trama = crear_trama(TYPE_END_DISTORT_FLECK_WORKER, (unsigned char*)CHECK_OK, strlen(CHECK_OK));
        if (write(socket_connection, success_trama, BUFFER_SIZE) < 0) {
            perror("Error enviando confirmación de MD5");
            free(success_trama);

            free(calculated_md5);
            free(md5sum);
            close(fd_file);
            free(filepath);
            close(socket_connection);
            return NULL;
        }
        free(success_trama);
        free(md5sum);
        free(calculated_md5);

        close(fd_file);

        printF("Archivo de Fleck recibido correctamente.\n");

        // Punto Control
        if (!client->active) {
            free(filepath);
            close(socket_connection);
            return NULL;
        }

        // 3. Distorsionar archivo
        if (strcmp(fileType, MEDIA) == 0) {
            printF("Distorsionando archivo de tipo MEDIA.\n");
        } else {
            printF("Distorsionando archivo de tipo TEXT.\n");
            if (distort_file_text(filepath, distorted_file_path, distort_factor) != 0) {
                // Manejar error
                free(filepath);
                close(socket_connection);
                return NULL;
            }
        } 
        free(filepath);

        // Punto Control
        if (!client->active) {
            free(distorted_file_path);
            close(socket_connection);
            return NULL;
        }


    } else {
        free_tramaResult(result);
    }


    printF("Enviando archivo distorsionado de vuelta a Fleck.\n");
    // ---- 4. Enviar archivo distorsionado de vuelta a Fleck ----





    // Limpieza final
    free(distorted_file_path);
    close(socket_connection);
    return NULL;
}

