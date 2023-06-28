#ifndef FILES_H
#define FILES_H

#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>

#define PATH_MAX 4096

int mkdir_p(const char* pathname, mode_t mode);
const char* getFileName(const char* path);
int open_file(FILE** curr_file, char* filename);
int is_regular_file(const char* path);

#endif