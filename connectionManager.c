#define _GNU_SOURCE
#include "connectionManager.h"

#include <errno.h>
#include <pthread.h>
#include <sched.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "message.h"

#define WINDOW_TIMEOUT_SEC 1

sliding_window_t *window = NULL;

void lista_insere(sliding_window_node_t *p, sliding_window_node_t *lista);

sliding_window_t *sw_create(int slots) {
    sliding_window_node_t *p;
    sliding_window_t *window = malloc(sizeof(sliding_window_t));
    window->capacity = slots;
    window->size = 0;
    window->slots = NULL;
    window->emptySlots = malloc(slots * sizeof(sliding_window_node_t));
    pthread_mutex_init(&window->mutex, NULL);
    sem_init(&window->sem_vaga, 0, slots);
    sem_init(&window->sem_item, 0, 0);

    unsigned char *packets_buffer = malloc(slots * PACKET_SIZE_BYTES * sizeof(unsigned char));
    window->packetPointer = packets_buffer;
    message_t **messages = malloc(slots * sizeof(message_t *));
    for (int i = 0; i < slots; i++) {
        messages[i] = init_message((i * PACKET_SIZE_BYTES) + packets_buffer);
    }

    for (int i = 0; i < slots; i++) {
        p = &(window->emptySlots[i]);
        window->emptySlots[i].inUse = 0;
        window->emptySlots[i].data = messages[i];
        window->emptySlots[i].next = p;
        window->emptySlots[i].prev = p;
        if (window->emptySlots == NULL) {
            p->next = p;
            p->prev = p;
            window->emptySlots = p;
        } else {
            lista_insere(p, window->emptySlots);
        }
    }

    free(messages);

    return window;
};

void lista_insere(sliding_window_node_t *p, sliding_window_node_t *lista) {
    p->prev = lista->prev;
    p->next = lista->prev->next;

    lista->prev->next = p;
    lista->prev = p;
}

void sw_insert(sliding_window_t *window, sliding_window_node_t **slot) {
    // Use getSlot para pegar uma vaga
    // Espera-se que o slot já não está na fila EmptySlots
    pthread_mutex_lock(&window->mutex);

    sliding_window_node_t *p = *slot;
    p->inUse = 1;
    if (window->slots == NULL) {
        p->next = p;
        p->prev = p;
        window->slots = p;
    } else {
        lista_insere(p, window->slots);
    }

    *slot = NULL;
    window->size += 1;

    sem_post(&window->sem_item);  // Sinaliza que já um item na janela.
    pthread_mutex_unlock(&window->mutex);
    return;
}

void sw_remove(sliding_window_t *window, int qnt) {
    int n;
    for (n = qnt; n > 0; n--)
        sem_wait(&window->sem_item);  // Aguarda por um item
    pthread_mutex_lock(&window->mutex);
    for (n = qnt; n > 0; n--) {
        sliding_window_node_t *p = window->slots;
        if (p->next == p) {
            window->slots = NULL;
        } else {
            p->prev->next = p->next;
            p->next->prev = p->prev;
            window->slots = p->next;
        }
        p->next = p;
        p->prev = p;

        if (window->emptySlots == NULL)
            window->emptySlots = p;
        else {
            lista_insere(p, window->emptySlots);
        }

        p->inUse = 0;

        window->size -= 1;
    }
    for (n = qnt; n > 0; n--)
        sem_post(&window->sem_vaga);  // Libera uma vaga na janela.
    pthread_mutex_unlock(&window->mutex);
    return;
}

void sw_peek(sliding_window_t *window, sliding_window_node_t **slot) {
    sem_wait(&window->sem_item);  // Aguarda por um item
    pthread_mutex_lock(&window->mutex);

    *slot = window->slots;

    sem_post(&window->sem_item);  // Devolve a contagem de item
    pthread_mutex_unlock(&window->mutex);
    return;
}

void sw_flush(sliding_window_t *window) {
    sw_remove(window, window->size);
}

void sw_free(sliding_window_t *windowPtr) {
    free(windowPtr->packetPointer);
    free(windowPtr);
    window = NULL;
}

sliding_window_node_t *sw_getEmptySlot(sliding_window_t *window) {
    sem_wait(&window->sem_vaga);  // pega uma vaga
    pthread_mutex_lock(&window->mutex);

    sliding_window_node_t *p = window->emptySlots;
    if (p->next == p) {
        window->emptySlots = NULL;
    } else {
        p->next->prev = p->prev;
        p->prev->next = p->next;
        window->emptySlots = p->next;
    }

    p->next = p;
    p->prev = p;

    pthread_mutex_unlock(&window->mutex);
    return p;
}

void reenviaJanela(int socketFD, sliding_window_t *window) {
    pthread_mutex_lock(&window->mutex);
    // Reenvia a janela inteira.
    if (window->size != 0) {
        sliding_window_node_t *start = window->slots;
        if (send_message(socketFD, start->data) == -1) {
            fprintf(stderr, "ERRO NO SEND MESSAGE em _senderAssistant\n");
            exit(-1);
        }

        sliding_window_node_t *p = start->next;
        while (p != start) {
            if (p->inUse == 0)
                continue;
            if (send_message(socketFD, p->data) == -1) {
                fprintf(stderr, "ERRO NO SEND MESSAGE em _senderAssistant\n");
                exit(-1);
            }

            p = p->next;
        }
    }
    pthread_mutex_unlock(&window->mutex);
}

void _senderAssistant_cleanup(void *arg) {
    free(((void **)arg)[0]);
    sem_t *sem = (sem_t *)((void **)arg)[1];
    char *threadComplete = (char *)((void **)arg)[2];
    int val;
    *threadComplete = 1;
    sem_getvalue(sem, &val);
    val += window->capacity;
    while (val >= 0) {
        sem_post(sem);
        val -= 1;
    }
    sem_destroy(sem);
}

void *_senderAssistant(void *arg) {
    int socketFD = *((int *)((void **)arg)[0]);
    message_t *errorResponse = (message_t *)((void **)arg)[1];
    sem_t *sem = (sem_t *)((void **)arg)[2];
    char *threadCompleted = (char *)((void **)arg)[3];
    void *packets_buffer = malloc(2 * PACKET_SIZE_BYTES * sizeof(unsigned char));
    message_t *messageR = init_message(packets_buffer);
    message_t *messageNACK = init_message(packets_buffer + PACKET_SIZE_BYTES);
    sliding_window_node_t *slot = NULL;
    int receiveStatus;
    int n;
    int seq;
    message_t *data;

    void *cleanup_arg[] = {packets_buffer, sem, threadCompleted};
    pthread_cleanup_push(_senderAssistant_cleanup, cleanup_arg);

    for (;;) {
        receiveStatus = receive_message(socketFD, messageR, WINDOW_TIMEOUT_SEC);
        if (receiveStatus == RECVM_STATUS_PARITY_ERROR) {
            // Envia NACK
            messageNACK->type = C_NACK;
            messageNACK->sequence = 0;  // Não tem
            messageNACK->length = sizeof(char);
            messageNACK->data[0] = messageR->sequence;
            if (send_message(socketFD, messageNACK) == -1) {
                fprintf(stderr, "ERRO NO SEND MESSAGE em _senderAssistant\n");
                exit(-1);
            }
            continue;
        } else if (receiveStatus == RECVM_TIMEOUT) {
            // Reenvia a janela inteira.
            reenviaJanela(socketFD, window);
            printf("Ocorreu um timeout\n");
            continue;
        } else if (receiveStatus > 0) {
            // recebe normalmente
        } else {  // Inclui receiveStatus == RECVM_STATUS_ERROR
            fprintf(stderr, "ERRO NO READ MESSAGE em _senderAssistant\n");
            exit(-1);
        }

        if (messageR->type == C_METAMESSAGE) {
            if (messageR->data[0] == 1) {
                pthread_exit(NULL);
            }
        }

        if (messageR->type != C_ACK && messageR->type != C_NACK) {
            memcpy(errorResponse, messageR, sizeof(message_t));
            pthread_exit(NULL);
        }

        // Remove os pacotes confirmados
        // Só lembrando que messageR sempre será ACK ou NACK
        sw_peek(window, &slot);
        data = slot->data;
        if (((ACK_NACK_CODE *)messageR)->code < ((message_t *)data)->sequence)
            seq = ((ACK_NACK_CODE *)messageR)->code + (SEQ_MAX + 1);
        else
            seq = ((ACK_NACK_CODE *)messageR)->code;
        n = seq + (messageR->type == C_ACK ? 1 : 0) - ((message_t *)data)->sequence;
        sw_remove(window, n);
        for (int i = n; i > 0; i--) {
            sem_post(sem);
        }

        // Se (messageR.type == C_ACK){
        //     Faz Nada
        // }

        if (messageR->type == C_NACK) {
            // Reenvia a janela inteira.
            reenviaJanela(socketFD, window);
        }
    }

    pthread_cleanup_pop(1);
    pthread_exit(NULL);
}

int cm_send_message(int socketFD, void *buf, size_t len, int type, message_t *errorResponse) {
    if (window == NULL)
        window = sw_create(5);
#ifdef DEBUG
    printf("SENDING...\n");
    int itens;
    int vagas;
    sem_getvalue(&window->sem_vaga, &vagas);
    sem_getvalue(&window->sem_item, &itens);
    printf("Window: %d size | %d itens | %d vagas\n", window->size, itens, vagas);
#endif
    size_t bytesSent = 0;
    size_t yetToSend = len;
    size_t sendLength;
    int qntOfPackets = (len + DATA_MAX_SIZE_BYTES - 1) / DATA_MAX_SIZE_BYTES;
    if (qntOfPackets == 0)
        qntOfPackets = 1;
    int currSeq = 0;
    message_t *messageT;
    sliding_window_node_t *slot = NULL;
    pthread_t AssistantThread;
    char assistantExited = 0;
    sem_t sem;
    sem_init(&sem, 0, 0);
    char threadCompleted = 0;
    void *_trash;
    flush_recv_queue(socketFD);

    void *args[] = {(void *)&socketFD, (void *)errorResponse, (void *)&sem, (void *)&threadCompleted};
    if (pthread_create(&AssistantThread, NULL, _senderAssistant, args) != 0) {
        return -1;
    }

    // Envia uma metamensagem
    slot = sw_getEmptySlot(window);
    messageT = slot->data;
    messageT->type = C_METAMESSAGE;
    messageT->sequence = currSeq;
    messageT->length = sizeof(char);
    messageT->data[0] = 2;

    currSeq = NEXT_SEQUENCE(currSeq);
    sw_insert(window, &slot);
    if (send_message(socketFD, messageT) == -1) {
        fprintf(stderr, "ERRO NO WRITE MESSAGE em cm_send_message\n");
        exit(-1);
    }

    // Envia os segmentos
    for (int i = 0; i < qntOfPackets; i++) {
        // Produz um pacote de até DATA_MAX_SIZE_BYTES bytes
        if (slot == NULL)
            slot = sw_getEmptySlot(window);
        messageT = slot->data;
        messageT->type = type;
        messageT->sequence = currSeq;
        sendLength = (yetToSend >= DATA_MAX_SIZE_BYTES ? DATA_MAX_SIZE_BYTES : yetToSend);
        messageT->length = sendLength;
        memcpy(messageT->data, (buf + bytesSent), sendLength);

        currSeq = NEXT_SEQUENCE(currSeq);
        bytesSent += sendLength;
        yetToSend -= sendLength;

        sw_insert(window, &slot);
        if (send_message(socketFD, messageT) == -1) {
            fprintf(stderr, "ERRO NO WRITE MESSAGE em cm_send_message\n");
            exit(-1);
        }

        sem_wait(&sem);

        if (pthread_tryjoin_np(AssistantThread, (void **)&_trash) != EBUSY) {
            assistantExited = 1;
            bytesSent = -1;
            break;
        }
    }

    // Envia código de fim de mensagem
    if (slot == NULL)
        slot = sw_getEmptySlot(window);
    messageT = slot->data;
    messageT->type = C_METAMESSAGE;
    messageT->sequence = currSeq;
    messageT->length = sizeof(char);
    messageT->data[0] = 0;

    currSeq = NEXT_SEQUENCE(currSeq);
    sw_insert(window, &slot);
    if (send_message(socketFD, messageT) == -1) {
        fprintf(stderr, "ERRO NO WRITE MESSAGE em cm_send_message\n");
        exit(-1);
    }

    if (assistantExited == 0) {
        pthread_join(AssistantThread, (void **)&_trash);
    }

    sw_free(window);

#ifdef DEBUG
    printf("SENDING DONE.\n");
    printf("%ld BYTES SENT.\n", bytesSent);
#endif

    return bytesSent;
}

void _receiverAssistant_cleanup(void *arg) {
    free(((void **)arg)[0]);
    int val;
    sem_t *sem = (sem_t *)((void **)arg)[1];
    char *threadComplete = (char *)((void **)arg)[2];
    *threadComplete = 1;
    sem_getvalue(sem, &val);
    val += window->capacity;
    while (val >= 0) {
        sem_post(sem);
        val -= 1;
    }
    sem_destroy(sem);
}

void *_receiverAssistant(void *arg) {
    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
    int socketFD = *((int *)((void **)arg)[0]);
    sem_t *sem = (sem_t *)((void **)arg)[1];
    char *threadComplete = (char *)((void **)arg)[2];
    unsigned char *messageType = (unsigned char *)((void **)arg)[3];
    void *packets_buffer = malloc(2 * PACKET_SIZE_BYTES * sizeof(unsigned char));

    void *cleanup_arg[] = {packets_buffer, sem, threadComplete};
    pthread_cleanup_push(_receiverAssistant_cleanup, cleanup_arg);
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);

    message_t *messageR;                                                        // Mensagem recebida
    message_t *messageT = init_message(packets_buffer);                         // Usado pra enviar mensagens
    message_t *messageNACK = init_message(packets_buffer + PACKET_SIZE_BYTES);  // Usado pra enviar NACK em caso de erro de paridade
    sliding_window_node_t *slot = NULL;
    int currSeq = 0;
    char wrongSequence = 0;
    int receiveStatus;
    *messageType = C_ERROR;

    // Recebe a metamensagem
    void *packet_messageR = malloc(PACKET_SIZE_BYTES * sizeof(unsigned char));
    messageR = init_message(packet_messageR);
    receiveStatus = receive_message(socketFD, messageR, 0);
    if (receiveStatus == RECVM_STATUS_PARITY_ERROR) {
        // Envia NACK
        messageNACK->type = C_NACK;
        messageNACK->sequence = 0;  // Não tem
        messageNACK->length = sizeof(char);
        messageNACK->data[0] = messageR->sequence;
        if (send_message(socketFD, messageNACK) == -1) {
            fprintf(stderr, "ERRO NO SEND MESSAGE em _receiverAssistant\n");
            exit(-1);
        }
        continue;
    } else if (receiveStatus > 0) {
        // recebe normalmente
    } else {  // Inclui receiveStatus == RECVM_STATUS_ERROR
        fprintf(stderr, "ERRO NO READ MESSAGE em _receiverAssistant\n");
        exit(-1);
    }

    if (messageR->type != C_METAMESSAGE) {
        pthread_exit((void *)(-1));
    }
    if (messageR->data[0] <= 1) {
        pthread_exit((void *)(-1));
    }

    // Envia primeiro ACK
    messageT->type = C_ACK;
    messageT->sequence = 0;  // Não tem
    messageT->length = sizeof(char);
    messageT->data[0] = messageR->sequence;
    currSeq = NEXT_SEQUENCE(currSeq);
    if (send_message(socketFD, messageT) == -1) {
        fprintf(stderr, "ERRO NO SEND MESSAGE em _receiverAssistant\n");
        exit(-1);
    }
    free(packet_messageR);

    // ==
    for (;;) {
        if (slot == NULL)
            slot = sw_getEmptySlot(window);
        messageR = slot->data;
        receiveStatus = receive_message(socketFD, messageR, 0);
        if (receiveStatus == RECVM_STATUS_PARITY_ERROR) {
            // Envia NACK
            messageNACK->type = C_NACK;
            messageNACK->sequence = 0;  // Não tem
            messageNACK->length = sizeof(char);
            messageNACK->data[0] = messageR->sequence;
            if (send_message(socketFD, messageNACK) == -1) {
                fprintf(stderr, "ERRO NO SEND MESSAGE em _receiverAssistant\n");
                exit(-1);
            }
            continue;
        } else if (receiveStatus > 0) {
            // recebe normalmente
        } else {  // Inclui receiveStatus == RECVM_STATUS_ERROR
            fprintf(stderr, "ERRO NO READ MESSAGE em _receiverAssistant\n");
            exit(-1);
        }

        if (messageR->type == C_METAMESSAGE) {
            if (messageR->data[0] == 0) {
                // Fim da mensagem
                messageT->type = C_METAMESSAGE;
                messageT->sequence = currSeq;  // Não tem
                messageT->length = sizeof(char);
                messageT->data[0] = 1;  // ACK do fim da mensagem
                if (send_message(socketFD, messageT) == -1) {
                    fprintf(stderr, "ERRO NO SEND MESSAGE em _receiverAssistant\n");
                    exit(-1);
                }
                pthread_exit(NULL);
            } else {
                pthread_exit((void *)(-1));
            }
        }

        if (messageR->type == C_NACK) {
            // Reenvia a ultima mensagem que não foi NACK
            if (send_message(socketFD, messageT) == -1) {
                fprintf(stderr, "ERRO NO SEND MESSAGE em _receiverAssistant\n");
                exit(-1);
            }

            continue;
        }

        if (messageR->sequence < currSeq) {
            // ou messageR->sequence é maior que currSeq, mas ocorreu overflow
            // nesse caso devemos discartar a mensagem e mandar NACK do currseq
            // ou messageR->sequence é menor que currSeq, logo um ACK foi perdido
            // nesse caso devemos reenviar o ultimo ACK

            // Ha não ser que seja o segundo caso, será o primeiro
            wrongSequence = 1;

            int limit = PREV_MULT_SEQUENCE(messageR->sequence, window->capacity - 1);
            for (int s = PREV_SEQUENCE(messageR->sequence); s != limit; s = PREV_SEQUENCE(s)) {
                if (messageR->sequence == s) {
                    // messageR->sequence é menor que currSeq
                    wrongSequence = 2;
                }
            }

        } else if (messageR->sequence > currSeq) {
            // messageR->sequence é de fato maior que currSeq
            // nesse caso devemos discartar a mensagem e mandar NACK do currseq
            wrongSequence = 1;
        }

        if (!wrongSequence) {
            // Envia ACK
            messageT->type = C_ACK;
            messageT->sequence = 0;  // Não tem
            messageT->length = sizeof(char);
            messageT->data[0] = messageR->sequence;
            *messageType = (unsigned char)messageR->type;
            currSeq = NEXT_SEQUENCE(currSeq);
            if (send_message(socketFD, messageT) == -1) {
                fprintf(stderr, "ERRO NO SEND MESSAGE em _receiverAssistant\n");
                exit(-1);
            }
            sw_insert(window, &slot);
            sem_post(sem);
        } else if (wrongSequence == 1) {
            // Envia NACK (currSeq)
            wrongSequence = 0;
            messageNACK->type = C_NACK;
            messageNACK->sequence = 0;  // Não tem
            messageNACK->length = sizeof(char);
            messageNACK->data[0] = currSeq;
            if (send_message(socketFD, messageNACK) == -1) {
                fprintf(stderr, "ERRO NO SEND MESSAGE em _receiverAssistant\n");
                exit(-1);
            }
        } else if (wrongSequence == 2) {
            // O Ultimo ACK não foi recebido. Reenvie-o
            wrongSequence = 0;
            messageT->type = C_ACK;
            messageT->sequence = 0;  // Não tem
            messageT->length = sizeof(char);
            messageT->data[0] = messageR->sequence;
            *messageType = (unsigned char)messageR->type;
            currSeq = NEXT_SEQUENCE(currSeq);
            if (send_message(socketFD, messageT) == -1) {
                fprintf(stderr, "ERRO NO SEND MESSAGE em _receiverAssistant\n");
                exit(-1);
            }
        }

        // Cuidado com wrongSequence ao inserir código aqui. \/
    }

    pthread_cleanup_pop(1);
    pthread_exit(NULL);
}

int cm_receive_message(int socketFD, void *buf, size_t len, unsigned char *returnedType) {
    if (window == NULL)
        window = sw_create(5);
#ifdef DEBUG
    printf("WAITING...\n");
    int itens;
    int vagas;
    sem_getvalue(&window->sem_vaga, &vagas);
    sem_getvalue(&window->sem_item, &itens);
    printf("Window: %d size | %d itens | %d vagas", window->size, itens, vagas);
#endif
    size_t bytesReceived = 0;
    message_t *messageR;
    pthread_t AssistantThread;
    sem_t sem;
    sliding_window_node_t *slot = NULL;
    char threadComplete = 0;
    sem_init(&sem, 0, 0);
    // Esse semaforo trava essa thread quando a janela está vazia,
    // mas com a vantagem de ser destroido quando a thread termina.
    char assistantExited = 0;
    unsigned char messageType;
    void *_trash;
    flush_recv_queue(socketFD);

    void *args[] = {(void *)&socketFD, (void *)&sem, (void *)&threadComplete, (void *)&messageType};
    if (pthread_create(&AssistantThread, NULL, _receiverAssistant, args) != 0)
        return -1;

    sem_wait(&sem);
    for (;;) {
        sw_peek(window, &slot);
        messageR = slot->data;
        if ((bytesReceived + messageR->length) > len) {
            // Avisa o client de erro de buffer cheio
            pthread_cancel(AssistantThread);
            void *packet_buffer = malloc(PACKET_SIZE_BYTES * sizeof(unsigned char));
            message_t *messageT = init_message(packet_buffer);

            messageError *messageErr = (messageError *)messageT;
            messageErr->type = C_ERROR;
            messageErr->sequence = 0;  // Não tem
            messageErr->length = sizeof(char);
            messageErr->errorCode = BUFFER_FULL;
            messageErr->extraInfo = bytesReceived;

            if (send_message(socketFD, messageT) == -1) {
                fprintf(stderr, "ERRO NO SEND MESSAGE em _receiverAssistant_cleanup\n");
                exit(-1);
            }
            free(packet_buffer);
            break;  // Buffer cheio
        }

        mempcpy((buf + bytesReceived), messageR->data, messageR->length);
        bytesReceived += messageR->length;
        sw_remove(window, 1);
        sem_wait(&sem);

        if (threadComplete == 1 || pthread_tryjoin_np(AssistantThread, (void **)&_trash) != EBUSY) {
            // Fim da mensagem
            assistantExited = 1;
            break;
        }
    }

    if (assistantExited == 0) {
#ifdef DEBUG
        printf("Esperando thread...\n");
#endif
        pthread_join(AssistantThread, (void **)&_trash);
#ifdef DEBUG
        printf("Pronto. Thread morreu.\n");
#endif
    }

    if (messageType == C_ERROR) {
        bytesReceived = -1;
    }

    *returnedType = messageType;

    sw_free(window);

#ifdef DEBUG
    printf("WAITING DONE.\n");
#endif

    return bytesReceived;
}
