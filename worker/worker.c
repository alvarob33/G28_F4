
#define _GNU_SOURCE


#include "../gotham/gothamlib.h"

#include "worker.h"



// Función para leer el archivo de configuración de Enigma/Harley
Enigma_HarleyConfig* WORKER_read_config(const char *config_file) {
    int fd = open(config_file, O_RDONLY);
    if (fd < 0) {
        perror("Error abriendo el archivo de configuración");
        return NULL;
    }

    Enigma_HarleyConfig* config = (Enigma_HarleyConfig*) malloc(sizeof(Enigma_HarleyConfig));
    if (config == NULL) {
        perror("Error en malloc");
        close(fd);
        return NULL;
    }

    // Leer la IP de Gotham
    config->ip_gotham = read_until(fd, '\n');
    if (config->ip_gotham == NULL) {
        perror("Error leyendo la IP de Gotham");
        free(config);
        close(fd);
        return NULL;
    }
    // Eliminar caracteres invisibes de la IP
    eliminar_caracteres(config->ip_gotham);

    // Leer el puerto de Gotham
    char *port_gotham_str = read_until(fd, '\n');
    if (port_gotham_str == NULL) {
        perror("Error leyendo el puerto de Gotham");
        free(config->ip_gotham); // Liberar la IP leída previamente
        free(config);
        close(fd);
        return NULL;
    }
    config->port_gotham = atoi(port_gotham_str); // Convertir string a entero
    free(port_gotham_str); // Liberar el buffer del puerto

    // Leer la IP para el Worker
    config->ip_fleck = read_until(fd, '\n');
    if (config->ip_fleck == NULL) {
        perror("Error leyendo la IP del Worker");
        free(config->ip_gotham); // Liberar la memoria previamente asignada
        free(config);
        close(fd);
        return NULL;
    }
    // Eliminar caracteres invisibes de la IP
    eliminar_caracteres(config->ip_fleck);

    // Leer el puerto del Worker
    char *port_worker_str = read_until(fd, '\n');
    if (port_worker_str == NULL) {
        perror("Error leyendo el puerto del Worker");
        free(config->ip_gotham);
        free(config->ip_fleck); // Liberar la memoria previamente asignada
        free(config);
        close(fd);
        return NULL;
    }
    config->port_fleck = atoi(port_worker_str); // Convertir string a int
    free(port_worker_str); // Liberar el buffer del puerto

    // Leer el directorio de trabajo
    config->worker_dir = read_until(fd, '\n');
    if (config->worker_dir == NULL) {
        perror("Error leyendo el directorio del Worker");
        free(config->ip_gotham);
        free(config->ip_fleck);
        free(config);
        close(fd);
        return NULL;
    }
    // Eliminar caracteres invisibes de la IP
    eliminar_caracteres(config->worker_dir);

    // Leer el tipo de worker (Media o Text)
    config->worker_type = read_until(fd, '\n');
    if (config->worker_type == NULL) {
        perror("Error leyendo el tipo de Worker");
        free(config->ip_gotham);
        free(config->ip_fleck);
        free(config->worker_dir);
        free(config);
        close(fd);
        return NULL;
    }
    // Eliminar caracteres invisibes de la IP
    eliminar_caracteres(config->worker_type);

    close(fd);
    return config; // Devolver la configuración leída
}

void WORKER_print_config(Enigma_HarleyConfig* config) {
    char* buffer;

    asprintf(&buffer, "IP Gotham: %s\n", config->ip_gotham);
    printF(buffer);
    free(buffer);

    asprintf(&buffer, "Puerto Gotham: %d\n", config->port_gotham);
    printF(buffer);
    free(buffer);

    asprintf(&buffer, "IP Worker: %s\n", config->ip_fleck);
    printF(buffer);
    free(buffer);

    asprintf(&buffer, "Puerto Worker: %d\n", config->port_fleck);
    printF(buffer);
    free(buffer);

    asprintf(&buffer, "Directorio Worker: %s\n", config->worker_dir);
    printF(buffer);
    free(buffer);

    asprintf(&buffer, "Tipo de Worker: %s\n", config->worker_type);
    printF(buffer);
    free(buffer);

    printF("\n");
}


// LIBERAR MEMORIA 

void WORKER_cancel_and_wait_threads(pthread_t* subthreads, int num_subthreads) {

    // Cerrar y liberar subthreads
    for (int i = 0; i < num_subthreads; i++) {
        if (pthread_cancel(subthreads[i]) != 0) {
            perror("Error al cancelar el thread");
        }
        
        // Esperamos a que el thread termine
        if (pthread_join(subthreads[i], NULL) != 0) {
            perror("Error al esperar el thread");
        }
    }
    
    free(subthreads);
    num_subthreads = 0; 
}


/**
 * @brief Conecta un Worker (Enigma o Harley) al sistema Gotham.
 * 
 * Esta función establece una conexión TCP/IP con el servidor Gotham utilizando un socket,
 * enviando información sobre el Worker (tipoWorker, IP y puerto). Luego, espera y verifica
 * la respuesta del servidor para confirmar si la conexión fue exitosa.
 * 
 * - Si la conexión es exitosa, se devuelve el descriptor del socket abierto para continuar
 *   con la comunicación.
 * - Si ocurre algún error, se libera la memoria dinámica utilizada
 *   y se devuelve -1 para indicar fallo.
 * 
 * @param config Puntero a una estructura de configuración (Enigma_HarleyConfig) que contiene
 *               los parámetros necesarios para realizar la conexión (tipo de worker, IP y puerto de Gotham).
 * 
 * @return int El descriptor del socket si la conexión es exitosa, o -1 si ocurre algún error durante el proceso.
 */
int WORKER_connect_to_gotham(Enigma_HarleyConfig *config, int* isPrincipalWorker) {
    //char* buffer;

    printF("Reading configuration file\n");

    if (strcmp(config->worker_type, "Text") == 0) {
        printF ("Connecting Enigma worker to the system..\n");
    }

    if (strcmp(config->worker_type, "Media") == 0) {
        printF ("Connecting Harley worker to the system..\n");
    }


    // Crear socket
    int sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd < 0) {
        printF("Error al crear el socket\n");
        return -1;
    }

    // Configurar dirección de Gotham
    struct sockaddr_in gotham_address;
    gotham_address.sin_family = AF_INET;
    gotham_address.sin_port = config->port_gotham;
    if (inet_pton(AF_INET, config->ip_gotham, &gotham_address.sin_addr) <= 0) {
        printF("Dirección IP de Gotham no válida\n");
        close(sock_fd);
        return -1;
    }

    // Conectar al servidor Gotham
    if (connect(sock_fd, (struct sockaddr *)&gotham_address, sizeof(gotham_address)) < 0) {
        printF("Error al conectar con Gotham\n");
        close(sock_fd);
        return -1;
    }

    // Preparar la trama para enviar a Gotham
    char *data;
    asprintf(&data, "%s&%s&%d", config->worker_type, config->ip_fleck, config->port_fleck);    // Se envía port_fleck para que Gotham sepa el puerto al que se tendrán que conectar los Flecks con el Worker
    if (data == NULL) {
        printF("Error en malloc para data\n");
        close(sock_fd);
        return -1;
    }
    

    unsigned char *trama = crear_trama(TYPE_CONNECT_WORKER_GOTHAM, (unsigned char*)data, strlen(data));
    if (trama == NULL) {
        printF("Error creando la trama\n");
        free(data);
        close(sock_fd);
        return -1;
    }

    // Enviar la trama a Gotham
    if (write(sock_fd, trama, BUFFER_SIZE) < 0) {
        printF("Error enviando la trama de conexión a Gotham\n");
        free(data);
        free(trama);
        close(sock_fd);
        return -1;
    }

    // Leer la respuesta de Gotham
    char *response = (char *)malloc(BUFFER_SIZE);
    if (response == NULL) {
        printF("Error en malloc para response\n");
        free(data);
        free(trama);
        close(sock_fd);
        return -1;
    }
    int bytes_read = read(sock_fd, response, BUFFER_SIZE);
    if (bytes_read < 0) {
        printF("Error leyendo la respuesta de Gotham\n");
        free(data);
        free(trama);
        free(response);
        close(sock_fd);
        return -1;
    }

    // Verificar si la conexión fue exitosa
    if (response[0] == TYPE_CONNECT_WORKER_GOTHAM) {//&& response[3] == '\0'
        printF("Connected to Mr. J System as SECONDARY Worker, ready to be a PRINCIPAL Worker\n");
    } else if (response[0] ==  TYPE_PRINCIPAL_WORKER ) {//&& response[3] == '\0'
        printF("Connected to Mr. J System as PRINCIPAL Worker, ready to listen to Fleck petitions\n");
        *isPrincipalWorker = 1;
    } else {
        printF("Conexión rechazada por Gotham.\n");
        free(data);
        free(trama);
        free(response);
        close(sock_fd);
        return -1;
    }    

    // Liberar memoria dinámica
    free(data);
    free(trama);
    free(response);

    // Devolvemos el socket abierto por si se desea seguir trabajando con él
    return sock_fd;
}

/*  Funcion para mantenernos respondiendo mensajes de gotham. Estos mensajes pueden ser HEARTBEAT o indicarnos
    que se nos ha asignado como workers principales.
*/
void* responder_gotham(void *arg) {
    int socket_fd = *(int *)arg;  // Obtener el socket_fd desde el argumento
    unsigned char buffer[BUFFER_SIZE];
    unsigned char* tramaEnviar;

    while (1) {
        // Recibir mensaje del cliente
        int bytes_read = recv(socket_fd, buffer, BUFFER_SIZE, 0);
        
        if (bytes_read <= 0) {
            if (bytes_read == 0) {
                // El cliente cerró la conexión
                printF("Gotham ha cerrado la conexión.\n");
                raise(SIGINT);
                return NULL;
            } else {
                // Error en recv
                perror("Error leyendo mensaje de Gotham.\n");
            }
            close(socket_fd);
            return NULL;  // Terminar el thread si ocurre un error
        }
        buffer[bytes_read] = '\0'; // Asegurar que está null-terminado
        
        //Leer como trama
        TramaResult* result = leer_trama(buffer);
        if (result == NULL)
        {
            printF("Error leyendo tramaa.\n");
            return NULL;  // Terminar el hilo si ocurre un error
        } 
        else 
        {
            //Si la trama es un mensaje HEARTBEAT responder
            if (result->type == TYPE_HEARTBEAT)
            {
                // Responder al cliente
                tramaEnviar = crear_trama(TYPE_HEARTBEAT, (unsigned char*)"", strlen(""));
                if (write(socket_fd, tramaEnviar, BUFFER_SIZE) < 0) {
                    perror("Error enviando respuesta al cliente");
                    close(socket_fd);
                    return NULL;  // Terminar el hilo si ocurre un error
                }
                free(tramaEnviar);
            }

            //Si la trama es un mensaje Asignación de Worker principal
            else if (result->type == TYPE_PRINCIPAL_WORKER)
            {
                printF("Somos principal\n");
                // Salimos para crear servidor de Flecks y convertirnos en Worker principal (Gestionado en harley.c o enigma.c)
                return NULL;   
            }
            
        }
        
    }

    return NULL;
}

int WORKER_disconnect_from_gotham(int sock_fd, Enigma_HarleyConfig *config) {

    // Preparar la trama para enviar el mensaje de desconexión
    unsigned char *trama = crear_trama(TYPE_DISCONNECTION, (unsigned char*)config->worker_type, strlen(config->worker_type));
    if (trama == NULL) {
        printF("Error creando la trama de desconexión\n");
        return -1;
    }

    // Enviar la trama de desconexión a Gotham
    if (write(sock_fd, trama, BUFFER_SIZE) < 0) {
        printF("Error enviando la trama de desconexión a Gotham\n");
        free(trama);
        return -1;
    }

    free(trama);

    // Cerrar el socket
    if (sock_fd >= 0) {
        close(sock_fd);
    }

    printF("Disconnected from Gotham\n");
    return 0;
}
