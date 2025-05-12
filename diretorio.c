#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdint.h>
#include <sys/stat.h>
#include <unistd.h>

#include "diretorio.h"

// Verifica se um arquivo de archive existe
int diretorio_existe(const char *nome_diretorio) {
    struct stat buffer;
    return (stat(nome_diretorio, &buffer) == 0);
}

// Cria uma nova estrutura de diretório vazia
diretorio_vinac *criar_diretorio() {
    diretorio_vinac *dir = malloc(sizeof(diretorio_vinac));
    if (!dir) {
        fprintf(stderr, "Erro: Falha ao alocar memória para diretório\n");
        return NULL;
    }

    memset(dir, 0, sizeof(diretorio_vinac));  // <--- ESSENCIAL

    dir->cabecalho.assinatura = VINAC_SIGNATURE;
    dir->cabecalho.versao = 1; // Versão do formato
    dir->cabecalho.num_membros = 0;
    dir->cabecalho.offset_dir = 0; // Será atualizado em finalizar_diretorio
    dir->cabecalho.tam_cabecalho = sizeof(cabecalho_vinac);

    dir->inicio = NULL;
    dir->fim = NULL;

    return dir;
}

// Encontra o maior UID no archive
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

// Carrega o diretório de um archive existente em memória
diretorio_vinac *carregar_diretorio(const char *nome_arquivo) {
    if (!nome_arquivo) return NULL;

    FILE *f = fopen(nome_arquivo, "rb");
    if (!f) {
        fprintf(stderr, "Erro ao abrir arquivo %s: %s\n", nome_arquivo, strerror(errno));
        return NULL;
    }

    diretorio_vinac *dir = malloc(sizeof(diretorio_vinac));
    if (!dir) {
        fprintf(stderr, "Erro: Falha ao alocar memória para diretório\n");
        fclose(f);
        return NULL;
    }

    memset(dir, 0, sizeof(diretorio_vinac));

    if (fread(&dir->cabecalho, sizeof(cabecalho_vinac), 1, f) != 1) {
        fprintf(stderr, "Erro: Falha ao ler cabeçalho de %s\n", nome_arquivo);
        free(dir);
        fclose(f);
        return NULL;
    }

    if (dir->cabecalho.assinatura != VINAC_SIGNATURE) {
        fprintf(stderr, "Erro: Assinatura inválida no cabeçalho de %s\n", nome_arquivo);
        free(dir);
        fclose(f);
        return NULL;
    }

    if (dir->cabecalho.versao != 1) {
        fprintf(stderr, "Erro: Versão não suportada (%u) em %s\n", dir->cabecalho.versao, nome_arquivo);
        free(dir);
        fclose(f);
        return NULL;
    }

    if (fseek(f, dir->cabecalho.offset_dir, SEEK_SET) != 0) {
        fprintf(stderr, "Erro: Falha ao posicionar no diretório de %s: %s\n", nome_arquivo, strerror(errno));
        free(dir);
        fclose(f);
        return NULL;
    }

    for (uint32_t i = 0; i < dir->cabecalho.num_membros; i++) {
        entrada_membro membro;
        memset(&membro, 0, sizeof(membro));  // 🔒 evita leitura de lixo no padding

        if (fread(&membro, sizeof(entrada_membro), 1, f) != 1) {
            fprintf(stderr, "Erro: Falha ao ler entrada de membro %u em %s: %s\n", i, nome_arquivo, strerror(errno));
            liberar_diretorio(dir);
            fclose(f);
            return NULL;
        }

        if (membro.tamanho_orig == 0 && membro.tamanho_disco != 0) {
            fprintf(stderr, "Erro: Metadados inválidos para membro %u (%s)\n", i, membro.nome);
            liberar_diretorio(dir);
            fclose(f);
            return NULL;
        }

        if (membro.comprimido && membro.tamanho_disco >= membro.tamanho_orig) {
            fprintf(stderr, "Erro: Membro %s é marcado como comprimido mas tamanho_disco >= tamanho_orig\n", membro.nome);
            liberar_diretorio(dir);
            fclose(f);
            return NULL;
        }

        if (strlen(membro.nome) >= NOME_MAXIMO) {
            fprintf(stderr, "Erro: Nome de membro muito longo: %s\n", membro.nome);
            liberar_diretorio(dir);
            fclose(f);
            return NULL;
        }

        entrada_no *no = malloc(sizeof(entrada_no));
        if (!no) {
            fprintf(stderr, "Erro: Falha ao alocar memória para entrada de membro\n");
            liberar_diretorio(dir);
            fclose(f);
            return NULL;
        }
        memset(no, 0, sizeof(entrada_no));
        memcpy(&no->membros, &membro, sizeof(entrada_membro));

        no->anterior = dir->fim;
        if (dir->fim) dir->fim->prox = no;
        dir->fim = no;
        if (!dir->inicio) dir->inicio = no;
    }

    fclose(f);
    return dir;
}

// Libera a estrutura de diretório e a lista encadeada
void liberar_diretorio(diretorio_vinac *dir) {
    entrada_no *atual = dir->inicio;
    while (atual) {
        entrada_no *prox = atual->prox;
        free(atual);
        atual = prox;
    }
    dir->inicio = NULL;
    dir->fim = NULL;
    free(dir);
}

// Encontra uma entrada de membro por nome
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

// Adiciona uma entrada de membro ao final da lista encadeada
void adicionar_entrada(diretorio_vinac *dir, const entrada_membro *nova_entrada) {
    entrada_no *novo_no = (entrada_no *)malloc(sizeof(entrada_no));
    if (!novo_no) {
        printf("Erro: Falha ao alocar memória para entrada do membro '%s'\n", nova_entrada->nome);
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

// Remove uma entrada de membro por nome
int remover_entrada(diretorio_vinac *dir, const char *nome) {
    if (!dir || !dir->inicio) return 0;

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
            return 1;
        }
        atual = atual->prox;
    }
    return 0;
}

//  Escreve o diretório e o cabeçalho atualizado no archive
void finalizar_diretorio(FILE *f, diretorio_vinac *dir) {
    if (!f || !dir) return;

    uint32_t contador = 0;
    entrada_no *atual = dir->inicio;
    while (atual) {
        contador++;
        atual = atual->prox;
    }
    dir->cabecalho.num_membros = contador;

    dir->cabecalho.offset_dir = ftell(f);

    atual = dir->inicio;
    while (atual) {
        entrada_membro m;
        memset(&m, 0, sizeof(m));  
        memcpy(&m, &atual->membros, sizeof(entrada_membro));

        if (fwrite(&m, sizeof(m), 1, f) != 1) {
            fprintf(stderr, "Erro: Falha ao gravar entrada do diretório\n");
            return;
        }
        atual = atual->prox;
    }

    if (fseek(f, 0, SEEK_SET) != 0) {
        fprintf(stderr, "Erro: Falha ao posicionar no início do arquivo\n");
        return;
    }

    cabecalho_vinac cab;
    memset(&cab, 0, sizeof(cab));  // evita lixo na escrita
    memcpy(&cab, &dir->cabecalho, sizeof(cabecalho_vinac));

    if (fwrite(&cab, sizeof(cab), 1, f) != 1) {
        fprintf(stderr, "Erro: Falha ao atualizar cabeçalho\n");
    }
}
