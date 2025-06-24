#define _GNU_SOURCE

#include "../worker/worker.h"
#include "../worker/worker_distort.h"
#include "harleylib.h"

// VARIABLES GLOBALES
Enigma_HarleyConfig* config = NULL;
int gotham_sock_fd = -1;
Server* server_flecks = NULL;

ClientThread* threads = NULL;           // Threads generados por cada conexión Fleck
// pthread_t* subthreads = NULL;      // Threads generados por cada conexión Fleck
int num_threads = 0;
int server_running = 0;

/***********************************************
*
* @Finalitat: Gestionar la senyal SIGINT per tancar correctament el worker Harley: desconnectar
*             de Gotham, tancar servidor i fils.
* @Parametres: ---
* @Retorn: ----
*
************************************************/
void handle_sigint(/*int sig*/) {

    printF("\nCerrando programa de manera segura...\n");

    // Cerrar conexión con Gotham
    WORKER_disconnect_from_gotham(gotham_sock_fd, config);

    // Liberar la memoria dinámica asignada
    free(config->ip_gotham);
    free(config->ip_fleck);
    free(config->worker_dir);
    free(config->worker_type);
    free(config);


    // Cerrar servidor
    if (server_flecks != NULL)
    {
        close_server(server_flecks);
    }

    // CERRAR CONEXIONES FLECKS

    // CERRAR THREADS
    WORKER_cancel_and_wait_threads(threads, num_threads);
    
    // Salir del programa
    signal(SIGINT, SIG_DFL);
    raise(SIGINT);
}


int main(int argc, char *argv[]) {
    signal(SIGINT, handle_sigint);
    if (argc != 2) {
        printF("Uso: ./harley <archivo_config>\n");
        return -1;
    }

    // Leer el archivo de configuración
    config = WORKER_read_config(argv[1]);
    if (config == NULL) {
        printF("Error al leer la configuración.\n");
        return -1;
    }

    printF("\nWorker Config Harley:\n");
    WORKER_print_config(config);

    // Conectar con Gotham
    int isPrincipalWorker = 0;     // Puntero entero que nos indica si somos el worker principal o no
    gotham_sock_fd = WORKER_connect_to_gotham(config, &isPrincipalWorker);
    if (gotham_sock_fd < 0) {
        printF("Error al conectar Enigma con Gotham.\n");
        // Liberar la memoria antes de salir
        free(config->ip_gotham);
        free(config->ip_fleck);
        free(config->worker_dir);
        free(config->worker_type);
        free(config);
        return -1;
    }

    // Creamos thread para responder Heartbeats o asignación_principal_worker de Gotham
    pthread_t thread_id;
    if (pthread_create(&thread_id, NULL, responder_gotham, (void *)&gotham_sock_fd) != 0) {
        perror("Error creando el hilo para heartbeat");
        return -1;
    }

    // Si no somos PrincipalWorker
    if (isPrincipalWorker == 0) {
        // Esperar a que el thread termine (cuando nos asignen como Worker principal)
        if (pthread_join(thread_id, NULL) != 0) {
            perror("Error en pthread_join");
            exit(EXIT_FAILURE);
        }   
        // Actualizamos el valor de isPrincipalWorker
        isPrincipalWorker = 1;

        printF("Principal Worker desconectado, ahora nosotros somos Principal.\n");
        // Volver a crear thread para responder HEARTBEATs
        if (pthread_create(&thread_id, NULL, responder_gotham, (void *)&gotham_sock_fd) != 0) {
            perror("Error creando el hilo para heartbeat");
            return -1;
        }
        
    }

    // Desvincular el thread para que no necesite ser unido posteriormente y se use para HEARTBEATs
    if (pthread_detach(thread_id) != 0) {
        perror("Error desvinculando el hilo");
        return -1;
    }



    /* SERVIDOR WORKER-FLECKS */
    // Crear nuevo servidor para conexiones con Fleck
    
    // Configurar servidor
    server_flecks = create_server(config->ip_fleck, config->port_fleck, 10);
    start_server(server_flecks);

    // Inicializamos subthreads
    // subthreads = malloc(num_threads * sizeof(pthread_t));    //Inicializamos mem dinámica (para después poder hacer simplemente realloc)
    // if (subthreads == NULL) {
    //     perror("Error al asignar memoria para los hilos");
    //     handle_sigint();
    // }

    //Bucle para leer cada conexion que nos llegue de un fleck
    int socket_connection;
    server_running = 1;
    while (server_running)
    {
        printF("Esperando conexiones de Flecks...\n");
        socket_connection = accept_connection(server_flecks);

        ClientThread* new_threads = realloc(threads, (num_threads + 1) * sizeof(ClientThread));
        if (!new_threads) {
            close(socket_connection);
            continue;
        }

        threads = new_threads;
        threads[num_threads].socket = socket_connection;
        threads[num_threads].active = 1;

        // Crear un hilo para manejar la conexión con el cliente(Fleck)
        if (pthread_create(&threads[num_threads].thread_id, NULL, handle_fleck_connection, &threads[num_threads]) != 0) {
            close(threads[num_threads].socket);
            perror("Error al crear el hilo");
            continue;
        }

        num_threads++;

    }
    close_server(server_flecks);



    return 0;
}
