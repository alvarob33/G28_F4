#define _GNU_SOURCE
#include <stdio.h>
#include <signal.h>
#include <unistd.h> 
#include <sys/wait.h> // Necesario para usar la funcion wait() [forks]
#include <sys/select.h>
#include <sys/types.h>
#include <pthread.h>

#include "gothamlib.h"


/* Variables globales */ 
GlobalInfoGotham* globalInfo = NULL;
 

// Funcion administra cierre de proceso padre
void handle_sigint(/*int sig*/) {

    printF("\nCerrando programa de manera segura...\n");

    // Liberar la memoria dinámica asignada
    free(globalInfo->config->ip_fleck);
    free(globalInfo->config->ip_workers);
    free(globalInfo->config);


    /* FLECK*/

    // Enviar mensajes de desconexion a Flecks
    //
    
    close_server(globalInfo->server_fleck);


    /* WORKER */

    // Enviar mensajes de desconexion a Workers
    //

    close_server(globalInfo->server_worker);


    pthread_mutex_lock(&globalInfo->worker_mutex);

    printF("Liberando memoria workers...\n");
    // Liberar memoria de los workers
    liberar_memoria_workers(globalInfo);

    pthread_mutex_unlock(&globalInfo->worker_mutex);

    // Destruir el mutex
    pthread_mutex_destroy(&globalInfo->worker_mutex);

    free(globalInfo);
    

    printF("Recursos liberados correctamente. Saliendo...\n\n");

    // Salir del programa
    return 0;
}


// Funcion para crear Servidor de Flecks y administrar las conexiones entrantes
void* fleck_server(/*void* arg*/) {

    // Configurar servidor
    globalInfo->server_fleck = create_server(config->ip_fleck, config->port_fleck, 10);
    start_server(globalInfo->server_fleck);


    printf("Esperando conexiones de Flecks...\n");
    //Bucle para leer cada petición que nos llegue de un fleck
    while (1)
    {
        // Creamos struct con argumentos para pasarle al thread
        ThreadArgsGotham* args = malloc(sizeof(ThreadArgsGotham));
        if (args == NULL) {
            perror("Error al asignar memoria para ThreadArgsGotham");
            continue;
        }
        args->socket_connection = accept_connection(globalInfo->server_fleck);
        args->globalInfo = globalInfo;

        // Crear un hilo para manejar la conexión con el cliente
        pthread_t thread_id;
        if (pthread_create(&thread_id, NULL, handle_fleck_connection, (void*)args) != 0) {
            perror("Error al crear el hilo");
        }

        // No esperamos a que el hilo termine, porque queremos seguir aceptando conexiones.
        pthread_detach(thread_id);

    }
    close_server(globalInfo->server_fleck);

    return NULL;
}

// Funcion para crear Servidor de Workers y administrar las conexiones entrantes
void* workers_server(/*void* arg*/) {

    int socket_connection;

    // Crear y configurar servidor
    globalInfo->server_worker = create_server(config->ip_workers, config->port_workers, 10);
    start_server(server_worker);


    //Bucle para leer cada conexion que nos llegue de un worker
    printF("Esperando conexiones de Workers...\n");
    while (1)
    {
        // Creamos struct con argumentos para pasarle al thread
        ThreadArgsGotham* args = malloc(sizeof(ThreadArgsGotham));
        if (args == NULL) {
            perror("Error al asignar memoria para ThreadArgsGotham");
            continue;
        }
        args->socket_connection = accept_connection(globalInfo->server_fleck);  // Aceptamos conexion
        args->globalInfo = globalInfo;

        // Crear un hilo para manejar la conexión con el cliente(Worker)
        pthread_t thread_id;
        if (pthread_create(&thread_id, NULL, handle_worker_connection, (void*)args) != 0) {
            perror("Error al crear el hilo");
        }

        // No esperamos a que el hilo termine, porque queremos seguir aceptando conexiones.
        pthread_detach(thread_id);

    }
    close_server(globalInfo->server_worker);

    return NULL;
}

int main(int argc, char *argv[]) {
    
    signal(SIGINT, handle_sigint); // Administrar cierre de recursos

    if (argc != 2) {
        printF( "Uso: ./gotham <archivo_config>\n");
        return -1;
    }

    // Creamos struct GlobalInfoGotham para info general de gotham
    globalInfo = malloc(sizeof(GlobalInfoGotham));
    if (globalInfo == NULL) {
        perror("Error al asignar memoria para GlobalInfoGotham");
        return -1;
    }

    // Leer el archivo de configuración
    globalInfo->config = GOTHAM_read_config(argv[1]);
    if (globalInfo->config == NULL) {
        perror("Error al leer la configuración.\n");
        return -1;
    }

    // Mostrar configuración
    GOTHAM_show_config(globalInfo->config);


    /// Inicializamos toda la información general en GlobalInfo
    globalInfo->num_workers = 0;
    globalInfo->enigma_pworker_index = -1;    //Inicializamos en -1 indicando que no hay
    globalInfo->harley_pworker_index = -1;    //Inicializamos en -1 indicando que no hay 

    globalInfo->fleck_sockets = (int*)malloc(1 * sizeof(int));  //Inicializamos mem dinámica (para después poder hacer simplemente ralloc)
    globalInfo->num_flecks = 0;


    //Creamos thread para servidores Fleck y Worker

    /* SERVIDOR WORKER */
    pthread_t workers_server_thread;
    if (pthread_create(&workers_server_thread, NULL, workers_server, NULL) != 0) {
        perror("Error al crear el hilo del servidor Workers");
        exit(EXIT_FAILURE);
    }

    /* SERVIDOR FLECK */
    pthread_t fleck_server_thread;
    if (pthread_create(&fleck_server_thread, NULL, fleck_server, NULL) != 0) {
        perror("Error al crear el hilo del servidor Fleck");
        exit(EXIT_FAILURE);
    }



    ///SELECT (IGNORAR ESTA PARTE!!!) Posible mejora a futuro:
    //
    // 
    // Server server_worker;
    // int new_socket, valread, i;
    // fd_set current_sockets, ready_sockets; // Conjunto de descriptores para `select`
    // int client_sockets[MAX_CONNECTIONS]; // Almacena los sockets activos
    // char buffer[256];
    // // // Inicializar lista de clientes
    // // for (i = 0; i < FD_SETSIZE; i++) {
    // //     client_sockets[i] = 0;
    // // }

    // // Configurar servidor
    // server_worker = create_server(config->ip_workers, config->port_workers, 10);
    // start_server(&server_worker);


    // // Limpiar el conjunto de descriptores
    // FD_ZERO(&current_sockets);
    // // Agregar el socket del servidor al conjunto
    // FD_SET(server_worker.server_fd, &current_sockets);

    // printf("Servidor Gotham listo para aceptar conexiones de Workers.\n");
    // while (1) {
    //     ready_sockets = current_sockets;

    //     // // Agregar sockets de clientes al conjunto
    //     for (i = 0; i < MAX_CONNECTIONS; i++) {
    //         int sd = client_sockets[i];
    //         if (sd > 0) {
    //             FD_SET(sd, &current_sockets);
    //         }
    //         if (sd > max_fd) {
    //             max_fd = sd; 
    //         }
    //     }

    //     // Esperar actividad en uno de los sockets
    //     if (select(MAX_CONNECTIONS, &ready_sockets, NULL, NULL, NULL) < 0) {
    //         perror("Error en select");
    //         return -1;
    //     }

    //     // Verificar si hay una nueva conexión entrante
    //     if (FD_ISSET(server_worker.server_fd, &current_sockets)) {
    //         new_socket = accept_connection(&server_worker);
    //         if (new_socket < 0) {
    //             perror("Error al aceptar nueva conexión");
    //             continue;
    //         }

    //         printf("Nueva conexión aceptada: socket %d\n", new_socket);

    //         // Agregar el nuevo socket a la lista de clientes
    //         for (i = 0; i < MAX_CONNECTIONS; i++) {
    //             if (client_sockets[i] == 0) {
    //                 client_sockets[i] = new_socket;
    //                 printf("Agregado a la lista de sockets en la posición %d\n", i);
    //                 break;
    //             }
    //         }

    //         if (i == MAX_CONNECTIONS) {
    //             printf("Número máximo de conexiones alcanzado. Cerrando conexión con %d\n", new_socket);
    //             close(new_socket);
    //         }
    //     }

    //     // Verificar actividad en los sockets de clientes
    //     for (i = 0; i < MAX_CONNECTIONS; i++) {
    //         int sd = client_sockets[i];
    //         if (FD_ISSET(sd, &current_sockets)) {
    //             // Manejar el mensaje del cliente
    //             handle_worker_message(sd);

    //             // Si el cliente cerró la conexión, eliminarlo de la lista
    //             if (recv(sd, buffer, sizeof(buffer), MSG_PEEK | MSG_DONTWAIT) == 0) {
    //                 close(sd);
    //                 client_sockets[i] = 0;
    //                 printf("Socket %d desconectado y eliminado de la lista.\n", sd);
    //             }
    //         }
    //     }
    // }

    // close_server(&server_worker);
    ///SELECT

 
    pthread_join(fleck_server_thread, NULL);
    pthread_join(workers_server_thread, NULL);
    
    // Liberar la memoria dinámica asignada
    free(globalInfo->config->ip_fleck);
    free(globalInfo->config->ip_workers);
    free(globalInfo->config);

    return 0;
}
