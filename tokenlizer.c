#include "tokenlizer.h"
enum {
  NONE,
  IN_WORD, 
  IN_STRING
}

char isSpace(unsigned char c){
  return c == ' ';
}

int tokenlize(int *argc, char **argv[], char *str){
  int state = NONE; 
  char *v = malloc(1024 * sizeof(char));
  if(!v){
    return -1;
  }

  char *start_of_arg;
  char *end_of_arg;

  for(char *p = str[0]; p != '\0'; p++){
    unsigned char c = *p;
    switch(state){
      case NONE:
        if(isSpace(c)){
          continue;
        }
        else{
          if(c == '"'){
            state = IN_STRING;
            start_of_arg = p + 1;
          }
          else{
            state = IN_WORD;
            start_of_arg = p;
          }
        }
        continue;
      case IN_STRING:
        
        continue;
    }
  }
}
