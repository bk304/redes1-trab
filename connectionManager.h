#ifndef CONNECTION_MANAGET_H
#define CONNECTION_MANAGET_H

typedef struct pthread_mutex_t pthread_mutex_t;
typedef struct sem_t sem_t;

typedef struct sliding_window {
    sliding_window_node_t *slots;
    int capacity;
    int size;
    pthread_mutex_t mutex;
    sem_t sem_vaga;  // Semáforo para controlar as vagas disponíveis na fila
    sem_t sem_item;  // Semáforo para controlar a disponibilidade de itens na fila
} sliding_window_t;

typedef struct sliding_window_node {
    void *data;
    sliding_window_node_t *next;
    char inUse;
    char validated;
} sliding_window_node_t;

sliding_window_t *sw_create(int slots);

// Insere um item na janela. Bloqueante caso esteja cheia.
void sw_insert(sliding_window_t *window, void *data);

// Retorna o primeiro item na janela no ponteiro data. Bloqueante caso esteja vazia.
void sw_remove(sliding_window_t *window, void **data);

// Retorna 1 caso esteja cheia, 0 caso contrario.
int sw_isFull(sliding_window_t *window);

// Retorna 1 se estiver vazia, 0 caso contrario.
int sw_isEmpty(sliding_window_t *window);

void sw_free(sliding_window_t *window);

void *cm_init(void *args);

#endif  // CONNECTION_MANAGET_H