#define _GNU_SOURCE
#include "../config/config.h"
#include "flecklib.h"
#include "../config/connections.h"

#include <stdio.h>
#include <ctype.h>
#include <dirent.h>
#include <signal.h>


int main(int argc, char *argv[]) {
    if (argc != 2) {
        printF("Uso: ./fleck <archivo_config>\n");
        return -1;
    }

    signal(SIGINT, FLECK_signal_handler);

    // Leer el archivo de configuración
    FleckConfig* config = FLECK_read_config(argv[1]);
    if (!config) {
        perror("Error al asignar memoria para config");
        return -1;
    }

    // Mostrar la configuración leída
    printF("\nFleck:\n");
    char *output;

    asprintf(&output, "Nombre de usuario: %s\n", config->username);
    printF(output);
    free(output);

    asprintf(&output, "Directorio de usuario: %s\n", config->user_dir);
    printF(output);
    free(output);

    asprintf(&output, "Dirección IP de Gotham: %s\n", config->gotham_ip);
    printF(output);
    free(output);

    asprintf(&output, "Puerto de Gotham: %d\n", config->gotham_port);
    printF(output);
    free(output);

    // Ejecutar el menú de opciones
    FLECK_handle_menu(config);

    // Conectar con Gotham
    if (FLECK_connect_to_gotham(config) != 0) {
        printF("Error al conectar Fleck con Gotham.\n");
        free(config->username);
        free(config->user_dir);
        free(config->gotham_ip);
        free(config);
        return -1;
    }

    // Liberar la memoria dinámica asignada
    free(config->username);
    free(config->user_dir);
    free(config->gotham_ip);
    free(config);

    return 0;
}
