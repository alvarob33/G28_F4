#define _GNU_SOURCE

#include "enigmalib.h"

// Función para manejar la conexión del cliente 
void* handle_fleck_connection(void* client_socket) {
    int socket_connection = *(int*)client_socket;
    char buffer[BUFFER_SIZE];
    int bytes_read = read(socket_connection, buffer, sizeof(buffer) - 1);

    if (bytes_read > 0) {
        buffer[bytes_read] = '\0';

        // Validar tipo de conexión
        if (buffer[0] == 0x01) {
            // Responder con OK
            char response[3] = {0x01, 0x00, 0x00};
            send(socket_connection, response, sizeof(response), 0);
            printf("Conexión de Fleck aceptada.\n");
        } else {
            // Responder con KO
            char response[] = {0x01, 0x00, 0x07, 'C', 'O', 'N', '_', 'K', 'O'};
            send(socket_connection, response, sizeof(response), 0);
            printf("Conexión de Fleck rechazada.\n");
        }

        // Mantener la conexión mientras Fleck esté activo
        while ((bytes_read = read(socket_connection, buffer, BUFFER_SIZE)) > 0) {
            buffer[bytes_read] = '\0';
            printf("Mensaje recibido de Fleck: %s\n", buffer);
        }

        printf("Fleck desconectado.\n");
    } else {
        perror("Error al leer datos del Fleck");
    }

    close(socket_connection);
    return NULL;
}

