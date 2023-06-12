#include <pthread.h>
#include <semaphore.h>
#include <stdlib.h>

#include "conectionManager.h"

sliding_window_t *window;
pthread_t *connectionManagerThread_Transmiter;
pthread_t *connectionManagerThread_Receiver;

sliding_window_t *sw_create(int slots) {
    sliding_window_t *window = malloc(sizeof(sliding_window_t));
    window->capacity = slots;
    window->size = 0;
    window->slots = malloc(slots * sizeof(sliding_window_node_t));
    pthread_mutex_init(&window->mutex, NULL);
    sem_init(&window->sem_vaga, 0, slots);
    sem_init(&window->sem_item, 0, 0);

    for (int i = 0; i < slots - 1; i++) {
        window->slots[i].inUse = 0;
        window->slots[i].next = window->slots + ((i + 1) * sizeof(sliding_window_node_t));
    }
    window->slots[slots - 1].inUse = 0;
    window->slots[slots - 1].next = window->slots;

    return window;
};

void sw_insert(sliding_window_t *window, void *data) {
    sem_wait(&window->sem_vaga);  // Aguarda uma vaga
    pthread_mutex_lock(&window->mutex);

    sliding_window_node_t *p = window.slots;
    while (p->inUse == 1) {
        p = p->next;
    }

    p->inUse = 1;
    p->validated = 0;
    p->data = data;

    window->size += 1;

    sem_post(&window->sem_item);  // Sinaliza que já um item na janela.
    pthread_mutex_unlock(&window->mutex);
    return;
}

void sw_remove(sliding_window_t *window, void **data) {
    sem_wait(&window->sem_item);  // Aguarda por um item
    pthread_mutex_lock(&window->mutex);

    sliding_window_node_t *p = window.slots;
    *data = p->data;
    p->inUse = 0;
    window->slots = p->next;

    window->size -= 1;

    sem_post(&window->sem_vaga);  // Libera uma vaga na janela.
    pthread_mutex_unlock(&window->mutex);
    return;
}

void sw_waitForItem(sliding_window_t *window) {
    sem_wait(&window->sem_item);
    sem_post(&window->sem_item);
    return;
}

int sw_isFull(sliding_window_t *window) {
    if (window->size >= window->capacity) {
        return 0;
    } else {
        return 1;
    }
}

int sw_isEmpty(sliding_window_t *window) {
    return (window->size == 0);
}

void sw_free(sliding_window_t *window) {
    free(window);
}

void *cm_init(void *args) {
    sliding_window_t *window = sw_create(5);

    connectionManagerThread_Transmiter = malloc(sizeof(pthread_t));
    connectionManagerThread_Receiver = malloc(sizeof(pthread_t));

    pthread_create(connectionManagerThread_Transmiter, NULL, cm_init_T, (void *)window);
    pthread_create(connectionManagerThread_Receiver, NULL, cm_init_R, (void *)window);
}

void *cm_init_T(void *args) {
    window = args;
    sliding_window_node_t *p;

    while (true) {
        sw_waitForItem(window);
        p = window->slots;
        while (p->inUse == 1) {
            // send(p->data)
            p = p->next;
        }

        // send()
    }
}

void *cm_init_R(void *args) {
    window = args;
    sliding_window_node_t *p;

    while (true) {
        // item = recv()
        p = window->slots;
        while (p->inUse == 1) {
            // send(p->data)
            p = p->next;
        }

        // send()
    }
}