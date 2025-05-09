#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdint.h>
#include <sys/stat.h>
#include <unistd.h> // Para definição de `time_t` em alguns sistemas

#include "arquivo.h"

// Implementações das funções auxiliares
int arquivo_existe(const char *nome_arquivo) {
    struct stat buffer;
    return (stat(nome_arquivo, &buffer) == 0);
}

diretorio_vinac *criar_diretorio() {
    diretorio_vinac *dir = (diretorio_vinac *)malloc(sizeof(diretorio_vinac));
    if (dir) {
        // Inicializar o cabeçalho
        dir->cabecalho.versao = 1; // Versão inicial
        dir->cabecalho.num_membros = 0;
        dir->cabecalho.offset_dir = sizeof(cabecalho_vinac); // Offset após o cabeçalho
        dir->inicio = NULL;
        dir->fim = NULL;
    }
    return dir;
}

uint32_t obter_maior_uid(diretorio_vinac *dir) {
    uint32_t maior = 0;
    entrada_no *atual = dir->inicio;
    while (atual) {
        if (atual->membros.uid > maior) {
            maior = atual->membros.uid;
        }
        atual = atual->prox;
    }
    return maior;
}

diretorio_vinac *carregar_diretorio(const char *nome_arquivo) {
    diretorio_vinac *dir = malloc(sizeof(diretorio_vinac));
    if (!dir) return NULL;

    FILE *arquivo_vinac = fopen(nome_arquivo, "rb");
    if (!arquivo_vinac) {
        free(dir);
        return NULL;
    }

    // Ler o cabeçalho
    if (fread(&dir->cabecalho, sizeof(cabecalho_vinac), 1, arquivo_vinac) != 1) {
        fclose(arquivo_vinac);
        free(dir);
        return NULL;
    }

    dir->inicio = dir->fim = NULL;
    fseek(arquivo_vinac, dir->cabecalho.offset_dir, SEEK_SET);

    for (uint32_t i = 0; i < dir->cabecalho.num_membros; i++) {
        entrada_no *novo_no = malloc(sizeof(entrada_no));
        if (!novo_no) {
            fclose(arquivo_vinac);
            liberar_diretorio(dir);
            return NULL;
        }

        // Ler dados do membro
        if (fread(&novo_no->membros, sizeof(entrada_membro), 1, arquivo_vinac) != 1) {
            free(novo_no);
            fclose(arquivo_vinac);
            liberar_diretorio(dir);
            return NULL;
        }

        novo_no->prox = NULL;
        novo_no->anterior = dir->fim;
        if (dir->fim) {
            dir->fim->prox = novo_no;
        } else {
            dir->inicio = novo_no;
        }
        dir->fim = novo_no;
    }

    // Atualiza UID global com base nos membros lidos
    uid_atual = obter_maior_uid(dir) + 1;

    fclose(arquivo_vinac);
    return dir;
}


void liberar_diretorio(diretorio_vinac *dir) {
    if (dir) {
        entrada_no *atual = dir->inicio;
        while (atual) {
            entrada_no *temp = atual;
            atual = atual->prox;
            free(temp);
        }
        free(dir);
    }
}

entrada_no *encontrar_entrada(diretorio_vinac *dir, const char *nome) {
    if (!dir || !dir->inicio) return NULL;

    entrada_no *atual = dir->inicio;
    while (atual) {
        if (strcmp(atual->membros.nome, nome) == 0) {
            return atual;
        }
        atual = atual->prox;
    }
    return NULL;
}

void adicionar_entrada(diretorio_vinac *dir, const entrada_membro *nova_entrada) {
    entrada_no *novo_no = (entrada_no *)malloc(sizeof(entrada_no));
    if (!novo_no) {
        printf("Erro ao alocar memória para nova entrada\n");
        return;
    }

    novo_no->membros = *nova_entrada;
    novo_no->prox = NULL;
    novo_no->anterior = dir->fim;

    if (dir->fim) {
        dir->fim->prox = novo_no;
    } else {
        dir->inicio = novo_no;
    }
    dir->fim = novo_no;

    dir->cabecalho.num_membros++;
}

void remover_entrada(diretorio_vinac *dir, const char *nome) {
    if (!dir || !dir->inicio) return;

    entrada_no *atual = dir->inicio;
    while (atual) {
        if (strcmp(atual->membros.nome, nome) == 0) {
            if (atual->anterior) {
                atual->anterior->prox = atual->prox;
            } else {
                dir->inicio = atual->prox;
            }

            if (atual->prox) {
                atual->prox->anterior = atual->anterior;
            } else {
                dir->fim = atual->anterior;
            }

            free(atual);
            dir->cabecalho.num_membros--;
            return;
        }
        atual = atual->prox;
    }
}