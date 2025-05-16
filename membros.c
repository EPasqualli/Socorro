#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdint.h>
#include <inttypes.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <limits.h>
#include <string.h> 
#include <sys/types.h>
#include <unistd.h>

#include "membros.h"
#include "lz.h"

// Atualiza o campo ordem de cada membro na lista encadeada
void atualizar_ordem(diretorio_vinac *dir) {
    entrada_no *atual = dir->inicio;
    uint32_t ordem = 1;
    while (atual) {
        atual->membros.ordem = ordem++;
        atual = atual->prox;
    }
}

// Verifica se um membro existe no archive
int membro_existe(const char *nome, diretorio_vinac *dir) {
    if (!dir || !nome) return 0;

    entrada_no *atual = dir->inicio;
    while (atual) {
        if (strcmp(atual->membros.nome, nome) == 0) {
            return 1;
        }
        atual = atual->prox;
    }
    return 0;
}

// Cria uma entrada de membro a partir de metadados do arquivo
entrada_membro criar_entrada_membro(const char *nome_arquivo, struct stat *info, long offset, int comprimir, uint32_t uid_usuario) {
    entrada_membro entrada;
    memset(&entrada, 0, sizeof(entrada));
    strncpy(entrada.nome, nome_arquivo, 1023);
    entrada.nome[1023] = '\0';
    entrada.uid = uid_usuario;
    entrada.tamanho_orig = info->st_size;
    entrada.data_mod = info->st_mtime;
    entrada.offset = offset;
    entrada.comprimido = comprimir;
    return entrada;
}

// Processa um membro para inserção, lidando com substituições
entrada_membro *processar_membro(diretorio_vinac *dir, const char *membro, int comprimir, long offset_fornecido, uint32_t *ordem_existente) {
    if (strchr(membro, ' ')) {
        printf("Erro: Nome do arquivo '%s' contém espaços\n", membro);
        return NULL;
    }

    struct stat info;
    if (stat(membro, &info) != 0) {
        printf("Erro: Não foi possível acessar o arquivo '%s'\n", membro);
        return NULL;
    }

    entrada_membro *entrada = malloc(sizeof(entrada_membro));
    if (!entrada) {
        printf("Erro: Falha ao alocar memória para entrada do membro '%s'\n", membro);
        return NULL;
    }

    uid_t uid_usuario = getuid(); // Obter o UID do usuário

    *entrada = criar_entrada_membro(membro, &info, offset_fornecido, comprimir, uid_usuario);

    entrada_no *existente = encontrar_entrada(dir, membro);
    if (existente) {
        *ordem_existente = existente->membros.ordem;
        remover_entrada(dir, membro);
    } else {
        *ordem_existente = dir->cabecalho.num_membros;
    }

    return entrada;
}