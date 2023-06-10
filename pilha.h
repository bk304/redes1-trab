/* 
 * Biblioteca para pilhas.
 * De Bruno Krügel. GRR: GRR20206874
 * Para compilar use o padrão ansi.
*/

#ifndef PILHA_H
#define PILHA_H

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

typedef struct node_pilha {
        struct node_pilha *prox;
        void *data;
} node_pilha;

typedef struct pilha{
        int data_size;
        int nelementos;
        node_pilha *topo;                     
} type_pilha;

/* Cria e inicializa toda a estrutura necessaria para as pilhas. Retorna um vetor para a pilha criada.  */
type_pilha *criar_pilha(int size);

/* Retorna true se a pilha estiver vazia ou false caso contrário. */
bool pilha_vazia(type_pilha *p);

/* Retorna true se a pilha tiver um único elemento. */
bool pilha_unitaria(type_pilha *p);

/* Insere x no inicio da pilha. */
void empilhar(type_pilha *p, void *x);

/* Retorna true caso tenha conseguido colocar o item do inicio da pilha em ptr, removendo-o da pilha, e false, caso contrario. */
bool desempilhar(type_pilha *p, void *ptr);

/* Retorna true caso tenha conseguido colocar o item do inicio da pilha em ptr, mas sem remover ele da pilha, e false, caso contrario. */
bool topo(type_pilha *p, void *ptr);

/* Libera todo espaço usado pela pilha, perdendo-a para sempre. */
void destruir_pilha(type_pilha *p);
void recursao_destruir_pilha(node_pilha *n);

#endif
