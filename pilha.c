/* 
 * Biblioteca para pilhas.
 * De Bruno Krügel. GRR: GRR20206874
 * Para compilar use o padrão ansi.
*/

#include "pilha.h"

type_pilha *criar_pilha(int data_size){
	type_pilha *p = malloc(sizeof(type_pilha));
	p->data_size = data_size;
	p->nelementos = 0;
	p->topo = NULL;

	return p;
}

bool pilha_vazia(type_pilha *p){
	return (p->nelementos == 0);
}

bool pilha_unitaria(type_pilha *p){
	return (p->nelementos == 1);
}

void empilhar(type_pilha *p, void *x){
	node_pilha *novo_nodo = malloc(sizeof(node_pilha));
	novo_nodo->data = malloc(p->data_size);
	memcpy(novo_nodo->data, x, p->data_size);

	novo_nodo->prox = p->topo;
	p->topo = novo_nodo;
	p->nelementos++;
}

bool desempilhar(type_pilha *p, void *ptr){
	if(pilha_vazia(p))
		return false;

	memcpy(ptr, p->topo->data, p->data_size);

	node_pilha *aux = p->topo;
	p->topo = aux->prox;
	free(aux->data);
	free(aux);
	p->nelementos--;

	return true;
}

bool topo(type_pilha *p, void *ptr){
	if(pilha_vazia(p))
		return false;

	memcpy(ptr, p->topo->data, p->data_size);
	return true;
}

void destruir_pilha(type_pilha *p){
	if( !pilha_vazia(p))
		recursao_destruir_pilha(p->topo);
	free(p);
}

void recursao_destruir_pilha(node_pilha *n){
	if(n->prox != NULL)
		recursao_destruir_pilha(n->prox);
	free(n->data);
	free(n);
}
