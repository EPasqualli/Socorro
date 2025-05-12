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

void atualizar_ordem(diretorio_vinac *dir) {
    entrada_no *atual = dir->inicio;
    uint32_t ordem = 0;
    while (atual) {
        atual->membros.ordem = ordem++;
        atual = atual->prox;
    }
}

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

void inserir_entrada_posicao(diretorio_vinac *dir, const entrada_membro *nova_entrada, uint32_t ordem) {
    entrada_no *novo = malloc(sizeof(entrada_no));
    if (!novo) {
        printf("Erro: Falha ao alocar memória para nova entrada\n");
        return;
    }
    novo->membros = *nova_entrada;
    novo->prox = NULL;
    novo->anterior = NULL;

    // Se lista estiver vazia
    if (!dir->inicio) {
        dir->inicio = dir->fim = novo;
        dir->cabecalho.num_membros++;
        return;
    }

    // Inserir no final se ordem for UINT32_MAX ou maior que número de membros
    if (ordem == UINT32_MAX || ordem >= dir->cabecalho.num_membros) {
        novo->anterior = dir->fim;
        dir->fim->prox = novo;
        dir->fim = novo;
        dir->cabecalho.num_membros++;
        return;
    }

    // Caso contrário, inserir na posição correta
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

diretorio_vinac *abrir_ou_criar_diretorio(const char *arquivo, FILE **fp_out, int usar_offsets_fornecidos) {
    if (!arquivo || !fp_out) {
        fprintf(stderr, "Erro: Parâmetros inválidos\n");
        return NULL;
    }

    *fp_out = NULL;
    diretorio_vinac *dir = NULL;

    if (diretorio_existe(arquivo)) {
        printf("\nDiretorio existente -- carregando itens\n");
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
        printf("\nDiretorio inexistente -- criando diretorio\n");

        // Aloca e zera a estrutura
        dir = malloc(sizeof(diretorio_vinac));
        if (!dir) {
            fprintf(stderr, "Erro: Falha ao alocar memória para diretório\n");
            return NULL;
        }
        memset(dir, 0, sizeof(diretorio_vinac));  // ⚠️ Essencial para evitar lixo

        // Inicializa campos obrigatórios do cabeçalho
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

        // Escreve o cabeçalho zerado corretamente inicializado
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


entrada_membro criar_entrada_membro(const char *nome_arquivo, struct stat *info, long offset, int comprimir, uint32_t uid_usuario) {
    entrada_membro entrada;
    memset(&entrada, 0, sizeof(entrada));
    strncpy(entrada.nome, nome_arquivo, 1023);
    entrada.nome[1023] = '\0';
    entrada.uid = uid_usuario; // Usar o UID do usuário
    entrada.tamanho_orig = info->st_size;
    entrada.data_mod = info->st_mtime;
    entrada.offset = offset;
    entrada.comprimido = comprimir;
    return entrada;
}

uint64_t escrever_dados(FILE *dest, FILE *orig, int comprimir, uint64_t tamanho_orig, uint8_t *buffer_tmp) {
    if (!dest || !orig || !buffer_tmp) {
        printf("Erro: Parâmetros inválidos\n");
        return 0;
    }

    uint64_t total_escrito = 0;

    // Desativar compressão se explicitamente desativada ou se o arquivo for muito grande
    if (!comprimir || tamanho_orig > UINT32_MAX) {
        printf("Compressão desativada, copiando dados diretamente\n");
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

    printf("Tentando compressão LZ\n");
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
        // Inicializar buffer_entrada para evitar dados não inicializados
        memset(buffer_entrada, 0, TAMANHO_BUFFER);
        size_t lido = fread(buffer_entrada, 1, a_ler, orig);
        if (lido == 0) {
            printf("Aviso: Fim de arquivo atingido prematuramente\n");
            break;
        }

        // Verificar se lido é válido para LZ_Compress
        if (lido > UINT32_MAX) {
            printf("Erro: Bloco de dados excede limite para compressão (%zu > %u)\n", lido, UINT32_MAX);
            free(buffer_entrada);
            free(buffer_comp);
            return total_escrito;
        }

        // ... depois compressão
        unsigned int tam_comp = LZ_Compress(buffer_entrada, buffer_comp, (unsigned int)lido);
        if (tam_comp >= lido) {
            printf("Aviso: Compressão não vantajosa (tam_comp=%u, lido=%zu), escrevendo sem compressão\n", tam_comp, lido);
            // Escrever dados não comprimidos
            if (fwrite(buffer_entrada, 1, lido, dest) != lido) {
                printf("Erro: Falha ao escrever dados não comprimidos\n");
                free(buffer_entrada);
                free(buffer_comp);
                return total_escrito;
            }
            total_escrito += lido;
        } else {
            printf("Aviso: Compressão vantajosa (tam_comp=%u, lido=%zu)\n", tam_comp, lido);
            // Escrever dados comprimidos
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

int inserir_membros(const char *arquivo, char **membros, int num_membros, int comprimir) {
    FILE *fp = NULL;
    diretorio_vinac *dir = abrir_ou_criar_diretorio(arquivo, &fp, 0);
    if (!dir || !fp) {
        printf("Erro: Não foi possível abrir ou criar o arquivo '%s'\n", arquivo);
        // Não fechar fp, pois abrir_ou_criar_diretorio já o gerencia
        if (dir) liberar_diretorio(dir);
        return 1;
    }

    // Lista temporária para armazenar novas entradas e suas ordens
    typedef struct {
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

    // Criar arquivo temporário
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

    // Escrever cabeçalho inicial
    fwrite(&dir->cabecalho, sizeof(cabecalho_vinac), 1, temp);

    // Copiar dados existentes e adicionar novos
    uint64_t novo_offset = sizeof(cabecalho_vinac);
    uint8_t buffer[TAMANHO_BUFFER];
    entrada_no *atual = dir->inicio;

    while (atual) {
        atual->membros.offset = novo_offset;

        // Verificar se o membro está sendo substituído
        int substituido = 0;
        for (int i = 0; i < num_novas; i++) {
            if (strcmp(atual->membros.nome, novas_entradas[i].entrada->nome) == 0) {
                substituido = 1;
                break;
            }
        }

        if (!substituido) {
            // Copiar dados existentes
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

    // Escrever dados dos novos membros e inserir na posição correta
    for (int i = 0; i < num_novas; i++) {
        entrada_membro *entrada = novas_entradas[i].entrada;
        //uint32_t ordem = novas_entradas[i].ordem;
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
        // Atualizar status de compressão com base no resultado
        entrada->comprimido = (entrada->tamanho_disco < entrada->tamanho_orig && entrada->tamanho_orig <= UINT32_MAX) ? 1 : 0;

        // Inserir na posição correta
        inserir_entrada_posicao(dir, entrada, UINT32_MAX);;
        fclose(fp_membro);
        novo_offset += entrada->tamanho_disco;
        // Exibir mensagem antes de liberar a entrada
        printf("Arquivo '%s' adicionado com sucesso.\n", entrada->nome);
        // Liberar a entrada após inserção, pois inserir_entrada_posicao copia os dados
        free(entrada);
        novas_entradas[i].entrada = NULL; // Evitar dangling pointers
    }

    // Liberar memória das novas entradas
    free(novas_entradas);

    // Atualizar ordem dos membros
    atualizar_ordem(dir);

    // Escrever diretório atualizado
    finalizar_diretorio(temp, dir);

    // Fechar arquivos
    fclose(fp);
    fclose(temp);

    // Substituir arquivo original
    if (rename(temp_file, arquivo) != 0) {
        printf("Erro: Não foi possível substituir o arquivo original\n");
        unlink(temp_file);
        liberar_diretorio(dir);
        return 1;
    }

    liberar_diretorio(dir);
    return 0;
}

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
        // Use offset antigo antes de sobrescrever
        uint64_t offset_antigo = atual->membros.offset;
        uint64_t tamanho = atual->membros.tamanho_disco;

        // Ler do arquivo original
        if (fseek(orig, offset_antigo, SEEK_SET) != 0) {
            fprintf(stderr, "Erro: Falha ao posicionar no offset %llu do membro '%s'\n",
                    (unsigned long long)offset_antigo, atual->membros.nome);
            fclose(orig);
            fclose(temp);
            unlink(temp_file);
            liberar_diretorio(dir);
            return 1;
        }

        // Atualizar novo offset antes de gravar (mas após ler o antigo)
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

// Helper para extrair um membro comprimido
int processar_extracao_comprimido(FILE *archive_f, FILE *output_f, const entrada_membro *member) {
    // Verificar tamanhos
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

    // Alocar buffers
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

    // Ler dados comprimidos
    size_t lido = fread(comp_buffer, 1, member->tamanho_disco, archive_f);
    if (lido != member->tamanho_disco) {
        fprintf(stderr, "Erro: leitura incompleta de '%s' — esperado %llu bytes, lido %zu.\n",
            member->nome, (unsigned long long)member->tamanho_disco, lido);
        free(comp_buffer);
        free(orig_buffer);
        return 1;
    }

    // Descomprimir os dados
    printf("Descompressão dos dados do membro compactado a ser estraído\n");
    LZ_Uncompress(comp_buffer, orig_buffer, (unsigned int)member->tamanho_disco);

    // Verificar estado do arquivo de saída
    if (ferror(output_f) || feof(output_f)) {
        fprintf(stderr, "Erro: arquivo de saída em estado inválido ao extrair '%s'.\n", member->nome);
        free(comp_buffer);
        free(orig_buffer);
        return 1;
    }

    // Escrever os dados descomprimidos
    size_t escrito = fwrite(orig_buffer, 1, member->tamanho_orig, output_f);
    if (escrito != member->tamanho_orig) {
        fprintf(stderr, "Erro: escrita incompleta ao salvar '%s' — esperado %llu bytes, escrito %zu.\n",
            member->nome, (unsigned long long)member->tamanho_orig, escrito);
        free(comp_buffer);
        free(orig_buffer);
        return 1;
    }

    // Verificar integridade do arquivo escrito
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

// Helper para extrair um membro não comprimido
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

int extrair_membros(const char *arquivo, char **nomes, int num_nomes) {
    diretorio_vinac *dir = carregar_diretorio(arquivo);
    if (!dir) {
        fprintf(stderr, "Erro: não foi possível carregar o diretório do arquivo '%s'.\n", arquivo);
        return 1;
    }
    listar_conteudo(arquivo);

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
            // Valida nome do membro
            if (strlen(atual->membros.nome) >= 1024) {
                fprintf(stderr, "Nome de membro muito longo, ignorando: %s\n", atual->membros.nome);
                atual = atual->prox;
                continue;
            }

            char caminho_final[2050];
            if (num_nomes == 0) {
                snprintf(caminho_final, sizeof(caminho_final), "%s/%s", pasta_destino, atual->membros.nome);

                // Cria diretórios intermediários
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

void listar_conteudo(const char *arquivo) {
    diretorio_vinac *dir = carregar_diretorio(arquivo);
    if (!dir) {
        printf("Erro ao carregar arquivo %s\n", arquivo);
        return;
    }

    printf("Conteúdo do diretorio %s:\n", arquivo);
    printf("---------------------------------------------------------------------------------------------\n");
    printf("%-20s %-6s %-10s %-10s %-20s %-8s %s %-20s\n", 
           "Nome", "UID", "Orig(B)", "Disco(B)", "Modificação", "Comprimido", "Ordem", "Offset");

    entrada_no *atual = dir->inicio;
    while (atual) {
        char data[20];
        strftime(data, sizeof(data), "%Y-%m-%d %H:%M:%S", localtime(&atual->membros.data_mod));
        printf("%-20s %-6u %-10" PRIu64 " %-10" PRIu64 " %-20s %-8s %u \t %-10ld\n",
               atual->membros.nome,
               atual->membros.uid,
               atual->membros.tamanho_orig,
               atual->membros.tamanho_disco,
               data,
               atual->membros.comprimido ? "Sim" : "Não",
               atual->membros.ordem,
               atual->membros.offset);
        atual = atual->prox;
    }

    liberar_diretorio(dir);
}