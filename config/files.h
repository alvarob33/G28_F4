#ifndef FILES_H
#define FILES_H


#define _GNU_SOURCE

#include <sys/types.h>
#include <sys/wait.h>   // waitpid

#include "../config/config.h"
#include "../config/connections.h"
//#include "../gotham/gothamlib.h"
//#include "flecklib.h"



char* get_string_file_size(const char* filename);
char* calculate_md5sum(const char* filename);



#endif