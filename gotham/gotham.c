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
    
    // ARKHAM
    // log_event(globalInfo, "Cerrando programa con SIGINT)");

    // Cerrar pipe para que Arkham termine
    close(globalInfo->log_fd);
    globalInfo->log_fd = -1;

    // Esperar a que Arkham termine
    if (globalInfo->arkham_pid > 0) {
        waitpid(globalInfo->arkham_pid, NULL, 0);
    }
    

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
    log_event(globalInfo, "Servidor para Workers iniciado");
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
    log_event(globalInfo, "Servidor para Flecks iniciado.");
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


/**
 * Crea el proceso Arkham y establece la comunicación por pipe
 * @param globalInfo Estructura con la información global del sistema
 * @return 0 en éxito, -1 en error
 */
int create_arkham_process(GlobalInfoGotham* globalInfo) {
    int pipefd[2];
    
    // Crear pipe
    if (pipe(pipefd) < 0) {
        perror("Error al crear pipe para Arkham");
        return -1;
    }

    // Crear proceso hijo
    pid_t pid = fork();
    if (pid < 0) {
        perror("Error al crear proceso Arkham");
        close(pipefd[0]);
        close(pipefd[1]);
        return -1;
    }

    if (pid == 0) { // Proceso hijo (Arkham)
        close(pipefd[1]); // Cerrar extremo de escritura
        
        // Redirigir stdin al pipe
        if (dup2(pipefd[0], STDIN_FILENO) == -1) {
            perror("Error redirigiendo stdin en Arkham");
            exit(EXIT_FAILURE);
        }
        close(pipefd[0]);

        // Ejecutar Arkham
        execl("./arkham.exe", "./arkham.exe", NULL);
        
        // Si llegamos aquí, hubo un error
        perror("Error al ejecutar Arkham");
        exit(EXIT_FAILURE);

    } else { // Proceso padre (Gotham)
        close(pipefd[0]); // Cerrar extremo de lectura
        globalInfo->log_fd = pipefd[1];
        globalInfo->arkham_pid = pid;
        
        log_event(globalInfo, "Sistema Gotham iniciado");
        return 0;
    }
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

    // Crear proceso Arkham
    if (create_arkham_process(globalInfo) == -1) {
        free(globalInfo->config);
        free(globalInfo);
        return -1;
    }

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


 
    pthread_join(globalInfo->fleck_server_thread, NULL);
    pthread_join(globalInfo->workers_server_thread, NULL);

    return 0;
}
