
#include "files.h"





// Devuelve string del tamaño del archivo indicado
char* get_string_file_size(const char* filename) {
    int fd = open(filename, O_RDONLY);
    if (fd == -1) {
        perror("No se pudo abrir el archivo");
        return NULL;
    }

    // Obtener el tamaño del archivo usando lseek
    long size = lseek(fd, 0, SEEK_END);  // Mover al final del archivo y obtener la posición
    if (size < 0) {
        perror("Error al obtener el tamaño del archivo");
        close(fd);  // Cerrar el archivo si ocurre un error
        return NULL;
    }

    // Cerrar  archivo
    close(fd);

    char* size_str;
    if (asprintf(&size_str, "%ld", size) < 0) {
        perror("Error al asignar memoria para el tamaño del archivo");
        return NULL;
    }

    return size_str;
}



char* calculate_md5sum(const char* filename) {
    if (filename == NULL) {
        fprintf(stderr, "El nombre del archivo no puede ser NULL.\n");
        return NULL;
    }

    // Creamos pipes (para comunicación)
    int pipe_fd[2];
    if (pipe(pipe_fd) < 0) {
        perror("Error al crear el pipe");
        return NULL;
    }

    // Creamos fork para que proceso hijo ejecute el comando y nosotros leamos el resultado
    pid_t pid = fork();
    if (pid < 0) {
        perror("Error al hacer fork");
        close(pipe_fd[0]);
        close(pipe_fd[1]);
        return NULL;
    }

    if (pid == 0) { // Proceso hijo
        // Redirigir la salida STDOUT(por pantalla) al extremo de escritura del pipe
        dup2(pipe_fd[1], STDOUT_FILENO);
        close(pipe_fd[0]); // Cerrar el extremo de lectura
        close(pipe_fd[1]);

        // Ejecutar el comando `md5sum` para calcularlo sobre el archivo indicado
        execlp("md5sum", "md5sum", filename, (char*)NULL);

        // Si execlp falla (el proceso se debería sustituir por md5sum, por lo que no debería pasar por aquí)
        perror("Error al ejecutar md5sum");
        exit(EXIT_FAILURE);
    }

    // Proceso padre
    close(pipe_fd[1]); // Cerrar el extremo de escritura

    // Esperar a que el proceso hijo termine antes de leer del pipe
    int status;
    waitpid(pid, &status, 0); // Esperamos que el proceso hijo termine
    if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
        fprintf(stderr, "El comando md5sum falló.\n");
        close(pipe_fd[0]);
        return NULL;
    }

    // Leer la salida del pipe
    char md5_output[64] = {0};
    ssize_t bytes_read = read(pipe_fd[0], md5_output, sizeof(md5_output) - 1);
    close(pipe_fd[0]); // Cerrar el extremo de lectura

    if (bytes_read <= 0) {
        perror("Error al leer la salida de md5sum");
        return NULL;
    }

    // Extraer solo el MD5 (los primeros 32 caracteres porque los siguientes dan otra información)
    char* md5sum = (char*)malloc(33);
    if (md5sum == NULL) {
        perror("Error al asignar memoria para el MD5 sum");
        return NULL;
    }
    strncpy(md5sum, md5_output, 32);
    md5sum[32] = '\0'; // Asegurarse de que la cadena termine en '\0'

    return md5sum;
}