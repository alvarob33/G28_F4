#define _GNU_SOURCE

#include "enigmalib.h"

#include <fcntl.h>
#include <unistd.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>


/***********************************************
*
* @Finalitat: Distorsionar un fitxer de text eliminant les paraules amb longitud menor a 'distort_factor'
*             i escriure el resultat a un fitxer de sortida.
* @Parametres:
*   in: input_path    = ruta al fitxer original.
*   in: output_path   = ruta on es crearà el fitxer distorsionat (es generará un nom basat en input_path).
*   in: distort_factor = longitud mínima de paraula a conservar (>=1).
* @Retorn: 0 en èxit, -1 en cas d’error.
*
************************************************/
int distort_file_text(const char* input_path, char* output_path, int distort_factor) {
    // Validación de parámetros
    if (!input_path || distort_factor < 1) {
        write(STDERR_FILENO, "Invalid parameters\n", 18);
        return -1;
    }

    // Abrir archivos
    int src_fd = open(input_path, O_RDONLY);
    if (src_fd == -1) {
        perror("open(input) failed");
        free(output_path);
        return -1;
    }

    int dst_fd = open(output_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (dst_fd == -1) {
        perror("open(output) failed");
        close(src_fd);
        free(output_path);
        return -1;
    }

    // Buffer de lectura
    char buffer[4096];
    ssize_t bytes_read;
    char* word = NULL;
    int word_len = 0;

    // Procesar archivo por bloques
    while ((bytes_read = read(src_fd, buffer, sizeof(buffer))) > 0) {
        // Recorrer cada caracter del buffer
        for (ssize_t i = 0; i < bytes_read; ++i) {
            if (isalpha(buffer[i])) {
                // Añadir caracter a la palabra actual
                char* new_word = realloc(word, word_len + 1);
                if (!new_word) {
                    perror("Memory allocation failed");
                    close(src_fd);
                    close(dst_fd);
                    free(word);
                    free(output_path);
                    return -1;
                }
                word = new_word;
                word[word_len++] = buffer[i];
            } else {
                // Procesar palabra completa
                if (word_len > 0) {
                    if (word_len >= distort_factor) {
                        if (write(dst_fd, word, word_len) != word_len) {
                            perror("write failed");
                            close(src_fd);
                            close(dst_fd);
                            free(word);
                            free(output_path);
                            return -1;
                        }
                    }
                    free(word);
                    word = NULL;
                    word_len = 0;
                }
                // Escribir caracter no alfabético
                if (write(dst_fd, &buffer[i], 1) != 1) {
                    perror("write failed");
                    close(src_fd);
                    close(dst_fd);
                    free(output_path);
                    return -1;
                }
            }
        }
    }

    // Procesar última palabra si existe
    if (word_len > 0) {
        if (word_len >= distort_factor) {
            write(dst_fd, word, word_len);
        }
        free(word);
    }

    // Manejo de errores de lectura
    if (bytes_read == -1) {
        perror("read failed");
        close(src_fd);
        close(dst_fd);
        free(output_path);
        return -1;
    }

    // Limpieza
    close(src_fd);
    close(dst_fd);

    return 0;
}
 