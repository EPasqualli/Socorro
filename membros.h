/*Operações das opções da main*/
#ifndef MEMBROS_H
#define MEMBROS_H

#include "diretorio.h"

void atualizar_ordem(diretorio_vinac *dir);

int membro_existe(const char *nome, diretorio_vinac *dir);

void inserir_entrada_posicao(diretorio_vinac *dir, const entrada_membro *nova_entrada, uint32_t ordem);

/*Funções auxiliares para inserção e remoção*/
diretorio_vinac *abrir_ou_criar_diretorio(const char *arquivo, FILE **fp_out, int usar_offsets_fornecidos);
entrada_membro criar_entrada_membro(const char *nome_arquivo, struct stat *info, long offset, int comprimir, uint32_t uid_usuario);
uint64_t escrever_dados(FILE *dest, FILE *orig, int comprimir, uint64_t tamanho_orig, uint8_t *buffer_tmp);
entrada_membro *processar_membro(diretorio_vinac *dir, const char *membro, int comprimir, long offset_fornecido, uint32_t *ordem_existente);

int inserir_membros(const char *arquivo, char **membros, int num_membros, int comprimir);

int mover_membro(const char *arquivo, const char *membro, const char *alvo);

int processar_extracao_comprimido(FILE *archive_f, FILE *output_f, const entrada_membro *member);
int processar_extracao_nao_comprimido(FILE *archive_f, FILE *output_f, const entrada_membro *member);
int extrair_membros(const char *arquivo, char **nomes, int num_nomes);

int remover_membros(const char *arquivo, char **nomes, int num);

void listar_conteudo(const char *arquivo); 

#endif /* MEMBROS_H */