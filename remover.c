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

// Remove membros especificados do archive
int remover_membros(const char *arquivo, char **nomes, int num) {
    diretorio_vinac *dir = carregar_diretorio(arquivo);
    if (!dir) {
        printf("Arquivo vazio, nunhum mebro a remover %s\n", arquivo);
        return 1;
    }

    int removidos = 0;
    for (int i = 0; i < num; i++) {
        if (remover_entrada(dir, nomes[i])) {
            printf("Membro '%s' removido.\n", nomes[i]);
            removidos++;
        } else {
            printf("Membro '%s' não encontrado.\n", nomes[i]);
        }
    }

    if (removidos == 0) {
        liberar_diretorio(dir);
        return 0;
    }

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

    entrada_no *atual = dir->inicio;
    uint64_t novo_offset = sizeof(cabecalho_vinac);
    uint8_t buffer[TAMANHO_BUFFER];

    while (atual) {
        uint64_t offset_original = atual->membros.offset;

        if (fseek(orig, offset_original, SEEK_SET) != 0) {
            printf("Erro: Falha ao posicionar no offset %" PRIu64 " do membro '%s'\n", 
                offset_original, atual->membros.nome);
            fclose(orig);
            fclose(temp);
            unlink(temp_file);
            liberar_diretorio(dir);
            return 1;
        }

        uint64_t bytes_restantes = atual->membros.tamanho_disco;
        while (bytes_restantes > 0) {
            size_t a_ler = bytes_restantes < TAMANHO_BUFFER ? bytes_restantes : TAMANHO_BUFFER;
            size_t lido = fread(buffer, 1, a_ler, orig);
            if (lido == 0 || lido != a_ler) {
                printf("Erro: Falha ao ler dados do membro '%s' (lido: %zu, esperado: %zu)\n",
                    atual->membros.nome, lido, a_ler);
                fclose(orig);
                fclose(temp);
                unlink(temp_file);
                liberar_diretorio(dir);
                return 1;
            }
            if (fwrite(buffer, 1, lido, temp) != lido) {
                printf("Erro: Falha ao escrever dados do membro '%s'\n", atual->membros.nome);
                fclose(orig);
                fclose(temp);
                unlink(temp_file);
                liberar_diretorio(dir);
                return 1;
            }
            bytes_restantes -= lido;
        }

        atual->membros.offset = novo_offset;
        novo_offset += atual->membros.tamanho_disco;
        atual = atual->prox;
    }

    atualizar_ordem(dir);

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
    return 0;
}