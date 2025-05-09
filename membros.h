/*Operações das opções da main*/
#ifndef MEMBROS_H
#define MEMBROS_H

#include "arquivo.h"

/*Funções auxiliares para inserção*/
uint32_t obter_proximo_uid();
diretorio_vinac *abrir_ou_criar_diretorio(const char *arquivo, FILE **fp_out);
entrada_membro criar_entrada_membro(const char *nome_arquivo, struct stat *info, long offset, int comprimir);
uint64_t escrever_dados(FILE *dest, FILE *orig, int comprimir, uint64_t tamanho_orig, uint8_t *buffer_tmp);
void processar_membro(FILE *arquivo_vinac, diretorio_vinac *dir, const char *membro, int comprimir);
void finalizar_diretorio(FILE *arquivo_vinac, diretorio_vinac *dir);

/*Inserção com compressão e sem compressão*/
int inserir_membros(const char *arquivo, char **membros, int num_membros, int comprimir);

/* int mover_membro(const char *arquivo, const char *membro, const char *alvo);
int extrair_membros(const char *arquivo, char **nomes, int num_nomes);
int remover_membros(const char *arquivo, char **nomes, int num);
void listar_conteudo(const char *arquivo); */

#endif /* MEMBROS_H */