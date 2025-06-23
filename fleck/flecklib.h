#ifndef FLECKLIB_H
#define FLECKLIB_H

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>

#include "structures.h"

FleckConfig* FLECK_read_config(const char *config_file);

void FLECK_handle_menu(FleckConfig *config);

int FLECK_connect_to_gotham(FleckConfig *config);

void FLECK_signal_handler();

#endif