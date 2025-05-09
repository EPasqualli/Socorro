#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdint.h>
#include <sys/stat.h>

#include "membros.h"
#include "lz.h"

uint32_t uid_atual = 1; // UID global persistente durante a execução

uint32_t obter_proximo_uid() {
    return uid_atual++;
}

diretorio_vinac *abrir_ou_criar_diretorio(const char *arquivo, FILE **fp_out) {
    diretorio_vinac *dir = NULL;

    if (arquivo_existe(arquivo)) {
        printf("\nDIRETORIO EXISTE -> CARREGANDO ITENS\n");
        dir = carregar_diretorio(arquivo);
        *fp_out = fopen(arquivo, "r+b");
    } else {
        printf("\nDIRETORIO NAO EXISTE -> CRIANDO DIRETORIO\n");
        dir = criar_diretorio();
        *fp_out = fopen(arquivo, "wb");
        if (*fp_out && dir) {
            fwrite(&dir->cabecalho, sizeof(cabecalho_vinac), 1, *fp_out);
        }
    }

    if (!*fp_out || !dir) {
        if (dir) liberar_diretorio(dir);
        return NULL;
    }

    return dir;
}

entrada_membro criar_entrada_membro(const char *nome_arquivo, struct stat *info, long offset, int comprimir) {
    entrada_membro entrada;
    memset(&entrada, 0, sizeof(entrada));
    strncpy(entrada.nome, nome_arquivo, 1023);
    entrada.nome[1023] = '\0';
    entrada.uid = obter_proximo_uid();
    entrada.tamanho_orig = info->st_size;
    entrada.data_mod = info->st_mtime;
    entrada.offset = offset;
    entrada.comprimido = comprimir;
    return entrada;
}

uint64_t escrever_dados(FILE *dest, FILE *orig, int comprimir, uint64_t tamanho_orig, uint8_t *buffer_tmp) {
    if (!comprimir) {
        size_t total = 0, lido;
        while ((lido = fread(buffer_tmp, 1, TAMANHO_BUFFER, orig)) > 0) {
            fwrite(buffer_tmp, 1, lido, dest);
            total += lido;
        }
        return total;
    }

    // Comprimir
    unsigned char *buffer_entrada = malloc(tamanho_orig);
    if (!buffer_entrada) return 0;

    fread(buffer_entrada, 1, tamanho_orig, orig);

    unsigned int max_tam = (tamanho_orig * 257) / 256 + 1;
    unsigned char *buffer_comp = malloc(max_tam);
    if (!buffer_comp) {
        free(buffer_entrada);
        return 0;
    }

    unsigned int tam_comp = LZ_Compress(buffer_entrada, buffer_comp, tamanho_orig);

    if (tam_comp < tamanho_orig) {
        fwrite(buffer_comp, 1, tam_comp, dest);
        free(buffer_entrada);
        free(buffer_comp);
        return tam_comp;
    } else {
        fseek(orig, 0, SEEK_SET);
        size_t total = 0, lido;
        while ((lido = fread(buffer_tmp, 1, TAMANHO_BUFFER, orig)) > 0) {
            fwrite(buffer_tmp, 1, lido, dest);
            total += lido;
        }
        free(buffer_entrada);
        free(buffer_comp);
        return total;
    }
}

void processar_membro(FILE *arquivo_vinac, diretorio_vinac *dir, const char *membro, int comprimir) {
    struct stat info;
    if (stat(membro, &info) != 0) return;

    FILE *fp_membro = fopen(membro, "rb");
    if (!fp_membro) return;

    long offset = ftell(arquivo_vinac);
    entrada_membro entrada = criar_entrada_membro(membro, &info, offset, comprimir);

    entrada_no *existente = encontrar_entrada(dir, membro);
    if (existente) {
        remover_entrada(dir, membro);
        entrada.ordem = existente->membros.ordem;
    } else {
        entrada.ordem = dir->cabecalho.num_membros;
    }

    uint8_t buffer[TAMANHO_BUFFER];
    entrada.tamanho_disco = escrever_dados(arquivo_vinac, fp_membro, comprimir, info.st_size, buffer);
    entrada.comprimido = (entrada.tamanho_disco < info.st_size) ? 1 : 0;

    adicionar_entrada(dir, &entrada);
    fclose(fp_membro);  // Feche o arquivo aqui!

    printf("Arquivo \"%s\" adicionado com sucesso.\n", entrada.nome);
}

void finalizar_diretorio(FILE *arquivo_vinac, diretorio_vinac *dir) {
    long pos = ftell(arquivo_vinac);
    dir->cabecalho.offset_dir = pos;

    fseek(arquivo_vinac, 0, SEEK_SET);
    fwrite(&dir->cabecalho, sizeof(cabecalho_vinac), 1, arquivo_vinac);

    entrada_no *atual = dir->inicio;
    while (atual) {
        fwrite(&atual->membros, sizeof(entrada_membro), 1, arquivo_vinac);
        atual = atual->prox;
    }

}


int inserir_membros(const char *arquivo, char **membros, int num_membros, int comprimir) {
    FILE *fp = NULL;
    diretorio_vinac *dir = abrir_ou_criar_diretorio(arquivo, &fp);
    if (!dir || !fp) return 1;

    printf("\nAQUI\n");
    fseek(fp, 0, SEEK_END);
    for (int i = 0; i < num_membros; i++) {
        processar_membro(fp, dir, membros[i], comprimir);
    }

    finalizar_diretorio(fp, dir);
    fclose(fp);
    liberar_diretorio(dir);
    return 0;
}


/* Função para mover um membro 
int mover_membro(const char *arquivo, const char *membro, const char *alvo) {
    diretorio_vinac_t *dir = carregar_diretorio(arquivo);
    if (!dir) {
        printf("Erro ao carregar o arquivo %s\n", arquivo);
        return 1;
    }

    entrada_no_t *ant = NULL, *atual = dir->inicio;
    entrada_no_t *movido = NULL;

    // Buscar e remover o nó do membro a ser movido
    while (atual) {
        if (strcmp(atual->entrada.nome, membro) == 0) {
            if (ant) ant->prox = atual->prox;
            else dir->inicio = atual->prox;
            movido = atual;
            break;
        }
        ant = atual;
        atual = atual->prox;
    }

    if (!movido) {
        printf("Membro \"%s\" não encontrado.\n", membro);
        liberar_diretorio(dir);
        return 1;
    }

    // Atualizar ordem (isso será refeito depois)
    movido->entrada.ordem = 0;

    // Inserir no novo local
    if (!alvo || strcmp(alvo, "NULL") == 0) {
        // Inserir no início
        movido->prox = dir->inicio;
        dir->inicio = movido;
    } else {
        entrada_no_t *ptr = dir->inicio;
        while (ptr && strcmp(ptr->entrada.nome, alvo) != 0) {
            ptr = ptr->prox;
        }

        if (!ptr) {
            printf("Membro-alvo \"%s\" não encontrado.\n", alvo);
            free(movido); // ou reinsere no final, se preferir
            liberar_diretorio(dir);
            return 1;
        }

        movido->prox = ptr->prox;
        ptr->prox = movido;
    }

    // Reatribuir as ordens com base na nova ordem da lista
    int ordem = 0;
    entrada_no_t *tmp = dir->inicio;
    while (tmp) {
        tmp->entrada.ordem = ordem++;
        tmp = tmp->prox;
    }

    FILE *f = fopen(arquivo, "r+b");
    if (!f) {
        printf("Erro ao reabrir arquivo %s\n", arquivo);
        liberar_diretorio(dir);
        return 1;
    }

    escrever_diretorio_em_arquivo(f, dir);

    fclose(f);
    liberar_diretorio(dir);

    printf("Membro \"%s\" movido com sucesso.\n", membro);
    return 0;
}*/

/* Função para extrair membros 
int extrair_membros(const char *arquivo, char **nomes, int num_nomes) {
    diretorio_vinac_t *dir = carregar_diretorio(arquivo);
    if (!dir) {
        printf("Erro ao carregar arquivo %s\n", arquivo);
        return 1;
    }

    FILE *f = fopen(arquivo, "rb");
    if (!f) {
        printf("Erro ao abrir o arquivo %s\n", arquivo);
        liberar_diretorio(dir);
        return 1;
    }

    entrada_no_t *atual = dir->inicio;
    while (atual) {
        int extrair = 0;
        if (num_nomes == 0) {
            extrair = 1;
        } else {
            for (int i = 0; i < num_nomes; i++) {
                if (strcmp(atual->entrada.nome, nomes[i]) == 0) {
                    extrair = 1;
                    break;
                }
            }
        }

        if (extrair) {
            FILE *out = fopen(atual->entrada.nome, "wb");
            if (!out) {
                printf("Erro ao criar arquivo %s\n", atual->entrada.nome);
                atual = atual->prox;
                continue;
            }

            fseek(f, atual->entrada.offset, SEEK_SET);
            if (atual->entrada.comprimido) {
                unsigned char *comp = malloc(atual->entrada.tamanho_disco);
                unsigned char *orig = malloc(atual->entrada.tamanho_orig);

                fread(comp, 1, atual->entrada.tamanho_disco, f);
                LZ_Uncompress(comp, orig, atual->entrada.tamanho_disco);
                fwrite(orig, 1, atual->entrada.tamanho_orig, out);

                free(comp);
                free(orig);
            } else {
                uint64_t restante = atual->entrada.tamanho_disco;
                unsigned char buffer[TAMANHO_BUFFER];
                while (restante > 0) {
                    size_t ler = restante < TAMANHO_BUFFER ? restante : TAMANHO_BUFFER;
                    fread(buffer, 1, ler, f);
                    fwrite(buffer, 1, ler, out);
                    restante -= ler;
                }
            }

            fclose(out);
            printf("Extraído: %s\n", atual->entrada.nome);
        }

        atual = atual->prox;
    }

    fclose(f);
    liberar_diretorio(dir);
    return 0;
}*

int remover_membros(const char *arquivo, char **nomes, int num) {
    diretorio_vinac_t *dir = carregar_diretorio(arquivo);
    if (!dir) {
        printf("Erro ao carregar arquivo %s\n", arquivo);
        return 1;
    }

    int removidos = 0;
    for (int i = 0; i < num; i++) {
        if (remover_entrada(dir, nomes[i])) {
            printf("Membro %s removido.\n", nomes[i]);
            removidos++;
        } else {
            printf("Membro %s não encontrado.\n", nomes[i]);
        }
    }

    FILE *f = fopen(arquivo, "r+b");
    if (!f) {
        liberar_diretorio(dir);
        return 1;
    }

    escrever_diretorio_em_arquivo(f, dir);

    fclose(f);
    liberar_diretorio(dir);
    return (removidos == num) ? 0 : 1;
}*/

/* Função para listar o conteúdo 
void listar_conteudo(const char *arquivo) {
    diretorio_vinac_t *dir = carregar_diretorio(arquivo);
    if (!dir) {
        printf("Erro ao carregar arquivo %s\n", arquivo);
        return;
    }

    printf("Conteúdo do archive %s:\n", arquivo);
    printf("---------------------------------------------------------------\n");
    printf("%-20s %-6s %-10s %-10s %-20s %s\n", 
        "Nome", "UID", "Orig(B)", "Disco(B)", "Modificação", "Ordem");

    entrada_no_t *atual = dir->inicio;
    while (atual) {
        char data[20];
        strftime(data, sizeof(data), "%Y-%m-%d %H:%M:%S", localtime(&atual->entrada.data_mod));
        printf("%-20s %-6u %-10llu %-10llu %-20s %u\n",
            atual->entrada.nome,
            atual->entrada.uid,
            atual->entrada.tamanho_orig,
            atual->entrada.tamanho_disco,
            data,
            atual->entrada.ordem
        );
        atual = atual->prox;
    }

    liberar_diretorio(dir);
}*/