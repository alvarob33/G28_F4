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

    printF("\n\nCerrando programa de manera segura...\n");
    log_event(globalInfo, "Shutdown received (SIGINT)");

    // CONFIG
    free(globalInfo->config->ip_fleck);
    free(globalInfo->config->ip_workers);
    free(globalInfo->config);


    /* WORKER */

    // Enviar mensajes de desconexion a Workers
    //

    printF("Liberando memoria workers...\n");
    close_server(globalInfo->server_worker);

    pthread_mutex_lock(&globalInfo->worker_mutex);
    liberar_memoria_workers(globalInfo);
    pthread_mutex_unlock(&globalInfo->worker_mutex);

    pthread_mutex_destroy(&globalInfo->worker_mutex);   // Destruir el mutex
    printF("Memoria de los Workers liberada correctamente.\n\n");


    /* FLECK */

    // Enviar mensajes de desconexion a Flecks
    //
    
    printF("Liberando memoria flecks...\n");
    close_server(globalInfo->server_fleck);

    pthread_mutex_lock(&globalInfo->fleck_mutex);
    liberar_memoria_flecks(globalInfo);
    pthread_mutex_unlock(&globalInfo->fleck_mutex);

    pthread_mutex_destroy(&globalInfo->fleck_mutex);    // Destruir el mutex
    printF("Memoria de los Flecks liberada correctamente.\n\n");


    // THREADS
    cancel_and_wait_threads(globalInfo);
    free(globalInfo);


    printF("Recursos liberados correctamente. Saliendo...\n");
    signal(SIGINT, SIG_DFL);
    raise(SIGINT);
}

// Funcion para crear Servidor de Workers y administrar las conexiones entrantes
void* workers_server(/*void* arg*/) {

    // Crear y configurar servidor
    globalInfo->server_worker = create_server(globalInfo->config->ip_workers, globalInfo->config->port_workers, 10);
    start_server(globalInfo->server_worker);


    printF("Esperando conexiones de Workers...\n");
    //Bucle para leer cada conexion que nos llegue de un worker
    while (1)
    {
        // Creamos struct con argumentos para pasarle al thread
        ThreadArgsGotham* args = malloc(sizeof(ThreadArgsGotham));
        if (args == NULL) {
            perror("Error al asignar memoria para ThreadArgsGotham");
            continue;
        }

        args->socket_connection = accept_connection(globalInfo->server_worker);  // Aceptamos conexion
        if (args->socket_connection >= 0)
        {
            args->global_info = globalInfo;

            // Crear un hilo para manejar la conexión con el cliente(Worker)
            pthread_t thread_id;
            if (pthread_create(&thread_id, NULL, handle_worker_connection, (void*)args) != 0) {
                perror("Error al crear el hilo");
            }

            // Guardar thread del fleck conectado
            pthread_mutex_lock(&globalInfo->subthreads_mutex);
            pthread_t* temp = (pthread_t*)realloc(globalInfo->subthreads, (globalInfo->num_subthreads +1) * sizeof(pthread_t));
            if (temp == NULL) {
                pthread_mutex_unlock(&globalInfo->subthreads_mutex);
                free(args);
                perror("Error al redimensionar el array de subthreads");
                continue;
            }
            globalInfo->subthreads = temp;
            globalInfo->subthreads[globalInfo->num_subthreads] = thread_id;
            globalInfo->num_subthreads++;
            pthread_mutex_unlock(&globalInfo->subthreads_mutex);

            // No esperamos a que el hilo termine, porque queremos seguir aceptando conexiones.
            pthread_detach(thread_id);
        }
        
    }
    //close_server(globalInfo->server_worker);

    return NULL;
}

// Funcion para crear Servidor de Flecks y administrar las conexiones entrantes
void* fleck_server(/*void* arg*/) {

    // Crear y configurar servidor
    globalInfo->server_fleck = create_server(globalInfo->config->ip_fleck, globalInfo->config->port_fleck, 10);
    start_server(globalInfo->server_fleck);


    printF("Esperando conexiones de Flecks...\n");
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
        if (args->socket_connection >= 0) 
        {
            args->global_info = globalInfo;

            // Guardar socket del fleck conectado
            pthread_mutex_lock(&globalInfo->fleck_mutex);
            int* temp = (int*)realloc(globalInfo->fleck_sockets, (globalInfo->num_flecks +1) * sizeof(int));
            if (temp == NULL) {
                pthread_mutex_unlock(&globalInfo->fleck_mutex);
                free(args);
                perror("Error al redimensionar fleck_sockets");
                continue;
            }
            globalInfo->fleck_sockets = temp;
            globalInfo->fleck_sockets[globalInfo->num_flecks] = args->socket_connection;
            globalInfo->num_flecks++;
            pthread_mutex_unlock(&globalInfo->fleck_mutex);
            

            // Crear un thread para manejar la conexión con el cliente
            pthread_t thread_id;
            if (pthread_create(&thread_id, NULL, handle_fleck_connection, (void*)args) != 0) {
                free(args);
                perror("Error al crear el hilo");
                continue;
            }

            // Guardar thread del fleck conectado
            pthread_mutex_lock(&globalInfo->subthreads_mutex);
            pthread_t* temp2 = (pthread_t*)realloc(globalInfo->subthreads, (globalInfo->num_subthreads +1) * sizeof(pthread_t));
            if (temp2 == NULL) {
                pthread_mutex_unlock(&globalInfo->subthreads_mutex);
                free(args);
                perror("Error al redimensionar el array de subthreads");
                continue;
            }
            globalInfo->subthreads = temp2;
            globalInfo->subthreads[globalInfo->num_subthreads] = thread_id;
            globalInfo->num_subthreads++;
            pthread_mutex_unlock(&globalInfo->subthreads_mutex);

            // No esperamos a que el hilo termine, porque queremos seguir aceptando conexiones.
            pthread_detach(thread_id);

        } else {
            printF("Error accepting connection.\n");
            free(args);
        }

    }
    //close_server(globalInfo->server_fleck);

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

     // Crear pipe para comunicación con Arkham
    int pipefd[2];
    if (pipe(pipefd) < 0) {
        perror("Error creando pipe para Arkham");
        exit(EXIT_FAILURE);
    }
    pid_t pid = fork();
    if (pid < 0) {
        perror("Error al forkar Arkham");
        exit(EXIT_FAILURE);
    }
    if (pid == 0) {
        // Proceso Arkham
        close(pipefd[1]);  // cerramos escritura
        // Reemplazar stdin por el extremo de lectura (fd = pipefd[0])
        dup2(pipefd[0], STDIN_FILENO);
        close(pipefd[0]);
        execl("./arkham.exe", "arkham.exe", NULL);
        perror("Error al ejecutar Arkham");
        exit(EXIT_FAILURE);
    }
    // Padre (Gotham)
    close(pipefd[0]);      // cerramos lectura
    globalInfo->log_fd = pipefd[1];

    // Mostrar configuración
    GOTHAM_show_config(globalInfo->config);


    /// Inicializamos toda la información general en GlobalInfo
    globalInfo->workers = 0;
    globalInfo->num_workers = 0;
    globalInfo->enigma_pworker_index = -1;    //Inicializamos en -1 indicando que no hay
    globalInfo->harley_pworker_index = -1;    //Inicializamos en -1 indicando que no hay 
    pthread_mutex_init(&globalInfo->worker_mutex, NULL);

    globalInfo->fleck_sockets = (int*)malloc(1 * sizeof(int));  //Inicializamos mem dinámica (para después poder hacer simplemente realloc)
    globalInfo->num_flecks = 0;
    pthread_mutex_init(&globalInfo->fleck_mutex, NULL);

    globalInfo->subthreads = malloc(globalInfo->num_subthreads * sizeof(pthread_t));    //Inicializamos mem dinámica (para después poder hacer simplemente realloc)
    if (globalInfo->subthreads == NULL) {
        perror("Error al asignar memoria para los hilos");
        exit(EXIT_FAILURE);
    }
    globalInfo->num_subthreads = 0;
    pthread_mutex_init(&globalInfo->subthreads_mutex, NULL);

    //Creamos threads para servidores Fleck y Worker

    /* SERVIDOR WORKER */
    if (pthread_create(&globalInfo->workers_server_thread, NULL, workers_server, NULL) != 0) {
        perror("Error al crear el hilo del servidor Workers");
        handle_sigint();
    }

    /* SERVIDOR FLECK */
    if (pthread_create(&globalInfo->fleck_server_thread, NULL, fleck_server, NULL) != 0) {
        perror("Error al crear el hilo del servidor Fleck");
        handle_sigint();
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

 
    pthread_join(globalInfo->fleck_server_thread, NULL);
    pthread_join(globalInfo->workers_server_thread, NULL);

    return 0;
}
