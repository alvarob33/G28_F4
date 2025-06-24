#define _GNU_SOURCE

#include <time.h>
#include <sys/socket.h>
#include <pthread.h>
#include <stdint.h>

#include "config.h"

// Definimos la lista de extensiones como una variable global
const char* const MEDIA_EXTENSIONS[] = {".png", ".jpg", ".jpeg", ".wav", ".mp3", NULL}; //.wav es audio
const char* const TEXT_EXTENSIONS[] = {".txt", ".md", ".log", ".csv", NULL};

// // Función para leer una línea hasta el carácter indicado

char* read_until(int fd, char end) {
    int i = 0, size;
    char c = '\0';
    char *string = (char *)malloc(1);

    if (string == NULL) {
        perror("Error en malloc");
        return NULL;
    }

    while (1) {
        size = read(fd, &c, sizeof(char));

        if (size == -1) {
            perror("Error al leer el archivo");
            free(string);
            return NULL;
        } else if (size == 0) { // EOF
            if (i == 0) {
                free(string);
                return NULL;
            }
            break;
        }

        if (c == end) {
            break;
        }

        char *tmp = realloc(string, i + 2);
        if (tmp == NULL) {
            perror("Error en realloc");
            free(string);
            return NULL;
        }

        string = tmp;
        string[i++] = c;
    }

    string[i] = '\0';

    // Limpiar \r y \n finales si existen
    while (i > 0 && (string[i - 1] == '\r' || string[i - 1] == '\n')) {
        string[--i] = '\0';
    }

    return string;
}


// Función para eliminar los carácteres '&' de una cadena
void remove_ampersand(char *str) {
    char *src = str, *dst = str;

    // Recorre la cadena y copia los caracteres que no sean '&'
    while (*src) {
        if (*src != '&') {
            *dst++ = *src;
        }
        src++;
    }

    *dst = '\0';  // Asegurar que es el final de la cadena
}

// Función para verificar si un archivo tiene la extensión especificada
int has_extension(const char *filename, const char *extension) {
    const char *punto = strrchr(filename, '.');
    return (punto && strcmp(punto, extension) == 0);
}

// Función para listar archivos en el directorio con una extensión específica
void list_files(const char *dir, const char *extension) {
    struct dirent *entry;
    DIR *dp;

    // Construir la ruta completa, "dir" es el directorio del usuario
    char *path;
    asprintf(&path, "users%s", dir);  // Crear ruta con el prefijo "users"

    dp = opendir(path);  // Abrir el directorio

    if (dp == NULL) {
        perror("Error abriendo el directorio");
        free(path);
        return;
    }

    while ((entry = readdir(dp)) != NULL) {
        // Ignorar entradas especiales "." y ".."
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        // Verificar si el archivo tiene la extensión correcta
        if (has_extension(entry->d_name, extension)) {
            printF(entry->d_name);
            printF("\n");
        }
    }
    closedir(dp);
    free(path);
}



// Función auxiliar para convertir una cadena a minúsculas
void to_lowercase(char *str) {
    for (int i = 0; str[i]; i++) {
        str[i] = tolower((unsigned char)str[i]);
    }
}

// Función para comprobar el tipo de archivo
// Devuelve "Media" o "Text" dependiendo de la extensión
char* file_type(const char* filename) {
    // Lista de extensiones de media
    // const char* media_extensions[] = {".png", ".jpg", ".jpeg", ".bmp", ".tga", ".wav"}; //.wav es audio
    // // Lista de extensiones de texto
    // const char* text_extensions[] = {".txt", ".md", ".log", ".csv"};

    // Buscar la última ocurrencia de '.' en el nombre del archivo
    const char* extension = strrchr(filename, '.');
    if (!extension) return NULL; // No tiene extensión

    // Comprobar si es una extensión de media
    for (int i = 0; MEDIA_EXTENSIONS[i] != NULL; i++) {  // Revisar hasta el NULL
        if (strcmp(extension, MEDIA_EXTENSIONS[i]) == 0) {
            return MEDIA;
        }
    }
    // Comprobar si es una extensión de texto
    for (int i = 0; TEXT_EXTENSIONS[i] != NULL; i++) {  // Revisar hasta el NULL
        if (strcmp(extension, TEXT_EXTENSIONS[i]) == 0) {
            return TEXT;
        }
    }

    // No coincide con ninguna extensión conocida
    return NULL;
}

char* wich_media(const char *filename) {
    const char *image_ext[] = { ".jpg", ".jpeg", ".png", NULL};
    const char *audio_ext[] = { ".mp3", ".wav", NULL};

    // Obtener puntero a la última ocurrencia de '.'
    const char *dot = strrchr(filename, '.');
    if (!dot || dot == filename) return NULL;  // No tiene extensión

    char ext[16];
    strncpy(ext, dot, sizeof(ext));
    ext[sizeof(ext) - 1] = '\0';
    to_lowercase(ext); // Convertimos la extensión a minúsculas

    for (int i = 0; image_ext[i] != NULL; i++) {
        if (strcmp(ext, image_ext[i]) == 0) return IMAGE;
    }
    for (int i = 0; audio_ext[i] != NULL; i++) {
        if (strcmp(ext, audio_ext[i]) == 0) return AUDIO;
    }

    return NULL;
}

// Función para eliminar caracteres de nueva línea y retorno de carro
void eliminar_caracteres(char *str) {
    size_t len = strlen(str);
    if (len > 0 && (str[len - 1] == '\n' || str[len - 1] == '\r')) {
        str[len - 1] = '\0';
    }
    if (len > 1 && (str[len - 2] == '\r')) {
        str[len - 2] = '\0';
    }
}

