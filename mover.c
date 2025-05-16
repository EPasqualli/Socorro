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
#include <string.h> // Para memset
#include <sys/types.h>
#include <unistd.h>

#include "membros.h"
#include "lz.h"

// Move um membro para após um membro-alvo ou para o início
int mover_membro(const char *arquivo, const char *membro, const char *alvo) {
    diretorio_vinac *dir = carregar_diretorio(arquivo);
    if (!dir) {
        printf("Erro ao carregar o arquivo %s\n", arquivo);
        return 1;
    }

    if (!dir->inicio) {
        printf("Erro: O arquivo %s está vazio\n", arquivo);
        liberar_diretorio(dir);
        return 1;
    }

    if (alvo && strcmp(membro, alvo) == 0) {
        printf("Erro: Membro '%s' não pode ser movido para si mesmo\n", membro);
        liberar_diretorio(dir);
        return 1;
    }

    entrada_no *movido = NULL;
    entrada_no *ant = NULL;
    entrada_no *atual = dir->inicio;

    while (atual) {
        if (strcmp(atual->membros.nome, membro) == 0) {
            movido = atual;
            if (ant) {
                ant->prox = atual->prox;
            } else {
                dir->inicio = atual->prox;
            }

            if (atual->prox) {
                atual->prox->anterior = ant;
            } else {
                dir->fim = ant;
            }

            movido->prox = NULL;
            movido->anterior = NULL;
            break;
        }
        ant = atual;
        atual = atual->prox;
    }

    if (!movido) {
        printf("Membro '%s' não encontrado\n", membro);
        liberar_diretorio(dir);
        return 1;
    }

    if (!alvo || strcmp(alvo, "NULL") == 0) {
        movido->prox = dir->inicio;
        if (dir->inicio) {
            dir->inicio->anterior = movido;
        }
        dir->inicio = movido;
        if (!dir->fim) {
            dir->fim = movido;
        }
    } else {
        entrada_no *ptr = dir->inicio;
        while (ptr && strcmp(ptr->membros.nome, alvo) != 0) {
            ptr = ptr->prox;
        }
        if (!ptr) {
            printf("Membro-alvo '%s' não encontrado\n", alvo);
            // Reinserir movido para evitar vazamento
            movido->prox = dir->inicio;
            if (dir->inicio) {
                dir->inicio->anterior = movido;
            }
            dir->inicio = movido;
            if (!dir->fim) {
                dir->fim = movido;
            }
            liberar_diretorio(dir);
            return 1;
        }
        movido->prox = ptr->prox;
        movido->anterior = ptr;
        if (ptr->prox) {
            ptr->prox->anterior = movido;
        } else {
            dir->fim = movido;
        }
        ptr->prox = movido;
    }

    atualizar_ordem(dir);

    char temp_file[] = "temp_vinac_XXXXXX";
    int fd = mkstemp(temp_file);
    if (fd == -1) {
        printf("Erro: Não foi possível criar arquivo temporário\n");
        liberar_diretorio(dir);
        return 1;
    }
    
    FILE *temp = fdopen(fd, "wb");
    if (!temp) {
        close(fd);
        unlink(temp_file);
        liberar_diretorio(dir);
        return 1;
    }

    FILE *orig = fopen(arquivo, "rb");
    if (!orig) {
        fclose(temp);
        unlink(temp_file);
        liberar_diretorio(dir);
        return 1;
    }

    fwrite(&dir->cabecalho, sizeof(cabecalho_vinac), 1, temp);

    atual = dir->inicio;
    uint64_t novo_offset = sizeof(cabecalho_vinac);
    uint8_t buffer[TAMANHO_BUFFER];

    while (atual) {
        uint64_t offset_antigo = atual->membros.offset;
        uint64_t tamanho = atual->membros.tamanho_disco;

        if (fseek(orig, offset_antigo, SEEK_SET) != 0) {
            fprintf(stderr, "Erro: Falha ao posicionar no offset %llu do membro '%s'\n",
                    (unsigned long long)offset_antigo, atual->membros.nome);
            fclose(orig);
            fclose(temp);
            unlink(temp_file);
            liberar_diretorio(dir);
            return 1;
        }

        atual->membros.offset = novo_offset;

        uint64_t bytes_restantes = tamanho;
        while (bytes_restantes > 0) {
            size_t a_ler = (bytes_restantes < TAMANHO_BUFFER) ? bytes_restantes : TAMANHO_BUFFER;
            size_t lido = fread(buffer, 1, a_ler, orig);
            if (lido != a_ler) {
                fprintf(stderr, "Erro: Falha ao ler membro '%s': lido %zu bytes, esperado %zu\n",
                        atual->membros.nome, lido, a_ler);
                fclose(orig);
                fclose(temp);
                unlink(temp_file);
                liberar_diretorio(dir);
                return 1;
            }

            if (fwrite(buffer, 1, lido, temp) != lido) {
                fprintf(stderr, "Erro: Falha ao escrever membro '%s'\n", atual->membros.nome);
                fclose(orig);
                fclose(temp);
                unlink(temp_file);
                liberar_diretorio(dir);
                return 1;
            }

            bytes_restantes -= lido;
        }

        novo_offset += tamanho;
        atual = atual->prox;
    }

    finalizar_diretorio(temp, dir);
    fclose(orig);
    fclose(temp);

    if (rename(temp_file, arquivo) != 0) {
        printf("Erro: Não foi possível substituir o arquivo original\n");
        unlink(temp_file);
        liberar_diretorio(dir);
        return 1;
    }

    liberar_diretorio(dir);
    printf("Membro '%s' movido com sucesso\n", membro);
    return 0;
}