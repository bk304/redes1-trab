#ifndef __TOKENLIZER_H__
#define __TOKENLIZER_H__

#define ARG_MAX_SIZE 4096

// Esse tokenlize aloca diversas strings em argv.
// Ele foi pensado para ser usado exclusivamente nesse projeto.
// Então ele identifica characteres '*' e expande como se fosse no bash.
int tokenlize(char *str, int *argc, char *argv[]);

#endif  // !__TOKENLIZER_H__
