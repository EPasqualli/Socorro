#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <time.h>
#include <stdint.h>

#include "membros.h"

void imprimir_ajuda() {
    printf("Uso: vinac <opção> <arquivo> [membro1 membro2 ...]\n");
    printf("Opções:\n");
    printf("  -ip, -p  : Insere/acrescenta membros sem compressão\n");
    printf("  -ic, -i  : Insere/acrescenta membros com compressão\n");
    printf("  -m membro alvo : Move o membro para depois do alvo (ou início se alvo é NULL)\n");
    printf("  -x       : Extrai membros do arquivo (todos se nenhum especificado)\n");
    printf("  -r       : Remove membros do arquivo\n");
    printf("  -c       : Lista o conteúdo do arquivo\n");
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        imprimir_ajuda();
        return 1;
    }

    const char *opcao = argv[1];
    const char *arquivo = argv[2];
    char **membros = &argv[3];
    int num_membros = argc - 3;

    if (strcmp(opcao, "-ip") == 0 || strcmp(opcao, "-p") == 0) {
        if (argc < 4) {
            fprintf(stderr, "Erro: Nenhum membro especificado para inserção sem compressão\n");
            imprimir_ajuda();
            return 1;
        }
        return inserir_membros(arquivo, membros, num_membros, 0);
    } 
    else if (strcmp(opcao, "-ic") == 0 || strcmp(opcao, "-i") == 0) {
        if (argc < 4) {
            fprintf(stderr, "Erro: Nenhum membro especificado para inserção com compressão\n");
            imprimir_ajuda();
            return 1;
        }
        return inserir_membros(arquivo, membros, num_membros, 1);
    }
    else if (strcmp(opcao, "-m") == 0) {
        if (argc != 5) {
            fprintf(stderr, "Erro: Uso: vinac -m <arquivo> <membro> <alvo>\n");
            imprimir_ajuda();
            return 1;
        }
        return mover_membro(arquivo, argv[3], argv[4]);
    }
    else if (strcmp(opcao, "-x") == 0) {
        return extrair_membros(arquivo, membros, num_membros);
    }
    else if (strcmp(opcao, "-r") == 0) {
        if (argc < 4) {
            fprintf(stderr, "Erro: Nenhum membro especificado para remoção\n");
            imprimir_ajuda();
            return 1;
        }
        return remover_membros(arquivo, membros, num_membros);
    }
    else if (strcmp(opcao, "-c") == 0) {
        listar_conteudo(arquivo);
        return 0;
    }
    else {
        fprintf(stderr, "Erro: Opção inválida '%s'\n", opcao);
        imprimir_ajuda();
        return 1;
    }

    return 0;
}