

#include "flecklib_distort.h"

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
    if ((*distortInfo->worker_ptr) != NULL) {
        if ((*distortInfo->worker_ptr)->IP != NULL) {
            free((*distortInfo->worker_ptr)->IP);
        }
        if ((*distortInfo->worker_ptr)->Port != NULL) {
            free((*distortInfo->worker_ptr)->Port);
        }
        // No se hace FREE porque se asignó con memoria estática
        // if ((*distortInfo->worker_ptr)->workerType != NULL) {
        //     free((*distortInfo->worker_ptr)->workerType);
        // }

        // Cerrar conexión socket con Worker
        if ((*distortInfo->worker_ptr)->socket_fd >= 0) {
            close((*distortInfo->worker_ptr)->socket_fd);
            (*distortInfo->worker_ptr)->socket_fd = -1; // Marcar como cerrado
        }

        // Liberar la memoria de la estructura WorkerFleck y asignarla como NULL
        free(*distortInfo->worker_ptr);
        *distortInfo->worker_ptr = NULL;
    }

    // Finalmente, liberar la estructura DistortInfo en sí misma
    free(distortInfo);
}

char* get_string_file_size(const char* filename) {
    int fd = open(filename, O_RDONLY);
    if (fd == -1) {
        perror("No se pudo abrir el archivo");
        return NULL;
    }

    // Obtener el tamaño del archivo usando lseek
    long size = lseek(fd, 0, SEEK_END);  // Mover al final del archivo y obtener la posición
    if (size < 0) {
        perror("Error al obtener el tamaño del archivo");
        close(fd);  // Cerrar el archivo si ocurre un error
        return NULL;
    }

    // Cerrar  archivo
    close(fd);

    char* size_str;
    if (asprintf(&size_str, "%ld", size) < 0) {
        perror("Error al asignar memoria para el tamaño del archivo");
        return NULL;
    }

    return size_str;
}



char* calculate_md5sum(const char* filename) {
    if (filename == NULL) {
        fprintf(stderr, "El nombre del archivo no puede ser NULL.\n");
        return NULL;
    }

    // Creamos pipes (para comunicación)
    int pipe_fd[2];
    if (pipe(pipe_fd) < 0) {
        perror("Error al crear el pipe");
        return NULL;
    }

    // Creamos fork para que proceso hijo ejecute el comando y nosotros leamos el resultado
    pid_t pid = fork();
    if (pid < 0) {
        perror("Error al hacer fork");
        close(pipe_fd[0]);
        close(pipe_fd[1]);
        return NULL;
    }

    if (pid == 0) { // Proceso hijo
        // Redirigir la salida STDOUT(por pantalla) al extremo de escritura del pipe
        dup2(pipe_fd[1], STDOUT_FILENO);
        close(pipe_fd[0]); // Cerrar el extremo de lectura
        close(pipe_fd[1]);

        // Ejecutar el comando `md5sum` para calcularlo sobre el archivo indicado
        execlp("md5sum", "md5sum", filename, (char*)NULL);

        // Si execlp falla (el proceso se debería sustituir por md5sum, por lo que no debería pasar por aquí)
        perror("Error al ejecutar md5sum");
        exit(EXIT_FAILURE);
    }

    // Proceso padre
    close(pipe_fd[1]); // Cerrar el extremo de escritura

    // Esperar a que el proceso hijo termine antes de leer del pipe
    int status;
    waitpid(pid, &status, 0); // Esperamos que el proceso hijo termine
    if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
        fprintf(stderr, "El comando md5sum falló.\n");
        close(pipe_fd[0]);
        return NULL;
    }

    // Leer la salida del pipe
    char md5_output[64] = {0};
    ssize_t bytes_read = read(pipe_fd[0], md5_output, sizeof(md5_output) - 1);
    close(pipe_fd[0]); // Cerrar el extremo de lectura

    if (bytes_read <= 0) {
        perror("Error al leer la salida de md5sum");
        return NULL;
    }

    // Extraer solo el MD5 (los primeros 32 caracteres porque los siguientes dan otra información)
    char* md5sum = (char*)malloc(33);
    if (md5sum == NULL) {
        perror("Error al asignar memoria para el MD5 sum");
        return NULL;
    }
    strncpy(md5sum, md5_output, 32);
    md5sum[32] = '\0'; // Asegurarse de que la cadena termine en '\0'

    return md5sum;
}



// Función para manejar la solicitud de distorsión
void* handle_distort_worker(void* arg) {
    DistortInfo* distortInfo = (DistortInfo*)arg; // Puntero a Worker* para igualarlo a NULL al final
    WorkerFleck* worker = *distortInfo->worker_ptr;

    // DEBUGGING:
    printf("Conectando a Worker en %s:%s...\n", worker->IP, worker->Port);

    if (distortInfo->worker_ptr == NULL ) {
        perror("WorkerFleck** es NULL");
        return NULL;
    }
    if (*distortInfo->worker_ptr == NULL) {
        perror("WorkerFleck* es NULL");
        return NULL;
    }

    // Crear socket de conexión con Worker
    worker->socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (worker->socket_fd < 0) {
        perror("Error al crear el socket");
        return NULL;
    }

    // Configurar la dirección del servidor (Worker)
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr)); // Poner a 0 toda la estructura
    server_addr.sin_family = AF_INET;            // Familia de direcciones: IPv4
    server_addr.sin_port = atoi(worker->Port); // Puerto del Worker (convertido a formato de red)

    // Convertir la IP de string a formato binario y configurarla
    if (inet_pton(AF_INET, worker->IP, &server_addr.sin_addr) <= 0) {
        perror("Error al convertir la IP");
        close(worker->socket_fd);
        return NULL;
    }

    // Conectar al servidor Worker
    if (connect(worker->socket_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Error al conectar con Worker");
        close(worker->socket_fd);
        return NULL;
    }

    // Enviar la solicitud de distorsión a Worker

    // Obtener filesize
    char* fileSize = get_string_file_size(distortInfo->filename);

    // Calcular MD5SUM
    char* fileMD5SUM = calculate_md5sum(distortInfo->filename);
    if (fileMD5SUM == NULL) {
        printf("Error: Error en Fleck calculando el MD5SUM del archivo.\n");
        return NULL;
    }
    fileMD5SUM[strlen(fileMD5SUM)] = '\0';

    char* data;
    
    asprintf(&data, "%s&%s&%s&%s", distortInfo->username, distortInfo->filename, fileSize, fileMD5SUM);
    printF(data);
    printF("\n");
    
    // unsigned char* tramaEnviar = crear_trama(TYPE_HEARTBEAT, (unsigned char*)"OK", strlen("OK"));
    // if (write(socket_fd, tramaEnviar, BUFFER_SIZE) < 0) {
    //     perror("Error enviando respuesta al cliente");
    //     close(socket_fd);
    //     continue;
    // }
    // free(tramaEnviar);
    

    // Leer la respuesta del servidor 
    // unsigned char buffer[BUFFER_SIZE];
    // int bytes_received = recv(worker->socket_fd, response, sizeof(response) - 1, 0);
    // if (bytes_received > 0) {
    //     response[bytes_received] = '\0'; // Terminar la cadena recibida
    //     printf("Respuesta del Worker: %s\n", response);
    // } else if (bytes_received == 0) {
    //     printf("Conexión cerrada por Worker.\n");
    // } else {
    //     perror("Error al recibir respuesta del Worker");
    // }


    // Actualizar el estado del Worker
    worker->status = 100;  // Suponemos que el trabajo se completó con éxito
    // Cerrar la conexión
    
    freeDistortInfo(distortInfo);
    printf("Conexión cerrada con el Worker %s:%s\n", worker->IP, worker->Port);
    return NULL;
}