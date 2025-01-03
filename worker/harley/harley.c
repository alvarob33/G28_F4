#define _GNU_SOURCE

#include "worker.h"
#include "harleylib.h"

Enigma_HarleyConfig* config = NULL;
int gotham_sock_fd = -1;
Server* server_flecks = NULL;

// Funcion administra cierre de proceso correctamente
void handle_sigint(/*int sig*/) {

    printF("\nCerrando programa de manera segura...\n");

    // Cerrar sockets
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
    
    // Salir del programa
    exit(0);
}


int main(int argc, char *argv[]) {
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

    if (isPrincipalWorker == 0) {
        // Esperar a que el thread termine (cuando nos asignen como Worker principal)
        //void* retval;
        if (pthread_join(thread_id, NULL) != 0) {
            perror("Error en pthread_join");
            exit(EXIT_FAILURE);
        }   
        printF("Principal Worker desconectado, ahora nosotros somos Principal.\n");
        // Actualizamos el valor de isPrincipalWorker
        isPrincipalWorker = 1;

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
    int socket_connection;

    server_flecks = create_server(config->ip_fleck, config->port_fleck, 10);
    start_server(server_flecks);


    //Bucle para leer cada conexion que nos llegue de un fleck
    while (1)
    {
        printF("Esperando conexiones de Flecks...\n");
        // Aceptar una nueva conexión
        socket_connection = accept_connection(server_flecks);

        // Crear un hilo para manejar la conexión con el cliente(Fleck)
        pthread_t thread_id;
        if (pthread_create(&thread_id, NULL, handle_fleck_connection, (void*)&socket_connection) != 0) {
            perror("Error al crear el hilo");
        }

        // No esperamos a que el hilo termine, porque queremos seguir aceptando conexiones.
        pthread_detach(thread_id);

    }
    close_server(server_flecks);


    // Liberar la memoria dinámica asignada
    free(config->ip_gotham);
    free(config->ip_fleck);
    free(config->worker_dir);
    free(config->worker_type);
    free(config);

    return 0;
}
