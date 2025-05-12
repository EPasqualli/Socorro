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

//  Lista os conteúdos do archive com detalhes dos membros
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