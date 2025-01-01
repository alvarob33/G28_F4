#ifndef CONFIG_H
#define CONFIG_H

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <dirent.h>
//#include <sys/types.h>
//#include <sys/stat.h>
#include <signal.h>


#define printF(X) write(1, X, strlen(X))

#define BUFFER_SIZE 256
#define MEDIA "Media"
#define TEXT "Text"

// Importamos las variables de las extensiones para que sean accesibles por cualquier archivo
extern const char *const MEDIA_EXTENSIONS[];
extern const char *const TEXT_EXTENSIONS[];

// Lee una línea hasta el carácter indicado
char* read_until(int fd, char end);

void remove_ampersand(char *str);

int has_extension(const char *filename, const char *extension);

void list_files(const char *dir, const char *extension);

void eliminar_caracteres(char *str);

char* file_type(const char* filename);


#endif