

#include "flecklib.h"
#include "flecklib_distort.h"


// Función para leer el archivo de configuración de Fleck
FleckConfig* FLECK_read_config(const char *config_file) {
    int fd = open(config_file, O_RDONLY);
    if (fd < 0) {
        perror("Error abriendo el archivo de configuración");
        return NULL;
    }

    FleckConfig* config = (FleckConfig*) malloc(sizeof(FleckConfig));
    if (config == NULL) {
        perror("Error en malloc");
        close(fd);
        return NULL;
    }

    // Leer el nombre de usuario
    config->username = read_until(fd, '\n');
    if (config->username == NULL) {
        perror("Error leyendo el nombre de usuario");
        free(config);
        close(fd);
        return NULL;
    }

    // Eliminar el '&' del username
    remove_ampersand(config->username);

    // Leer el directorio de usuario
    config->user_dir = read_until(fd, '\n');
    if (config->user_dir == NULL) {
        perror("Error leyendo el directorio del usuario");
        free(config->username);
        free(config);
        close(fd);
        return NULL;
    }

    // Leer la IP de Gotham
    config->gotham_ip = read_until(fd, '\n');
    if (config->gotham_ip == NULL) {
        perror("Error leyendo la IP de Gotham");
        free(config->username);
        free(config->user_dir);
        free(config);
        close(fd);
        return NULL;
    }

    // Leer el puerto de Gotham
    char *buffer = read_until(fd, '\n');
    if (buffer == NULL) {
        perror("Error leyendo el puerto de Gotham");
        free(config->username);
        free(config->user_dir);
        free(config->gotham_ip);
        free(config);
        close(fd);
        return NULL;
    }
    config->gotham_port = atoi(buffer); // Convertir string a entero
    free(buffer); // Liberar el buffer del puerto

    close(fd);
    return config; // Devolver la configuración
}

void FLECK_signal_handler() {
    printF("\nSaliendo del programa...\n");
    exit(0);
}

int FLECK_connect_to_gotham(FleckConfig *config) {
    printF("Iniciando conexión de Fleck con Gotham...\n");

    // Crear socket
    int sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd < 0) {
        perror("Error al crear el socket");
        return -1;
    }

    // Configurar la dirección de Gotham
    struct sockaddr_in gotham_addr;
    gotham_addr.sin_family = AF_INET;
    gotham_addr.sin_port = htons(config->gotham_port);

    // Eliminar caracteres invisibles de la IP
    eliminar_caracteres(config->gotham_ip); // Asegurarnos de limpiar la IP

    // DEBUGGING: Imprimir la IP después de limpiarla
    // printf("Conectando a Gotham en IP: '%s', Puerto: %d\n", config->gotham_ip, config->gotham_port);

    // Convertir IP de Gotham
    if (inet_pton(AF_INET, config->gotham_ip, &gotham_addr.sin_addr) <= 0) {
        perror("Dirección IP de Gotham no válida");
        close(sock_fd);
        return -1;
    }

    // Conectar con Gotham
    if (connect(sock_fd, (struct sockaddr *)&gotham_addr, sizeof(gotham_addr)) < 0) {
        perror("Error al conectar con Gotham");
        close(sock_fd);
        return -1;
    }

    printF("Conexión establecida con Gotham, enviando datos...\n");

    // Crear trama para enviar
    unsigned char data[BUFFER_SIZE];
    eliminar_caracteres(config->username); 
    snprintf(data, sizeof(data), "%s&%s&%d", config->username, config->gotham_ip, config->gotham_port); // Formato: <username>&<IP>&<Port>

    unsigned char *trama = crear_trama(TYPE_CONNECT_FLECK_GOTHAM, data); // Crear trama con TYPE = 0x01
    if (trama == NULL) {
        perror("Error al crear la trama");
        close(sock_fd);
        return -1;
    }

    // DEBUGGING
    // printf("Trama enviada: TYPE=0x%02x, DATA=%s\n", trama[0], &trama[3]);

    // Enviar trama a Gotham
    if (write(sock_fd, trama, BUFFER_SIZE) < 0) {
        perror("Error enviando trama a Gotham");
        free(trama);
        close(sock_fd);
        return -1;
    }

    free(trama); // Liberar la memoria usada para la trama

    // Leer respuesta de Gotham
    unsigned char response[BUFFER_SIZE];
    int bytes_read = read(sock_fd, response, BUFFER_SIZE);
    if (bytes_read <= 0) {
        perror("Error leyendo respuesta de Gotham");
        close(sock_fd);
        return -1;
    }

    TramaResult *result = leer_trama(response); // Procesar la respuesta recibida
    if (result == NULL) {
        printF("Error en la trama recibida de Gotham.\n");
        close(sock_fd);
        return -1;
    }

    if (result->type == TYPE_CONNECT_FLECK_GOTHAM)
    {
        printF("Conexión aceptada por Gotham.\n");
        free_tramaResult(result);
        return sock_fd;                 // Retornar fd del socket abierto con Gotham
    } else {
        char* buffer;
        asprintf(&buffer, "Respuesta desconocida de Gotham: , DATA=%s\n", result->data);
        printF(buffer);

        free_tramaResult(result);
        close(sock_fd);
        return -1;
    }
    
    // Verificar la respuesta de Gotham
    /*
    if (strcmp(result->data, "OK") == 0) {
        printF("Conexión aceptada por Gotham.\n");
    } else if (strcmp(result->data, "CON_KO") == 0) {
        printF("Conexión rechazada por Gotham.\n");
        free_tramaResult(result);
        close(sock_fd);
        return -1;
    } else {
        printF("Respuesta desconocida de Gotham.\n");
        free_tramaResult(result);
        close(sock_fd);
        return -1;
    }
    */
    

}

/* DISTORT */ 


// Función para manejar el menú de opciones
void FLECK_handle_menu(FleckConfig *config) {
    char input[64];     // Establecemos que el máximo de caracteres que se pueden introducir por terminal son 64
    char* buffer = NULL;

    WorkerFleck* worker_text = NULL;
    WorkerFleck* worker_media = NULL;
    int num_workers = 0;
    int socket_gotham = -1; // Socket de conexión con Gotham

    //  pthread_t heartbeat_thread; // Hilo para el heartbeat

    while (1) {
        printF("\n$ ");

        ssize_t bytes_read = read(0, input, 64-1);  // Leer input del usuario
        input[bytes_read] = '\0';

        // Eliminar espacios adicionales y obtener el comando principal
        char *cmd = strtok(input, " \t\n");
        if (cmd == NULL) continue;

        // Convertir el comando a minúsculas
        for (int i = 0; cmd[i]; i++) {
            cmd[i] = tolower(cmd[i]);
        }

        // CONNECT
        if (strcmp(cmd, "connect") == 0) {
            char *arg = strtok(NULL, " \t\n");
            if (arg == NULL) {
                printF("Command OK\n");

                if (socket_gotham == -1) {
                    socket_gotham = FLECK_connect_to_gotham(config);
                    if (socket_gotham < 0) {
                        printF("Error al conectar Fleck con Gotham.\n");
                        socket_gotham = -1;
                    } else {
                        printF("Conexión establecida con Gotham.\n");
                        // if (pthread_create(&heartbeat_thread, NULL, enviar_heartbeat_constantemente, &socket_gotham) != 0) {
                        //     perror("Error al crear el hilo de heartbeat");
                        //     close(socket_gotham);
                        //     socket_gotham = -1;
                        // }
                    }
                } else {
                    printF("Ya estás conectado a Gotham.\n");
                }

            } else {
                printF("Unknown command\n");
            }

        // LIST
        } else if (strcmp(cmd, "list") == 0) {
            char *arg = strtok(NULL, " \t\n");
            if (arg && (strcasecmp(arg, "media") == 0 || strcasecmp(arg, "text") == 0)) {
                char *extra = strtok(NULL, " \t\n");
                if (extra == NULL) {
                    if (strcasecmp(arg, "media") == 0) {
                        asprintf(&buffer, "Listando archivos multimedia en el directorio %s:\n", config->user_dir);
                        printF(buffer);
                        free(buffer);
                        list_files(config->user_dir, ".wav");
                        list_files(config->user_dir, ".jpg");
                        list_files(config->user_dir, ".png");
                    } else {
                        asprintf(&buffer, "Listando archivos de texto en el directorio %s:\n", config->user_dir);
                        printF(buffer);
                        free(buffer);
                        list_files(config->user_dir, ".txt");
                    }
                } else {
                    printF("Unknown command\n");
                }
            } else {
                printF("Command KO\n");
                printF("Uso: list <media|text>\n");
            }


        // DISTORT
        } else if (strcmp(cmd, "distort") == 0) {
            
            if (socket_gotham == -1) {
                printF("No estás conectado a Gotham. Usa el comando 'connect' primero.\n");
                continue;   // Pasamos a la siguiente iteración del bucle
            }

            // Parsear partes comando separadas por espacios
            DistortInfo* distortInfo = (DistortInfo *)malloc(sizeof(DistortInfo));
            if (distortInfo == NULL) {
                perror("Failed to allocate memory for distortInfo");
                continue;
            }

            distortInfo->username = strdup(config->username);
            distortInfo->filename = strdup(strtok(NULL, " "));
            distortInfo->distortion_factor = strdup(strtok(NULL, " "));
            char *extra = strtok(NULL, " \t\n");

            if (distortInfo->filename && distortInfo->distortion_factor && extra == NULL) {
                printF("Command OK\n");

                // Obtener tipo de media del archivo
                char* mediaType = file_type(distortInfo->filename);
                if (mediaType == NULL) {
                    printF("Cancelando: Media type no reconocido.\n");
                    continue;
                }

                if (strcmp(mediaType, MEDIA) == 0 && worker_media != NULL)
                {
                    printF("Cancelando: Ya hay una distorsión 'Media' en curso.\n");
                    continue;
                } else if (strcmp(mediaType, TEXT) == 0 && worker_text != NULL)
                {
                    printF("Cancelando: Ya hay una distorsión 'Text' en curso.\n");
                    continue;
                } 

                // Enviar petición de distort a Gotham (y guardar mediaType del archivo)
                sendDistortGotham(distortInfo->filename, socket_gotham, mediaType);
                //Leer respuesta de Gotham como trama
                TramaResult* result = receiveDistortGotham(socket_gotham);
                if (result == NULL) {
                    perror("Error leyendo trama.\n");
                    continue;
                }

                // Comprobar si la trama es un mensaje DISTORT
                if (result->type == TYPE_DISTORT_FLECK_GOTHAM)
                {
                    // Si no hay Workers de nuestro tipo disponibles salir
                    if (strcmp(result->data, "DISTORT_KO") == 0) {
                        asprintf(&buffer, "No hay Workers de %s disponibles.\n", mediaType);
                        printF(buffer);
                        free_tramaResult(result);
                        continue;
                    } // Si el media no fue reconocido por Gotham salir
                    else if (strcmp(result->data, "MEDIA_KO") == 0)
                    {
                        asprintf(&buffer, "Media type '%s' no reconocido.\n", mediaType);
                        printF(buffer);
                        free_tramaResult(result);
                        continue;
                    }
                    

                    // Si hay Worker disponible
                    ///
                    pthread_t thread_id;
                    if (strcmp(mediaType, MEDIA))
                    {
                        // Guardar info Worker
                        if (store_new_worker(result, &worker_media, mediaType) < 1) {
                            perror("Error al guardar el WorkerFleck");
                            continue;
                        }
                        distortInfo->worker_ptr = &worker_media;
                        
                        // Crear hilo para enviar solicitud distort a Worker
                        if (pthread_create(&thread_id, NULL, handle_distort_worker, (void*)distortInfo) != 0) {
                            perror("Error al crear el hilo");
                        }
                    } else if (strcmp(mediaType, TEXT))
                    {
                        // Guardar info Worker
                        if (store_new_worker(result, &worker_text, mediaType) < 1) continue;
                        distortInfo->worker_ptr = &worker_text;

                        // Crear hilo para enviar solicitud distort a Worker
                        if (pthread_create(&thread_id, NULL, handle_distort_worker, (void*)distortInfo) != 0) {
                            perror("Error al crear el hilo");
                        }
                    }
                    
                    // No esperamos a que el hilo termine, porque queremos seguir pudiendo generar conexiones.
                    pthread_detach(thread_id);
                    
                    
                } else {
                    perror("Error: El mensaje recibido de Gotham es inesperado.\n");
                    free_tramaResult(result);
                    continue;
                }
                


            } else {
                printF("Commando Incorrecto.\n");
                printF("Uso: distort <filename> <factor>\n");
            }

        // CHECK STATUS
        } else if (strcmp(cmd, "check") == 0) {
            char *arg = strtok(NULL, " \t\n");
            if (arg && strcasecmp(arg, "status") == 0) {
                char *extra = strtok(NULL, " \t\n");
                if (extra == NULL) {
                    printF("Command OK\n");
                } else {
                    printF("Unknown command\n");
                }
            } else {
                printF("Command KO\n");
            }

        // CLEAR
        } else if (strcmp(cmd, "clear") == 0) {
            char *arg = strtok(NULL, " \t\n");
            if (arg && strcasecmp(arg, "all") == 0) {
                char *extra = strtok(NULL, " \t\n");
                if (extra == NULL) {
                    printF("Command OK\n");
                } else {
                    printF("Unknown command\n");
                }
            } else {
                printF("Command KO\n");
            }

        // LOGOUT
        } else if (strcmp(cmd, "logout") == 0) {
            char *arg = strtok(NULL, " \t\n");
            if (arg == NULL) {
                //printF("Thanks for using Mr. J System, see you soon, chaos lover :)\n");

                if (socket_gotham != -1) {
                unsigned char *trama = crear_trama(0x03, "LOGOUT");
                    if (send(socket_gotham, trama, BUFFER_SIZE, 0) < 0) {
                        perror("Error enviando comando de logout a Gotham");
                    } else {
                        printF("Desconexión solicitada a Gotham.\n");
                    }
                free(trama);
                // pthread_cancel(heartbeat_thread);
                close(socket_gotham);
                socket_gotham = -1;

                } else {
                    printF("No estás conectado a Gotham.\n");
                }

                break;  // Salir del bucle y desconectar
            } else {
                printF("Unknown command\n");
            }

        } else {
            printF("Unknown command\n");
            //printF("Comando desconocido. Intenta 'connect', 'distort', 'list', o 'logout'.\n");
        }
    }
}
