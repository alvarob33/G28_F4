#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include "../config/connections.h"
#include "../config/config.h"

// Helper: recorta '\n' final de ctime()
static void chop_newline(char *s) {
    char *p;
    if ((p = strchr(s, '\n'))) *p = '\0';
}

int main() {
    unsigned char buffer[BUFFER_SIZE];
    ssize_t n;
    
    while ((n = read(STDIN_FILENO, buffer, BUFFER_SIZE)) > 0) {
        TramaResult *tr = leer_trama(buffer);
        if (!tr) continue;
        printF("LOL\n");
        // Preparar timestamp (quita el '\n' de ctime)
        char ts_copy[64];
        strncpy(ts_copy, tr->timestamp, sizeof(ts_copy)-1);
        ts_copy[sizeof(ts_copy)-1] = '\0';
        chop_newline(ts_copy);

        // Abrir logs.txt en modo append
        int fd = open("logs.txt", O_CREAT|O_APPEND|O_WRONLY, 0644);
        if (fd >= 0) {
            printF("SIIII\n");
            dprintf(fd, "[%s] %s\n", ts_copy, tr->data);
            close(fd);
        }
        free_tramaResult(tr);
    }
    return 0;
}