#ifndef CONNECTION_MANAGET_H
#define CONNECTION_MANAGET_H

#include <sys/stat.h>
#include <sys/types.h>

#define PATH_MAX 4096

int
mkdir_p (const char *pathname, 
         mode_t      mode);

#endif