#include "flecklib.h"
#include "flecklib_distort.h"

/***********************************************
*
* @Finalitat: Llegir i parsejar el fitxer de configuració de Fleck, extraient usuari, directori i
*             dades de connexió a Gotham.
* @Parametres:
*   in: config_file = ruta al fitxer de configuració.
* @Retorn: Punter a FleckConfig amb camps omplerts, o NULL en cas d’error.
*
************************************************/
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

/***********************************************
*
* @Finalitat: Gestionar senyal de sortida (SIGINT) imprimint missatge i sortint del procés.
* @Parametres: ---
* @Retorn: ---
*
************************************************/
void FLECK_signal_handler() {
    printF("\nSaliendo del programa...\n");
    exit(0);
}

/***********************************************
*
* @Finalitat: Establir connexió TCP amb Gotham mitjançant la configuració, enviar trama de CONNECT, 
*               i tornar el descriptor de socket.
* @Parametres:
*   in: config = punter a FleckConfig amb IP i port.
* @Retorn: Descriptor de socket en èxit, -1 en error.
*
************************************************/
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
    gotham_addr.sin_port = config->gotham_port;

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
    char* data;
    eliminar_caracteres(config->username); 
    asprintf(&data, "%s&%s&%d", config->username, config->gotham_ip, config->gotham_port); // Formato: <username>&<IP>&<Port>

    unsigned char *trama = crear_trama(TYPE_CONNECT_FLECK_GOTHAM, (unsigned char*)data, strlen(data)); // Crear trama con TYPE = 0x01
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

/***********************************************
*
* @Finalitat: Mostrar per pantalla l’estat de distorsió dels workers de tipus Text i Media.
* @Parametres:
*   in: worker_text  = punter a WorkerFleck de text (o NULL).
*   in: worker_media = punter a WorkerFleck de media (o NULL).
* @Retorn: ---
*
************************************************/
void mostrar_estado_workers(WorkerFleck* worker_text, WorkerFleck* worker_media) {
    char *estado_text = NULL;
    char *estado_media = NULL;
    
    printF("\n========= ESTADO DE WORKERS =========\n\n");
    
    // Formatear estado worker texto
    if (worker_text == NULL) {
        asprintf(&estado_text, "Worker de Texto: No tiene distorsión activa\n");
    } else {
        asprintf(&estado_text, "Worker de Texto [%s:%s]: %d%% completado\n", 
                worker_text->IP, worker_text->Port, worker_text->status);
    }

    printF(estado_text);
    free(estado_text); 
    
    // Formatear estado worker media
    if (worker_media == NULL) {
        asprintf(&estado_media, "Worker de Media:  No tiene distorsión activa\n");
    } else {
        asprintf(&estado_media, "Worker de Media  [%s:%s]: %d%% completado\n", 
                worker_media->IP, worker_media->Port, worker_media->status);
    }
    printF(estado_media);
    free(estado_media); 
    
    printF("\n=====================================\n\n");
}

/***********************************************
*
* @Finalitat: Processar el menú interactiu de Fleck, acceptant i executant comandes: connect,
*             list, distort, check status, clear, logout.
* @Parametres:
*   in: config = punter a FleckConfig amb dades de sessió.
* @Retorn: ---
*
************************************************/
void FLECK_handle_menu(FleckConfig *config) {
    char input[64];     // Establecemos que el máximo de caracteres que se pueden introducir por terminal son 64
    char* buffer = NULL;

    WorkerFleck* worker_text = NULL;
    WorkerFleck* worker_media = NULL;
    // int num_workers = 0;
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

            distortInfo->socket_gotham = socket_gotham; // Guardamos el socket de conexión con Gotham
            distortInfo->username = strdup(config->username);
            distortInfo->user_dir = strdup(config->user_dir);
            distortInfo->filename = strdup(strtok(NULL, " \t\n"));
            distortInfo->distortion_factor = strdup(strtok(NULL, " \t\n"));
            char *extra = strtok(NULL, " \t\n");

            if (distortInfo->filename && distortInfo->distortion_factor && extra == NULL) {
                printF("Command OK\n");

                // Obtener tipo de media del archivo
                char* mediaType = file_type(distortInfo->filename);
                if (mediaType == NULL) {
                    printF("Cancelando: Media type no reconocido.\n");
                    freeDistortInfo(distortInfo); 
                    continue;
                }

                if (strcmp(mediaType, MEDIA) == 0 && worker_media != NULL)
                {
                    printF("Cancelando: Ya hay una distorsión 'Media' en curso.\n");
                    freeDistortInfo(distortInfo);
                    continue;
                } else if (strcmp(mediaType, TEXT) == 0 && worker_text != NULL)
                {
                    printF("Cancelando: Ya hay una distorsión 'Text' en curso.\n");
                    freeDistortInfo(distortInfo);
                    continue;
                } 

                pthread_t thread_id;
                // Crear hilo para enviar solicitud distort
                if (strcmp(mediaType, MEDIA))
                {
                    if (request_distort_gotham(socket_gotham, mediaType, &worker_media, distortInfo) > 0)
                    {
                        if (pthread_create(&thread_id, NULL, handle_distort_worker, (void*)distortInfo) != 0) {
                            perror("Error al crear el hilo");
                        }
                    } else {
                        perror("Error solicitando distort a Gotham.\n");
                        freeDistortInfo(distortInfo);
                        continue;
                    }
                } else if (strcmp(mediaType, TEXT))
                {
                    if (request_distort_gotham(socket_gotham, mediaType, &worker_text, distortInfo) > 0)
                    {
                        if (pthread_create(&thread_id, NULL, handle_distort_worker, (void*)distortInfo) != 0) {
                            perror("Error al crear el hilo");
                        }
                    } else {
                        perror("Error solicitando distort a Gotham.\n");
                        freeDistortInfo(distortInfo);
                        continue;
                    }
                }
                

            } else {
                freeDistortInfo(distortInfo); // Liberar memoria si los argumentos son incorrectos
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
                    mostrar_estado_workers(worker_text, worker_media);
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
                    unsigned char *trama = crear_trama(TYPE_DISCONNECTION, (unsigned char*)"LOGOUT", strlen("LOGOUT"));
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
