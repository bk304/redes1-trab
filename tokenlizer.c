#include "tokenlizer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ARG_MAX_SIZE 4096

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

int insert(char *word, char c) {
    strncat(word, &c, sizeof(char));
}

char *newWord(int length) {
    char *word = (char *)calloc(length, sizeof(char));
    if (!word) {
        return NULL;
    } else {
        word[0] = '\0';
    }
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

    newArgument = strdup(word);
    if (newArgument == NULL) {
        free(word);
        return 0;
    }
    argv[(*argc)++] = newArgument;
    resetWord(word);

    return 1;
}

int tokenlize(char *str, int *argc, char *argv[]) {
    int state = NONE;
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
