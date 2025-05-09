/* vinac.c - Arquivo principal do arquivador VINAc */
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
    printf("  -m membro: Move o membro para depois do alvo especificado\n");
    printf("  -x       : Extrai membros do arquivo\n");
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
    
    // Verificar opção e chamar a função correspondente
    if (strcmp(opcao, "-ip") == 0 || strcmp(opcao, "-p") == 0) {
        if (argc < 4) {
            printf("Erro: Nenhum membro especificado para inserção\n");
            return 1;
        }
        return inserir_membros(arquivo, membros, num_membros, 0);
    } 
    else if (strcmp(opcao, "-ic") == 0 || strcmp(opcao, "-i") == 0) {
        if (argc < 4) {
            printf("Erro: Nenhum membro especificado para inserção\n");
            return 1;
        }
        return inserir_membros(arquivo, membros, num_membros, 1);
    }
    else if (strcmp(opcao, "-m") == 0) {
        if (argc < 5) {
            printf("Erro: Uso: vinac -m <arquivo> <membro> <alvo>\n");
            return 1;
        }
        printf("\nFuncionalidade em desenvolvimento\n");
        //return mover_membro(arquivo, argv[3], argv[4]);
    }
    else if (strcmp(opcao, "-x") == 0) {
        printf("\nFuncionalidade em desenvolvimento\n");
        //return extrair_membros(arquivo, argv + 3, argc - 3);
    }
    else if (strcmp(opcao, "-r") == 0) {
        if (argc < 4) {
            printf("Erro: Nenhum membro especificado para remoção\n");
            return 1;
        }
        printf("\nFuncionalidade em desenvolvimento\n");
        //return remover_membros(arquivo, membros, num_membros);
    }
    else if (strcmp(opcao, "-c") == 0) {
        printf("\nFuncionalidade em desenvolvimento\n");
        //return listar_conteudo(arquivo);
    }
    else {
        printf("Opção inválida: %s\n", opcao);
        imprimir_ajuda();
        return 1;
    }

    return 0;
}