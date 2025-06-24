#define _GNU_SOURCE

#include <time.h>
#include <sys/socket.h>
#include <pthread.h>
#include <stdint.h>

#include "config.h"

// Definimos la lista de extensiones como una variable global
const char* const MEDIA_EXTENSIONS[] = {".png", ".jpg", ".jpeg", ".wav", ".mp3", NULL}; //.wav es audio
const char* const TEXT_EXTENSIONS[] = {".txt", ".md", ".log", ".csv", NULL};

/***********************************************
*
* @Finalitat: Llegir caràcters des de l’entrada fins a trobar el caràcter `end` o EOF, i retornar una cadena dinàmica amb el contingut llegit.
* @Parametres:
*   in:  fd    = descriptor del fitxer d’on llegir.
*   in:  end   = caràcter delimitador (no s’inclou a la sortida).
*   out: string = punter a la cadena dinàmica amb les dades llegides.
* @Retorn:
*   Punter a una cadena acabada en ‘\0’ amb el contingut llegit,
*   o NULL en cas d’error o si no s’ha llegit cap caràcter abans de EOF.
*
************************************************/
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


/***********************************************
*
* @Finalitat: Eliminar tots els caràcters ‘&’ de la cadena indicada, compactant els altres caràcters.
* @Parametres: in/out: str = cadena a modificar.
* @Retorn: --- (modifica directament `str`).
*
************************************************/
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


/***********************************************
*
* @Finalitat: Comprovar si un fitxer té una extensió concreta.
* @Parametres:
*   in: filename  = nom del fitxer.
*   in: extension = extensió buscada (inclosos el punt).
* @Retorn: 1 si `filename` acaba amb `extension`, 0 altrament.
*
************************************************/
int has_extension(const char *filename, const char *extension) {
    const char *punto = strrchr(filename, '.');
    return (punto && strcmp(punto, extension) == 0);
}

/***********************************************
*
* @Finalitat: Llistar per stdout tots els fitxers dins de `dir` que tinguin l’extensió especificada.
* @Parametres:
*   in: dir       = directori on cercar (sense barres al final).
*   in: extension = extensió dels fitxers a llistar.
* @Retorn: --- 
*
************************************************/
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


/***********************************************
*
* @Finalitat: Convertir tots els caràcters de la cadena a minúscula.
* @Parametres:
*   in/out: str = cadena a convertir in-place.
* @Retorn: .-- (modifica directament `str`).
*
************************************************/
void to_lowercase(char *str) {
    for (int i = 0; str[i]; i++) {
        str[i] = tolower((unsigned char)str[i]);
    }
}

/***********************************************
*
* @Finalitat: Determinar si un fitxer és de tipus Text o Media a partir de la seva extensió.
* @Parametres:
*   in: filename = nom del fitxer a analitzar.
* @Retorn:
*   Punter a la cadena "Media" si és extensió de media,
*   "Text" si és extensió de text, o NULL si no és cap de les dues.
*
************************************************/
char* file_type(const char* filename) {
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


/***********************************************
*
* @Finalitat: Classificar un fitxer com Image o Audio segons la seva extensió especíﬁca.
* @Parametres:
*   in: filename = nom del fitxer a analitzar.
* @Retorn:
*   "Image" si l’extensió és d’imatge,
*   "Audio" si és d’àudio, o NULL en altres casos.
*
************************************************/
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

/***********************************************
*
* @Finalitat: Eliminar caràcters de nova línia i retorn de carro al final de la cadena, si existeixen.
* @Parametres:
*   in/out: str = cadena a netejar in-place.
* @Retorn: --- (modifica directament `str`).
*
************************************************/
void eliminar_caracteres(char *str) {
    size_t len = strlen(str);
    if (len > 0 && (str[len - 1] == '\n' || str[len - 1] == '\r')) {
        str[len - 1] = '\0';
    }
    if (len > 1 && (str[len - 2] == '\r')) {
        str[len - 2] = '\0';
    }
}

