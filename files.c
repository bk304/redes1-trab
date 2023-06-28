#include "files.h"

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

/*
 * Copyright 2011 Matthew Brush <mbrush(at)codebrainz(dot)ca>
 * Feel free to use this code however you want to.
 */
int mkdir_p(const char* pathname,
            mode_t mode) {
    char* tok = NULL;
    char path[PATH_MAX + 1] = {0};
    char tmp[PATH_MAX + 1] = {0};
    struct stat st = {0};

    /* invalid pathname */
    if (!pathname || !pathname[0])
        return -1;

    /* pathname already exists and is a directory */
    if (stat(pathname, &st) == 0 && S_ISDIR(st.st_mode))
        return 0;

    /* doesn't need parent directories created */
    if (mkdir(pathname, mode) == 0)
        return 0;

    /* prepend initial / if needed */
    if (pathname[0] == '/')
        tmp[0] = '/';

    /* make a copy of pathname and start tokenizing it */
    strncpy(path, pathname, PATH_MAX);
    tok = strtok(path, "/");

    /* keep going until there are no tokens left */
    while (tok) {
        /* append the next token to the path */
        strcat(tmp, tok);

        /* create the directory and keep going unless mkdir fails and
         * errno doesn't indicate that the path already exists */
        errno = 0;
        if (mkdir(tmp, mode) != 0 && errno != EEXIST)
            return -1; /* errno still set from mkdir() call */

        /* append a / to the path for the next token and get it */
        strcat(tmp, "/");
        tok = strtok(NULL, "/");
    }

    /* success */
    return 0;
}

const char* getFileName(const char* path) {
    const char* fileName = strrchr(path, '/');
    if (fileName == NULL) {
        fileName = strrchr(path, '\\');
    }
    if (fileName == NULL) {
        fileName = path;
    } else {
        fileName++;  // Pular o caractere de separação de diretório
    }
    return fileName;
}

int open_file(FILE** curr_file, char* filename) {
    struct stat about_file;

    if (*curr_file != NULL) {
        fclose(*curr_file);
    }

    *curr_file = fopen(filename, "r");
    if (*curr_file == NULL)
        return errno;

    if (fstat(fileno(*curr_file), &about_file))
        return errno;

    return 0;
}

int is_regular_file(const char* path) {
    struct stat path_stat;
    stat(path, &path_stat);
    return S_ISREG(path_stat.st_mode);
}