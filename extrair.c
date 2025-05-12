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

// Extrai um membro comprimido
int processar_extracao_comprimido(FILE *archive_f, FILE *output_f, const entrada_membro *member) {
    if (member->tamanho_disco == 0 || member->tamanho_orig == 0) {
        fprintf(stderr, "Erro: membro '%s' possui tamanho inválido (tamanho_disco=%llu, tamanho_orig=%llu).\n",
                member->nome, (unsigned long long)member->tamanho_disco,
                (unsigned long long)member->tamanho_orig);
        return 1;
    }

    if (member->tamanho_disco > UINT_MAX || member->tamanho_orig > UINT_MAX) {
        fprintf(stderr, "Erro: membro '%s' excede o limite suportado (tamanho_disco=%llu, tamanho_orig=%llu).\n",
                member->nome, (unsigned long long)member->tamanho_disco,
                (unsigned long long)member->tamanho_orig);
        return 1;
    }

    if (member->comprimido && member->tamanho_disco >= member->tamanho_orig) {
        fprintf(stderr, "Erro: membro comprimido '%s' tem tamanho_disco (%llu) >= tamanho_orig (%llu), o que é inválido.\n",
                member->nome, (unsigned long long)member->tamanho_disco,
                (unsigned long long)member->tamanho_orig);
        return 1;
    }

    unsigned char *comp_buffer = malloc(member->tamanho_disco);
    unsigned char *orig_buffer = malloc(member->tamanho_orig);
    if (!comp_buffer || !orig_buffer) {
        fprintf(stderr, "Erro: falha na alocação de memória para extrair '%s' (tamanho_disco=%llu, tamanho_orig=%llu).\n",
            member->nome, (unsigned long long)member->tamanho_disco,
            (unsigned long long)member->tamanho_orig);
        free(comp_buffer);
        free(orig_buffer);
        return 1;
    }

    size_t lido = fread(comp_buffer, 1, member->tamanho_disco, archive_f);
    if (lido != member->tamanho_disco) {
        fprintf(stderr, "Erro: leitura incompleta de '%s' — esperado %llu bytes, lido %zu.\n",
            member->nome, (unsigned long long)member->tamanho_disco, lido);
        free(comp_buffer);
        free(orig_buffer);
        return 1;
    }

    printf("Descompressão dos dados do membro compactado a ser estraído\n");
    LZ_Uncompress(comp_buffer, orig_buffer, (unsigned int)member->tamanho_disco);

    if (ferror(output_f) || feof(output_f)) {
        fprintf(stderr, "Erro: arquivo de saída em estado inválido ao extrair '%s'.\n", member->nome);
        free(comp_buffer);
        free(orig_buffer);
        return 1;
    }

    size_t escrito = fwrite(orig_buffer, 1, member->tamanho_orig, output_f);
    if (escrito != member->tamanho_orig) {
        fprintf(stderr, "Erro: escrita incompleta ao salvar '%s' — esperado %llu bytes, escrito %zu.\n",
            member->nome, (unsigned long long)member->tamanho_orig, escrito);
        free(comp_buffer);
        free(orig_buffer);
        return 1;
    }

    if (fflush(output_f) != 0 || ferror(output_f)) {
        fprintf(stderr, "Erro: falha ao finalizar escrita no arquivo de saída para '%s'.\n", member->nome);
        free(comp_buffer);
        free(orig_buffer);
        return 1;
    }

    free(comp_buffer);
    free(orig_buffer);
    return 0;
}

// Extrai um membro não comprimido
int processar_extracao_nao_comprimido(FILE *archive_f, FILE *output_f, const entrada_membro *member) {
    uint64_t restante = member->tamanho_disco;
    unsigned char buffer[TAMANHO_BUFFER];
    int erro_io = 0;

    while (restante > 0) {
        size_t bytes_para_ler = (size_t)((restante < TAMANHO_BUFFER) ? restante : TAMANHO_BUFFER);
        size_t bytes_lidos = fread(buffer, 1, bytes_para_ler, archive_f);

        if (bytes_lidos == 0) {
            if (feof(archive_f)) {
                fprintf(stderr, "Erro: fim inesperado do arquivo ao extrair '%s' (restavam %llu bytes).\n",
                    member->nome, (unsigned long long)restante);
            } else {
                fprintf(stderr, "Erro: falha na leitura de '%s': %s.\n", member->nome, strerror(errno));
            }
            erro_io = 1;
            break;
        }

        size_t bytes_escritos = fwrite(buffer, 1, bytes_lidos, output_f);
        if (bytes_escritos != bytes_lidos) {
            fprintf(stderr, "Erro: falha na escrita ao extrair '%s': %s.\n", member->nome, strerror(errno));
            erro_io = 1;
            break;
        }

        restante -= bytes_lidos;
    }

    return erro_io ? 1 : 0;
}

// Extrai membros especificados ou todos do archive
int extrair_membros(const char *arquivo, char **nomes, int num_nomes) {
    diretorio_vinac *dir = carregar_diretorio(arquivo);
    if (!dir) {
        fprintf(stderr, "Erro: não foi possível carregar o diretório do arquivo '%s'.\n", arquivo);
        return 1;
    }

    listar_conteudo(arquivo); // <-- Apenas para teste

    FILE *f = fopen(arquivo, "rb");
    if (!f) {
        fprintf(stderr, "Erro: não foi possível abrir o arquivo '%s': %s.\n", arquivo, strerror(errno));
        liberar_diretorio(dir);
        return 1;
    }

    char pasta_destino[1024] = {0};
    if (num_nomes == 0) {
        snprintf(pasta_destino, sizeof(pasta_destino), "extraidos_%s", strrchr(arquivo, '/') ? strrchr(arquivo, '/') + 1 : arquivo);
        mkdir(pasta_destino, 0755);
    }

    int extraidos = 0;
    entrada_no *atual = dir->inicio;

    while (atual) {
        int deve_extrair = 0;

        if (num_nomes == 0) {
            deve_extrair = 1;
        } else {
            for (int i = 0; i < num_nomes; i++) {
                if (strcmp(atual->membros.nome, nomes[i]) == 0) {
                    deve_extrair = 1;
                    break;
                }
            }
        }

        if (deve_extrair) {
            if (strlen(atual->membros.nome) >= 1024) {
                fprintf(stderr, "Nome de membro muito longo, ignorando: %s\n", atual->membros.nome);
                atual = atual->prox;
                continue;
            }

            char caminho_final[2050];
            if (num_nomes == 0) {
                snprintf(caminho_final, sizeof(caminho_final), "%s/%s", pasta_destino, atual->membros.nome);
                char temp[2050];
                strncpy(temp, caminho_final, sizeof(temp));
                temp[sizeof(temp) - 1] = '\0';
                for (char *p = temp + strlen(pasta_destino) + 1; *p; p++) {
                    if (*p == '/') {
                        *p = '\0';
                        mkdir(temp, 0755);
                        *p = '/';
                    }
                }
            } else {
                snprintf(caminho_final, sizeof(caminho_final), "extraido_%s", atual->membros.nome);
            }

            FILE *out = fopen(caminho_final, "wb");
            if (!out) {
                fprintf(stderr, "Erro: não foi possível criar o arquivo de saída '%s': %s.\n", caminho_final, strerror(errno));
                atual = atual->prox;
                continue;
            }

            if (fseek(f, atual->membros.offset, SEEK_SET) != 0) {
                fprintf(stderr, "Erro: falha ao posicionar no offset %llu do membro '%s'.\n", 
                    (unsigned long long)atual->membros.offset, atual->membros.nome);
                fclose(out);
                atual = atual->prox;
                continue;
            }

            int erro = 0;
            if (atual->membros.comprimido) {
                erro = processar_extracao_comprimido(f, out, &atual->membros);
            } else {
                erro = processar_extracao_nao_comprimido(f, out, &atual->membros);
            }

            fclose(out);

            if (erro) {
                fprintf(stderr, "Erro: falha na extração do membro '%s'. Removendo arquivo parcial '%s'.\n", atual->membros.nome, caminho_final);
                unlink(caminho_final);
            } else {
                struct stat st;
                if (stat(caminho_final, &st) == 0 && st.st_size != atual->membros.tamanho_orig) {
                    fprintf(stderr, "Tamanho incorreto em %s (esperado %llu, obtido %lld)\n",
                            caminho_final,
                            (unsigned long long)atual->membros.tamanho_orig,
                            (long long)st.st_size);
                    unlink(caminho_final);
                } else {
                    printf("Extração concluída com sucesso: %s\n", caminho_final);
                    extraidos++;
                }
            }
        }

        atual = atual->prox;
    }

    fclose(f);
    liberar_diretorio(dir);

    if (extraidos == 0) {
        printf("Nenhum membro foi extraído\n");
    } else {
        printf("Total de membros extraídos: %d\n", extraidos);
    }
    return 0;
}