#ifndef CONNECTION_MANAGET_H
#define CONNECTION_MANAGET_H

#include <pthread.h>
#include <semaphore.h>

#include "message.h"

typedef struct sliding_window_node_t {
    t_message *data;
    struct sliding_window_node_t *next;
    struct sliding_window_node_t *prev;
    char inUse;
} sliding_window_node_t;

typedef struct sliding_window_t {
    sliding_window_node_t *slots;
    sliding_window_node_t *emptySlots;
    void *packetPointer; // Usado pra desalocar todas as mensagens
    int capacity;
    int size;
    pthread_mutex_t mutex;
    sem_t sem_vaga;  // Semáforo para controlar as vagas disponíveis na fila
    sem_t sem_item;  // Semáforo para controlar a disponibilidade de itens na fila
} sliding_window_t;

sliding_window_t *sw_create(int slots);

// Insere um item na janela. Bloqueante caso esteja cheia.
void sw_insert(sliding_window_t *window, sliding_window_node_t **slot) ;

// Retorna o primeiro item na janela no ponteiro data. Bloqueante caso esteja vazia.
void sw_remove(sliding_window_t *window);
void sw_no_mutex_remove(sliding_window_t *window);

// Retorna o primeiro item na janela no ponteiro data, mas sem remover ele. Bloqueante caso esteja vazia.
void sw_peek(sliding_window_t *window, sliding_window_node_t **slot);
// Retorna 1 caso esteja cheia, 0 caso contrario.
int sw_isFull(sliding_window_t *window);

// Retorna 1 se estiver vazia, 0 caso contrario.
int sw_isEmpty(sliding_window_t *window);

void sw_free(sliding_window_t *window);

void sw_flush(sliding_window_t *window);

int cm_send_message(int socketFD, const void *buf, size_t len, int type, t_message *errorResponse);

int cm_receive_message(int socketFD, void *buf, size_t len, unsigned char *type);

#endif  // CONNECTION_MANAGET_H
