#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdint.h>
#include <sys/stat.h>

#include "membros.h"

int inserir_membros(const char *arquivo, char **membros, int num_membros, int comprimir) {
    diretorio_vinac_t *dir;

    // Verificar se o arquivo existe para carregar o diretório ou criar um novo
    if (arquivo_existe(arquivo)) {
        dir = carregar_diretorio(arquivo);
        if (!dir) {
            printf("Erro ao carregar diretório de %s\n", arquivo);
            return 1;
        }
    } else {
        dir = criar_diretorio();
        if (!dir) {
            printf("Erro ao criar diretório para %s\n", arquivo);
            return 1;
        }
    }

    FILE *arquivo_vinac = fopen(arquivo, "r+b");
    if (!arquivo_vinac) {
        // Se o arquivo não existir, criar um novo
        arquivo_vinac = fopen(arquivo, "wb");
        if (!arquivo_vinac) {
            printf("Erro ao abrir o arquivo %s\n", arquivo);
            liberar_diretorio(dir);
            return 1;
        }

        // Escrever o cabeçalho inicial no novo arquivo
        dir->cabecalho.versao = 1; // Versão inicial
        dir->cabecalho.num_membros = 0;
        dir->cabecalho.offset_dir = sizeof(cabecalho_vinac); // Offset após o cabeçalho
        if (fwrite(&dir->cabecalho, sizeof(cabecalho_vinac), 1, arquivo_vinac) != 1) {
            printf("Erro ao escrever cabeçalho\n");
            fclose(arquivo_vinac);
            liberar_diretorio(dir);
            return 1;
        }
    }

    // Ir para o final do arquivo para adicionar os novos membros
    fseek(arquivo_vinac, 0, SEEK_END);

    // Buffer para armazenar os dados do arquivo membro
    unsigned char buffer[TAMANHO_BUFFER];
    size_t bytes_lidos;

    // Loop para processar cada membro a ser inserido
    for (int i = 0; i < num_membros; i++) {
        struct stat info_arquivo;

        // Obter informações do arquivo membro
        if (stat(membros[i], &info_arquivo) != 0) {
            printf("Erro ao obter informações do arquivo %s\n", membros[i]);
            continue; // Pular para o próximo membro em caso de erro
        }

        FILE *arquivo_membro = fopen(membros[i], "rb");
        if (!arquivo_membro) {
            printf("Erro ao abrir o arquivo membro %s\n", membros[i]);
            continue; // Pular para o próximo membro
        }

        // Preparar a nova entrada para o diretório
        entrada_membro nova_entrada;
        strncpy(nova_entrada.nome, membros[i], 1023); // Copiar o nome do membro
        nova_entrada.nome[1023] = '\0'; // Garantir a terminação nula
        nova_entrada.uid = obter_proximo_uid(); // Obter um novo UID
        nova_entrada.tamanho_orig = info_arquivo.st_size; // Tamanho original do arquivo
        nova_entrada.data_mod = info_arquivo.st_mtime; // Data de modificação
        nova_entrada.offset = ftell(arquivo_vinac); // Offset no arquivo .vinac
        nova_entrada.comprimido = comprimir;

        // Verificar se o membro já existe no diretório
        entrada_no *entrada_existente = encontrar_entrada(dir, nova_entrada.nome);
        if (entrada_existente) {
            // Se o membro existe, remover a entrada existente para substituir
            remover_entrada(dir, nova_entrada.nome);
            // Manter a ordem original para inserção na mesma posição
            nova_entrada.ordem = entrada_existente->membros.ordem;
        } else {
            // Se for um novo membro, adicionar ao final
            nova_entrada.ordem = dir->cabecalho.num_membros;
        }

        // Lógica de compressão (ou não) dos dados do membro
        if (comprimir) {
            // Alocar buffer para leitura do arquivo membro
            unsigned char *buffer_entrada = (unsigned char *)malloc(info_arquivo.st_size);
            if (!buffer_entrada) {
                printf("Erro ao alocar memória para comprimir %s\n", membros[i]);
                fclose(arquivo_membro);
                continue;
            }

            // Ler o arquivo membro para o buffer
            if (fread(buffer_entrada, 1, info_arquivo.st_size, arquivo_membro) != info_arquivo.st_size) {
                printf("Erro ao ler arquivo %s para compressão\n", membros[i]);
                free(buffer_entrada);
                fclose(arquivo_membro);
                continue;
            }

            // Alocar buffer para os dados comprimidos (considerando a expansão máxima)
            unsigned int tamanho_buffer_comp = (info_arquivo.st_size * 257) / 256 + 1;
            unsigned char *buffer_comprimido = (unsigned char *)malloc(tamanho_buffer_comp);
            if (!buffer_comprimido) {
                printf("Erro ao alocar memória para buffer comprimido\n");
                free(buffer_entrada);
                fclose(arquivo_membro);
                continue;
            }

            // Tentar comprimir os dados
            unsigned int tamanho_comprimido = LZ_Compress(buffer_entrada, buffer_comprimido, info_arquivo.st_size);

            // Verificar se a compressão foi vantajosa
            if (tamanho_comprimido < info_arquivo.st_size) {
                // Escrever os dados comprimidos no arquivo .vinac
                if (fwrite(buffer_comprimido, 1, tamanho_comprimido, arquivo_vinac) != tamanho_comprimido) {
                    printf("Erro ao escrever dados comprimidos\n");
                    free(buffer_entrada);
                    free(buffer_comprimido);
                    fclose(arquivo_membro);
                    continue;
                }
                nova_entrada.tamanho_disco = tamanho_comprimido;
                nova_entrada.comprimido = 1; // Marcar como comprimido
            } else {
                // Se a compressão não for vantajosa, escrever os dados originais
                fseek(arquivo_membro, 0, SEEK_SET); // Voltar ao início do arquivo membro
                nova_entrada.tamanho_disco = info_arquivo.st_size;
                nova_entrada.comprimido = 0;   // Marcar como não comprimido
                while ((bytes_lidos = fread(buffer, 1, TAMANHO_BUFFER, arquivo_membro)) > 0) {
                    if (fwrite(buffer, 1, bytes_lidos, arquivo_vinac) != bytes_lidos) {
                        printf("Erro ao escrever dados não comprimidos\n");
                        break;
                    }
                }
            }

            // Liberar a memória alocada para os buffers
            free(buffer_entrada);
            free(buffer_comprimido);
        } else {
            // Se não for para comprimir, apenas copiar os dados
            nova_entrada.tamanho_disco = info_arquivo.st_size;
            nova_entrada.comprimido = 0; // Marcar como não comprimido

            while ((bytes_lidos = fread(buffer, 1, TAMANHO_BUFFER, arquivo_membro)) > 0) {
                if (fwrite(buffer, 1, bytes_lidos, arquivo_vinac) != bytes_lidos) {
                    printf("Erro ao escrever dados não comprimidos\n");
                    break;
                }
            }
        }

        fclose(arquivo_membro);

        // Adicionar a entrada ao diretório (após a possível remoção)
        adicionar_entrada(dir, &nova_entrada);
        printf("Arquivo %s adicionado com sucesso\n", membros[i]);
    }

    // Após processar todos os membros, atualizar o diretório no arquivo .vinac
    long posicao_dir = ftell(arquivo_vinac); // Obter a posição onde o diretório será escrito
    dir->cabecalho.offset_dir = posicao_dir;  // Atualizar o offset do diretório no cabeçalho

    // Escrever o cabeçalho atualizado
    fseek(arquivo_vinac, 0, SEEK_SET);
    if (fwrite(&dir->cabecalho, sizeof(cabecalho_vinac), 1, arquivo_vinac) != 1) {
        printf("Erro ao atualizar cabeçalho\n");
        fclose(arquivo_vinac);
        liberar_diretorio(dir);
        return 1;
    }

    // Escrever o diretório (lista encadeada) no arquivo
    entrada_no *atual = dir->inicio;
    while (atual) {
        fwrite(&atual->membros, sizeof(entrada_membro), 1, arquivo_vinac);
        atual = atual->prox;
    }

    fclose(arquivo_vinac);
    liberar_diretorio(dir);
    return 0;
}


/* Função para mover um membro */
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
}

/* Função para extrair membros */
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
}

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
}

/* Função para listar o conteúdo */
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
}