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

// Abre um archive existente ou cria um novo
diretorio_vinac *abrir_ou_criar_diretorio(const char *arquivo, FILE **fp_out, int usar_offsets_fornecidos) {
    if (!arquivo || !fp_out) {
        fprintf(stderr, "Erro: Parâmetros inválidos\n");
        return NULL;
    }

    *fp_out = NULL;
    diretorio_vinac *dir = NULL;

    if (diretorio_existe(arquivo)) {
        printf("\nDiretorio existente --> Carregando itens\n");
        dir = carregar_diretorio(arquivo);
        if (!dir) {
            fprintf(stderr, "Erro: Falha ao carregar diretório de '%s'\n", arquivo);
            return NULL;
        }

        *fp_out = fopen(arquivo, "r+b");
        if (!*fp_out) {
            perror("Erro ao abrir arquivo em modo r+b");
            liberar_diretorio(dir);
            return NULL;
        }

        if (!usar_offsets_fornecidos) {
            entrada_no *ultimo = dir->fim;
            if (ultimo) {
                fseek(*fp_out, ultimo->membros.offset + ultimo->membros.tamanho_disco, SEEK_SET);
            } else {
                fseek(*fp_out, sizeof(cabecalho_vinac), SEEK_SET);
            }
        }
    } else {
        printf("\nDiretorio inexistente --> Criando diretorio\n");

        dir = malloc(sizeof(diretorio_vinac));
        if (!dir) {
            fprintf(stderr, "Erro: Falha ao alocar memória para diretório\n");
            return NULL;
        }
        memset(dir, 0, sizeof(diretorio_vinac));

        dir->cabecalho.assinatura = 0x56494E4143ULL;  // "VINAC"
        dir->cabecalho.versao = 1;
        dir->cabecalho.tam_cabecalho = sizeof(cabecalho_vinac);
        dir->cabecalho.num_membros = 0;
        dir->cabecalho.offset_dir = 0;

        dir->inicio = NULL;
        dir->fim = NULL;

        *fp_out = fopen(arquivo, "wb");
        if (!*fp_out) {
            perror("Erro ao criar arquivo em modo wb");
            free(dir);
            return NULL;
        }

        if (fwrite(&dir->cabecalho, sizeof(cabecalho_vinac), 1, *fp_out) != 1) {
            fprintf(stderr, "Erro: Falha ao escrever cabeçalho inicial\n");
            fclose(*fp_out);
            *fp_out = NULL;
            free(dir);
            return NULL;
        }
    }

    return dir;
}

uint64_t escrever_dados(FILE *dest, FILE *orig, int comprimir, uint64_t tamanho_orig, uint8_t *buffer_tmp) {
    if (!dest || !orig || !buffer_tmp) {
        printf("Erro: Parâmetros inválidos\n");
        return 0;
    }

    uint64_t total_escrito = 0;

    if (!comprimir || tamanho_orig > UINT32_MAX) {
        size_t lido;
        while ((lido = fread(buffer_tmp, 1, TAMANHO_BUFFER, orig)) > 0) {
            if (fwrite(buffer_tmp, 1, lido, dest) != lido) {
                printf("Erro: Falha ao escrever dados não comprimidos\n");
                return total_escrito;
            }
            total_escrito += lido;
        }
        return total_escrito;
    }

    unsigned char *buffer_entrada = malloc(TAMANHO_BUFFER);
    unsigned char *buffer_comp = malloc(TAMANHO_BUFFER + (TAMANHO_BUFFER / 256) + 1);
    if (!buffer_entrada || !buffer_comp) {
        printf("Erro: Falha ao alocar memória para compressão\n");
        free(buffer_entrada);
        free(buffer_comp);
        return total_escrito;
    }

    memset(buffer_comp, 0, TAMANHO_BUFFER + (TAMANHO_BUFFER / 256) + 1); // <- necessário

    uint64_t total_lido = 0;
    while (total_lido < tamanho_orig) {
        size_t a_ler = tamanho_orig - total_lido < TAMANHO_BUFFER ? tamanho_orig - total_lido : TAMANHO_BUFFER;

        memset(buffer_entrada, 0, TAMANHO_BUFFER);
        size_t lido = fread(buffer_entrada, 1, a_ler, orig);
        if (lido == 0) {
            printf("Aviso: Fim de arquivo atingido prematuramente\n");
            break;
        }

        if (lido > UINT32_MAX) {
            printf("Erro: Bloco de dados excede limite para compressão (%zu > %u)\n", lido, UINT32_MAX);
            free(buffer_entrada);
            free(buffer_comp);
            return total_escrito;
        }

        unsigned int tam_comp = LZ_Compress(buffer_entrada, buffer_comp, (unsigned int)lido);
        if (tam_comp >= lido) {
            printf("Aviso: Compressão não vantajosa (tam_comp=%u, lido=%zu), escrevendo sem compressão\n", tam_comp, lido);

            if (fwrite(buffer_entrada, 1, lido, dest) != lido) {
                printf("Erro: Falha ao escrever dados não comprimidos\n");
                free(buffer_entrada);
                free(buffer_comp);
                return total_escrito;
            }
            total_escrito += lido;
        } else {
            printf("Aviso: Compressão vantajosa (tam_comp=%u, lido=%zu)\n", tam_comp, lido);

            if (fwrite(buffer_comp, 1, tam_comp, dest) != tam_comp) {
                printf("Erro: Falha ao escrever dados comprimidos\n");
                free(buffer_entrada);
                free(buffer_comp);
                return total_escrito;
            }
            total_escrito += tam_comp;
        }
        total_lido += lido;
    }

    free(buffer_entrada);
    free(buffer_comp);
    return total_escrito;
}

// Insere uma entrada de membro em uma ordem especificada ou no final
void inserir_entrada_posicao(diretorio_vinac *dir, const entrada_membro *nova_entrada, uint32_t ordem) {
    entrada_no *novo = malloc(sizeof(entrada_no));
    if (!novo) {
        printf("Erro: Falha ao alocar memória para nova entrada\n");
        return;
    }
    novo->membros = *nova_entrada;
    novo->prox = NULL;
    novo->anterior = NULL;

    if (!dir->inicio) {
        dir->inicio = dir->fim = novo;
        dir->cabecalho.num_membros++;
        return;
    }

    if (ordem == UINT32_MAX || ordem >= dir->cabecalho.num_membros) {
        novo->anterior = dir->fim;
        dir->fim->prox = novo;
        dir->fim = novo;
        dir->cabecalho.num_membros++;
        return;
    }

    entrada_no *atual = dir->inicio;
    uint32_t pos = 0;
    while (atual && pos < ordem) {
        atual = atual->prox;
        pos++;
    }

    if (atual) {
        novo->prox = atual;
        novo->anterior = atual->anterior;
        if (atual->anterior) {
            atual->anterior->prox = novo;
        } else {
            dir->inicio = novo;
        }
        atual->anterior = novo;
        dir->cabecalho.num_membros++;
    }
}

// Função principal de inserção de membros não comprimidos ou comprimidos
int inserir_membros(const char *arquivo, char **membros, int num_membros, int comprimir) {
    FILE *fp = NULL;
    diretorio_vinac *dir = abrir_ou_criar_diretorio(arquivo, &fp, 0);
    if (!dir || !fp) {
        printf("Erro: Não foi possível abrir ou criar o arquivo '%s'\n", arquivo);
        if (dir) 
            liberar_diretorio(dir);
        return 1;
    }

    typedef struct entrada_info {
        entrada_membro *entrada;
        uint32_t ordem;
    } entrada_info;

    entrada_info *novas_entradas = malloc(num_membros * sizeof(entrada_info));
    
    if (!novas_entradas) {
        printf("Erro: Falha ao alocar memória para novas entradas\n");
        fclose(fp);
        liberar_diretorio(dir);
        return 1;
    }

    int num_novas = 0;
    for (int i = 0; i < num_membros; i++) {
        if (membro_existe(membros[i], dir)) {
            printf("Aviso: Membro '%s' já existe no diretório, será substituído.\n", membros[i]);
        }
        uint32_t ordem_existente;
        entrada_membro *entrada = processar_membro(dir, membros[i], comprimir, -1, &ordem_existente);
        if (entrada) {
            novas_entradas[num_novas].entrada = entrada;
            novas_entradas[num_novas].ordem = ordem_existente;
            num_novas++;
        }
    }

    char temp_file[] = "temp_vinac_XXXXXX";
    int fd = mkstemp(temp_file);
    if (fd == -1) {
        printf("Erro: Não foi possível criar arquivo temporário\n");
        for (int i = 0; i < num_novas; i++) free(novas_entradas[i].entrada);
        free(novas_entradas);
        fclose(fp);
        liberar_diretorio(dir);
        return 1;
    }

    FILE *temp = fdopen(fd, "wb");
    if (!temp) {
        close(fd);
        unlink(temp_file);
        for (int i = 0; i < num_novas; i++) free(novas_entradas[i].entrada);
        free(novas_entradas);
        fclose(fp);
        liberar_diretorio(dir);
        return 1;
    }

    fwrite(&dir->cabecalho, sizeof(cabecalho_vinac), 1, temp);

    uint64_t novo_offset = sizeof(cabecalho_vinac);
    uint8_t buffer[TAMANHO_BUFFER];
    entrada_no *atual = dir->inicio;

    while (atual) {
        atual->membros.offset = novo_offset;

        int substituido = 0;
        for (int i = 0; i < num_novas; i++) {
            if (strcmp(atual->membros.nome, novas_entradas[i].entrada->nome) == 0) {
                substituido = 1;
                break;
            }
        }

        if (!substituido) {
            fseek(fp, atual->membros.offset, SEEK_SET);
            uint64_t bytes_restantes = atual->membros.tamanho_disco;
            while (bytes_restantes > 0) {
                size_t a_ler = bytes_restantes < TAMANHO_BUFFER ? bytes_restantes : TAMANHO_BUFFER;
                size_t lido = fread(buffer, 1, a_ler, fp);
                if (lido == 0 || lido != a_ler) {
                    printf("Erro: Falha ao ler dados do membro '%s'\n", atual->membros.nome);
                    fclose(temp);
                    unlink(temp_file);
                    for (int i = 0; i < num_novas; i++) free(novas_entradas[i].entrada);
                    free(novas_entradas);
                    fclose(fp);
                    liberar_diretorio(dir);
                    return 1;
                }
                if (fwrite(buffer, 1, lido, temp) != lido) {
                    printf("Erro: Falha ao escrever dados do membro '%s'\n", atual->membros.nome);
                    fclose(temp);
                    unlink(temp_file);
                    for (int i = 0; i < num_novas; i++) free(novas_entradas[i].entrada);
                    free(novas_entradas);
                    fclose(fp);
                    liberar_diretorio(dir);
                    return 1;
                }
                bytes_restantes -= lido;
            }
            novo_offset += atual->membros.tamanho_disco;
        }
        atual = atual->prox;
    }

    for (int i = 0; i < num_novas; i++) {
        entrada_membro *entrada = novas_entradas[i].entrada;
        FILE *fp_membro = fopen(entrada->nome, "rb");
        if (!fp_membro) {
            printf("Erro: Não foi possível abrir o arquivo '%s'\n", entrada->nome);
            fclose(temp);
            unlink(temp_file);
            for (int j = 0; j < num_novas; j++) free(novas_entradas[j].entrada);
            free(novas_entradas);
            fclose(fp);
            liberar_diretorio(dir);
            return 1;
        }

        entrada->offset = novo_offset;
        entrada->tamanho_disco = escrever_dados(temp, fp_membro, entrada->comprimido, entrada->tamanho_orig, buffer);
        entrada->comprimido = (entrada->tamanho_disco < entrada->tamanho_orig && entrada->tamanho_orig <= UINT32_MAX) ? 1 : 0;

        inserir_entrada_posicao(dir, entrada, UINT32_MAX);;
        fclose(fp_membro);
        novo_offset += entrada->tamanho_disco;
        printf("Arquivo '%s' adicionado com sucesso.\n", entrada->nome);
        free(entrada);
        novas_entradas[i].entrada = NULL; 
    }

    free(novas_entradas);
    atualizar_ordem(dir);
    finalizar_diretorio(temp, dir);

    fclose(fp);
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