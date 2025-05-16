/*Operações das opções da main*/
#ifndef MEMBROS_H
#define MEMBROS_H

#include "diretorio.h"

// --------------------- Funções de membros.c ---------------------
// Atualiza o campo ordem de cada membro na lista encadeada
void atualizar_ordem(diretorio_vinac *dir);
// Verifica se um membro existe no archive
int membro_existe(const char *nome, diretorio_vinac *dir);
// Cria uma entrada de membro a partir de metadados do arquivo
entrada_membro criar_entrada_membro(const char *nome_arquivo, struct stat *info, long offset, int comprimir, uint32_t uid_usuario);
// Processa um membro para inserção, lidando com substituições
entrada_membro *processar_membro(diretorio_vinac *dir, const char *membro, int comprimir, long offset_fornecido, uint32_t *ordem_existente);


// --------------------- Funções de inserir.c ---------------------
// Abre um archive existente ou cria um novo
diretorio_vinac *abrir_ou_criar_diretorio(const char *arquivo, FILE **fp_out, int usar_offsets_fornecidos);
uint64_t escrever_dados(FILE *dest, FILE *orig, int comprimir, uint64_t tamanho_orig, uint8_t *buffer_tmp);
// Insere uma entrada de membro em uma ordem especificada ou no final.
void inserir_entrada_posicao(diretorio_vinac *dir, const entrada_membro *nova_entrada, uint32_t ordem);
// Função principal de inserção de membros não comprimidos ou comprimidos
int inserir_membros(const char *arquivo, char **membros, int num_membros, int comprimir);


// --------------------- Função de mover.c ---------------------
// Move um membro para após um membro-alvo ou para o início
int mover_membro(const char *arquivo, const char *membro, const char *alvo);


// --------------------- Funções de extrair.c ---------------------
// Extrai um membro comprimido
int processar_extracao_comprimido(FILE *archive_f, FILE *output_f, const entrada_membro *member);
// Extrai um membro não comprimido
int processar_extracao_nao_comprimido(FILE *archive_f, FILE *output_f, const entrada_membro *member);
// Extrai membros especificados ou todos do archive
int extrair_membros(const char *arquivo, char **nomes, int num_nomes);


// --------------------- Função de remover.c ---------------------
// Remove membros especificados do archive
int remover_membros(const char *arquivo, char **nomes, int num);


// --------------------- Função de listar.c ---------------------
//  Lista os conteúdos do archive com detalhes dos membros
void listar_conteudo(const char *arquivo); 

#endif /* MEMBROS_H */