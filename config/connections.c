
#define _GNU_SOURCE
#include <stdio.h>
#include <time.h>
#include <sys/socket.h>
#include <pthread.h>
#include <stdint.h>

#include "connections.h"


/***********************************************
*
* @Finalitat: Crear i configurar un servidor TCP en l’adreça i port indicats, preparant-lo per acceptar connexions.
* @Parametres:
*   in:  ip_addr         = cadena amb l’adreça IP on escoltar.
*   in:  port            = port en format host.
*   in:  max_connections = nombre màxim de connexions en cua.
* @Retorn: Punter a una estructura Server inicialitzada, o interromp l’execució en cas d’error greu.
*
************************************************/
Server* create_server(char* ip_addr, int port, int max_connections) {
    Server* server = (Server*)malloc(sizeof(Server)); 
    if (server == NULL) {
        perror("Error al asignar memoria para el servidor");
        exit(EXIT_FAILURE);
    }

    // Crear el socket (IPv4 y TCP)
    if ((server->server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Error al crear el socket");
        exit(EXIT_FAILURE);
    }

    // Configurar opción SO_REUSEADDR para evitar "Address already in use"
    int opt = 1;
    if (setsockopt(server->server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("Error al configurar SO_REUSEADDR");
        close(server->server_fd);
        free(server);
        exit(EXIT_FAILURE);
    }

    // Configurar la dirección del servidor
    memset(&server->address, 0, sizeof(server->address));
    server->address.sin_family = AF_INET;        // IPv4
    server->address.sin_addr.s_addr = inet_addr(ip_addr); // Aceptar conexiones en cualquier interfaz
    server->address.sin_port = port;     // Puerto en formato de red
    server->port = port;
    server->max_connections = max_connections;

    // Enlazar el file descriptor del socket a la dirección y puerto
    if (bind(server->server_fd, (struct sockaddr *)&server->address, sizeof(server->address)) < 0) {
        perror("Error al enlazar el socket servidor");
        close(server->server_fd);
        exit(EXIT_FAILURE);
    }

    return server;
}

/***********************************************
*
* @Finalitat: Posar el servidor en mode “escolta” per començar a acceptar connexions entrats.
* @Parametres:
*   in: server = punter al servidor previament creat.
* @Retorn: --- (aborta l’execució si no pot escoltar)
*
************************************************/
void start_server(Server *server) {
    char* buffer;

    if (server == NULL)
    {
        printF("Error: Estructura del servidor no creada.\n");
        return;
    }
    
    if (listen(server->server_fd, server->max_connections) < 0) {
        perror("Error al iniciar la escucha.\n");
        close(server->server_fd);
        exit(EXIT_FAILURE);
    }

    asprintf(&buffer, "Servidor escuchando en el puerto %d..\n", server->port);
    printF(buffer);
    free(buffer);
}

/***********************************************
*
* @Finalitat: Tancar el socket del servidor i alliberar recursos associats.
* @Parametres:
*   in: server = punter al servidor a tancar.
* @Retorn: ---
*
************************************************/
void close_server(Server *server) {
    if (server == NULL) return;

    if (server->server_fd >= 0) {
        close(server->server_fd);
    }
    printF("Servidor cerrado.\n");
}

/***********************************************
*
* @Finalitat: Acceptar una nova connexió de client sobre el servidor especificat.
* @Parametres:
*   in: server = punter al servidor que escolta.
* @Retorn: Descriptor del socket del client acceptat, o -1 en cas d’error.
*
************************************************/
int accept_connection(Server *server) {
    socklen_t addrlen = sizeof(server->address);
    if (server == NULL) {
        perror( "Error: servidor no inicializado.\n");
        return -1;
    }

    int new_socket = accept(server->server_fd, (struct sockaddr *)&server->address, &addrlen);
    if (new_socket < 0) {
        perror("Error al aceptar la conexión");
        return -1;
    }
    
    return new_socket;
}

// POST: se debe hacer free() de la trama devuelta
/***********************************************
*
* @Finalitat: Crear una trama de mida fixa (BUFFER_SIZE) amb tipus, llargada de dades, dades, checksum i timestamp.
* @Parametres:
*   in: TYPE        = byte de tipus de la trama.
*   in: data        = punter a les dades a enviar.
*   in: data_length = longitud de les dades (<=247).
* @Retorn: Punter a un buffer de `unsigned char` de mida BUFFER_SIZE, o NULL en cas d’error.
*
************************************************/
unsigned char* crear_trama(int TYPE, unsigned char* data, size_t data_length) {
    // Preparar la trama para enviar
    // [1B] TYPE, [2B] DATA_LENGTH, [247B] DATA, [2B] CHECKSUM, [4B] TIMESTAMP
    
    if (data_length > 247) {
        printF("Error: los datos superan el tamaño permitido (247 bytes).\n");
        return NULL;
    }

    unsigned char *trama = (unsigned char *)malloc(BUFFER_SIZE);
    if (trama == NULL) {
        printF("Error en malloc para trama\n");
        return NULL;
    }

    memset(trama, 0, BUFFER_SIZE);  // Inicializar la trama a ceros
    trama[0] = TYPE; // TYPE
    trama[1] = (data_length >> 8) & 0xFF; // DATA_LENGTH (parte alta)
    trama[2] = data_length & 0xFF;        // DATA_LENGTH (parte baja)
    
    // Copiar los datos binarios en la sección de DATA
    memcpy(&trama[3], data, data_length);

    // Obtención del timestamp (4 bytes a partir de trama[252])
    time_t timestamp = time(NULL);
    trama[252] = (timestamp >> 24) & 0xFF; // Byte más significativo
    trama[253] = (timestamp >> 16) & 0xFF;
    trama[254] = (timestamp >> 8) & 0xFF;
    trama[255] = timestamp & 0xFF;         // Byte menos significativo
    
    // Cálculo del checksum (suma de los bytes de trama[0] hasta trama[249])
    unsigned short checksum = 0; // Usamos 2 bytes (16 bits) para el checksum
    for (int i = 0; i < 250; i++) {
        checksum += trama[i]; // Sumamos cada byte
    }
    for (int i = 252; i < 256; i++) {
        checksum += trama[i]; // Sumamos cada byte
    }
    checksum %= 65536;  // Por si el resultado supera 16 bits
    trama[250] = (checksum >> 8) & 0xFF; // Parte alta del checksum
    trama[251] = checksum & 0xFF;        // Parte baja del checksum

    return trama;
}

/***********************************************
*
* @Finalitat: Analitzar una trama rebuda, validar checksum, extreure tipus, timestamp i dades.
* @Parametres:
*   in: trama = buffer de mida BUFFER_SIZE amb la trama.
* @Retorn: Punter a TramaResult amb camps omplerts, o NULL si checksum incorrecte o error.
*
************************************************/
// POST: se debe hacer free_tramaResult().
TramaResult* leer_trama(unsigned char *trama) {
    if (trama == NULL) return NULL;
    
    // Comprobar CHECKSUM
    unsigned short checksum_calculado = 0;
    for (int i = 0; i < 250; i++) {
        checksum_calculado += trama[i]; // Sumar cada byte
    }
    for (int i = 252; i < 256; i++) {
        checksum_calculado += trama[i]; // Sumar cada byte
    }
    unsigned short checksum_enviado = (trama[250] << 8) | trama[251]; // Reconstruir el checksum 

    /* Para DEBUGGING: Comparar checksums */
    // printf("Type: 0x%04x\n", trama[0]);
    // printf("Checksum calculado: 0x%04x, Checksum recibido: 0x%04x\n", checksum_calculado, checksum_enviado);

    if (checksum_calculado != checksum_enviado) {
        printF("Error: Checksum inválido.\n");
        return NULL;
    }

    // Crear el struct de resultado
    TramaResult *result = (TramaResult *)malloc(sizeof(TramaResult));
    if (result == NULL) {
        printF("Error: No se pudo asignar memoria para la estructura TramaResult.\n");
        return NULL;
    }
    
    result->type = trama[0];
    result->timestamp = NULL;
    result->data = NULL;


    // Leer el campo data_length 
    result->data_length = (trama[1] << 8) | trama[2]; // Reconstruir la longitud de los datos
    if (result->data_length > 247 || result->data_length < 0) {
        printF("Error: Longitud inválida.\n");
        free(result);
        return NULL;
    }

    // Copiar los datos
    result->data = (char *)malloc(result->data_length + 1); // +1 para el terminador nulo
    if (result->data == NULL) {
        printF("Error: No se pudo asignar memoria para los datos.\n");
        free(result);
        return NULL;
    }
    memcpy(result->data, &trama[3], result->data_length);
    result->data[result->data_length] = '\0'; // Asegurar que los datos están terminados en nulo

    // Obtener el timestamp y guardarlo en TramaResult
    time_t timestamp = (trama[252] << 24) | (trama[253] << 16) | (trama[254] << 8) | trama[255];
    result->timestamp = ctime(&timestamp);
    if (result->timestamp == NULL) {
        printF("Error: No se pudo convertir el timestamp.\n");
        free(result->data);
        free(result);
        return NULL;
    }

    return result;
}

/***********************************************
*
* @Finalitat: Alliberar memòria associada a un TramaResult (només 'data', la timestamp és estàtica).
* @Parametres:
*   in: result = punter a TramaResult a alliberar.
* @Retorn: --- (allibera result->data i la pròpia estructura).
*
************************************************/
void free_tramaResult(TramaResult *result) {
    if (result == NULL) return;

    // Liberamos la memoria de 'data'
    if (result->data != NULL) {
        free(result->data);
        result->data = NULL;  
    }

    // 'timestamp' no se libera porque ctime() devuelve un puntero que apunta a memoria estática
    result->timestamp = NULL;

    // Finalmente, liberamos la memoria de la estructura en sí
    free(result);
}


/***********************************************
*
* @Finalitat: Enviar heartbeats periòdics per mantenir viva la connexió amb un client.
* @Parametres:
*   in: socket_fd = descriptor del socket del client.
* @Retorn: Retorna quan el client tanca o hi ha error.
*
************************************************/
void enviar_heartbeat_constantemente(int socket_fd) {
    
    unsigned char buffer[BUFFER_SIZE];
    unsigned char* tramaEnviar;

    while (1) {
        // Enviar el mensaje de heartbeat
        tramaEnviar = crear_trama(TYPE_HEARTBEAT, (unsigned char*)"", strlen(""));
        if (write(socket_fd, tramaEnviar, BUFFER_SIZE) < 0) {
            perror("Error enviando heartbeat");
            close(socket_fd);
            return;
        }
        free(tramaEnviar);

        // Esperar la respuesta del cliente
        int bytes_read = recv(socket_fd, buffer, BUFFER_SIZE, 0);
        if (bytes_read <= 0) {
            if (bytes_read == 0) {
                printF("El cliente ha cerrado la conexión..\n");
            } else {
                // Error en recv
                perror("Error leyendo respuesta del cliente");
            }
            //close(socket_fd);     //COMENTADO: Ya lo gestiona remove_worker()
            return;  // Terminar el hilo si ocurre un error
        }

        // Leer respuesta del cliente
        TramaResult* result = leer_trama(buffer);
        if (result->type == TYPE_HEARTBEAT) {
            /* Descomentar para debugar HEARTBEAT*/
            //  printf("Respuesta del cliente: %s\n", buffer);
        } else if (result->type == TYPE_DISCONNECTION)
        {
            /* Desconexión, salir de HEARTBEAT */
            printF("El cliente ha cerrado la conexión...\n");
            //close(socket_fd);     //COMENTADO: Ya lo gestiona remove_worker()
            return;
        }
        
        free(result);

        // Esperar antes de enviar el siguiente heartbeat
        sleep(HEARTBEAT_SLEEP_TIME); // Enviar un heartbeat cada X segundos
    }
    
}

/***********************************************
*
* @Finalitat: Responder automàticament a heartbeats rebuts des del servidor.
* @Parametres:
*   in: arg = punter a integer amb el socket_fd.
* @Retorn: Retorna quan el servidor tanca o hi ha error.
*
************************************************/
void* responder_heartbeat_constantemente(void *arg) {
    int socket_fd = *(int *)arg;  // Obtener el socket_fd desde el argumento
    unsigned char buffer[BUFFER_SIZE];
    unsigned char* tramaEnviar;

    while (1) {
        // Leer el mensaje del servidor
        int bytes_read = recv(socket_fd, buffer, BUFFER_SIZE, 0);

        if (bytes_read <= 0) {
            if (bytes_read == 0) {
                // El servidor cerró la conexión
                printF("El servidor ha cerrado la conexión.\n");
            } else {
                // Error en recv
                perror("Error leyendo mensaje del servidor");
            }
            close(socket_fd);
            pthread_exit(NULL);  // Terminar el thread si ocurre un error
        }

        
        TramaResult* trama = leer_trama(buffer);
        if (trama->type == TYPE_HEARTBEAT)
        {
            //Si la trama es un mensaje HEARTBEAT responder con OK
            // Responder al servidor
            tramaEnviar = crear_trama(TYPE_HEARTBEAT, (unsigned char*)"", strlen(""));
            if (write(socket_fd, tramaEnviar, BUFFER_SIZE) < 0) {
                perror("Error enviando respuesta al servidor");
                close(socket_fd);
                pthread_exit(NULL);  // Terminar el hilo si ocurre un error
            }
        }
        
    }

    return NULL;
}