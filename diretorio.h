#ifndef DIRETORIO_H
#define DIRETORIO_H

#define NOME_MAXIMO 1024  /* Tamanho máximo do nome do arquivo */
#define MAX_MEMBROS 1000  /* Número máximo de membros no arquivo .vc */
#define TAMANHO_BUFFER 8192 /* Tamanho do buffer para leitura/escrita */
#define VINAC_SIGNATURE 0x56494E4143ULL /* Assinatura "VINAC" em uint64_t */

extern uint32_t uid_atual;

#include <stdint.h>
#include <time.h>
#include <errno.h>

/* Estruturas de dados */
typedef struct cabecalho_vinac {
    uint64_t assinatura;    // Assinatura do formato ("VINAC")
    uint8_t versao;         // Versão do formato
    uint32_t num_membros;   // Número de membros
    uint64_t offset_dir;    // Offset para área de diretório (será atualizado no final)
    uint64_t tam_cabecalho; // Tamanho fixo do cabeçalho
} cabecalho_vinac;

typedef struct entrada_membro {
    char nome[NOME_MAXIMO]; // Nome do arquivo
    uint32_t uid;           // UID único
    uint64_t tamanho_orig;  // Tamanho original
    uint64_t tamanho_disco; // Tamanho armazenado (com ou sem compressão)
    time_t data_mod;        // Data de modificação
    uint32_t ordem;         // Ordem de inserção no archive
    uint64_t offset;        // Offset a partir do início do arquivo .vc
    uint8_t comprimido;     // 1 se comprimido, 0 se armazenado puro
} entrada_membro;

/* Nó da lista encadeada */
typedef struct entrada_no {
    entrada_membro membros;       // Dados reais do membro
    struct entrada_no *prox;      // Ponteiro para o próximo nó
    struct entrada_no *anterior;  // Ponteiro para o anterior
} entrada_no;

/* Diretório VINAc com lista encadeada */
typedef struct diretorio_vinac {
    cabecalho_vinac cabecalho;  // Cabeçalho do arquivo
    entrada_no *inicio;         // Início da lista encadeada
    entrada_no *fim;            // Fim da lista
} diretorio_vinac;

// Protótipos das funções auxiliares
int diretorio_existe(const char *nome_diretorio);
diretorio_vinac *criar_diretorio();
uint32_t obter_maior_uid(diretorio_vinac *dir);
diretorio_vinac *carregar_diretorio(const char *nome_arquivo);
void liberar_diretorio(diretorio_vinac *dir);
entrada_no *encontrar_entrada(diretorio_vinac *dir, const char *nome);
void adicionar_entrada(diretorio_vinac *dir, const entrada_membro *nova_entrada);
int remover_entrada(diretorio_vinac *dir, const char *nome);
void finalizar_diretorio(FILE *f, diretorio_vinac *dir) ;

#endif /* DIRETORIO_H */