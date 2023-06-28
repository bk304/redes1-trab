#!/bin/bash

# Diretório de destino para as bibliotecas
LIBS_DIR="./serverFolder"

# Lista das bibliotecas necessárias
LIBS=(
    "/lib/x86_64-linux-gnu/libc.so.6"
    "/lib/x86_64-linux-gnu/libm.so.6"
    "/lib/x86_64-linux-gnu/libpthread.so.0"
    "/lib/x86_64-linux-gnu/libgcc_s.so.1"
    "/lib64/ld-linux-x86-64.so.2"
    # Adicione outras bibliotecas necessárias aqui
)

# Criar diretório de destino, se não existir
mkdir -p "$LIBS_DIR"

# Copiar as bibliotecas para o diretório de destino
for lib in "${LIBS[@]}"; do
    cp -v --parents "$lib" "$LIBS_DIR"
done

# Verificar e corrigir permissões das bibliotecas
chown -R root:root "$LIBS_DIR"
chmod -R 755 "$LIBS_DIR"