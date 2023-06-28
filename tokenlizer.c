#include "tokenlizer.h"

#include <glob.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum token_part { NONE,
                  IN_WORD,
                  IN_STRING };

char *strdup(const char *s) {
    size_t slen = strlen(s);
    char *result = malloc(slen + 1);
    if (result == NULL) {
        return NULL;
    }

    memcpy(result, s, slen + 1);
    return result;
}

char *insert(char *word, char c) {
    return strncat(word, &c, sizeof(char));
}

char *newWord(int length) {
    char *word = (char *)calloc(length, sizeof(char));
    if (!word) {
        return NULL;
    } else {
        word[0] = '\0';
    }

    return word;
}

int hasStar(char *str) {
    int i = 0;
    for (char c = str[0]; c != '\0'; c = str[++i]) {
        if (c == '*')
            return 1;
    }

    return 0;
}

void resetWord(char *word) {
    memset(word, 0, ARG_MAX_SIZE);
    word[0] = '\0';
}

char isSpace(unsigned char c) {
    return c == ' ';
}

int includeArg(char *word, int *argc, char *argv[]) {
    char *newArgument;
    glob_t result;
    int status;

    if (!hasStar(word)) {
        newArgument = strdup(word);
        if (newArgument == NULL) {
            free(word);
            return 0;
        }
        argv[(*argc)++] = newArgument;
    } else {
        status = glob(word, 0, NULL, &result);
        if (status != 0) {
            free(word);
            printf("ERRO NO glob: %d\n", status);
            return 0;
        }

        for (size_t i = 0; i < result.gl_pathc; i++) {
            newArgument = strdup(result.gl_pathv[i]);
            if (newArgument == NULL) {
                free(word);
                return 0;
            }
            argv[(*argc)++] = newArgument;
        }

        globfree(&result);
    }

    resetWord(word);

    return 1;
}

int tokenlize(char *str, int *argc, char *argv[]) {
    int state = NONE;

    if (*argc > 0) {
        for (int i = 0; i < *argc; i++)
            free(argv[i]);

        *argc = 0;
    }

    char *word = newWord(ARG_MAX_SIZE);
    if (word == NULL) {
        free(word);
        return -1;
    }

    *argc = 0;

    for (char *p = str; *p != '\0'; p++) {
        unsigned char c = *p;

        switch (state) {
            case NONE:
                if (isSpace(c)) {
                    continue;
                } else {
                    if (c == '"') {
                        state = IN_STRING;
                    } else {
                        state = IN_WORD;
                        insert(word, c);
                    }
                }
                continue;
            case IN_STRING:
                if (c == '"') {
                    state = NONE;
                    if (!includeArg(word, argc, argv)) return -1;
                } else {
                    insert(word, c);
                }
                continue;
            case IN_WORD:
                if (isSpace(c)) {
                    state = NONE;
                    if (!includeArg(word, argc, argv)) return -1;
                } else if (c == '"') {
                    continue;
                } else {
                    insert(word, c);
                }
                continue;
        }
    }

    if (state == IN_WORD) {
        if (!includeArg(word, argc, argv)) return -1;
    }

    free(word);
    return 0;
}
