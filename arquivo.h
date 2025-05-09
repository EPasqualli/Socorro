/*Operações com a manipulação do diretório*/
#ifndef ARQUIVO_H
#define ARQUIVO_H

#define TAMANHO_BUFFER 4096 // Tamanho do buffer para leitura/escrita de arquivos
extern uint32_t uid_atual;

/* Estruturas de dados */
typedef struct cabecalho_vinac{
    uint8_t versao;         // Versão do formato
    uint32_t num_membros;   // Número de membros
    uint64_t offset_dir;    // Offset para área de diretório
} cabecalho_vinac;

typedef struct entrada_membro {
    char nome[1024];        // Nome do arquivo
    uint32_t uid;            // UID único
    uint64_t tamanho_orig;  // Tamanho original
    uint64_t tamanho_disco; // Tamanho armazenado (com ou sem compressão)
    time_t data_mod;        // Data de modificação
    uint32_t ordem;          // Ordem de inserção no archive
    uint64_t offset;        // Offset a partir do início do arquivo .vc
    uint8_t comprimido;      // 1 se comprimido, 0 se armazenado puro
} entrada_membro;

/* Nó da lista encadeada */
typedef struct entrada_no {
    entrada_membro membros;       // Dados reais do membro
    struct entrada_no *prox;      // Ponteiro para o próximo nó
    struct entrada_no *anterior;
} entrada_no;

/* Diretório VINAc com lista encadeada */
typedef struct diretorio_vinac{
    cabecalho_vinac cabecalho;  // Cabeçalho do arquivo
    entrada_no *inicio;          // Início da lista encadeada
    entrada_no *fim;
} diretorio_vinac;

// Protótipos das funções auxiliares
int arquivo_existe(const char *nome_arquivo);
diretorio_vinac *criar_diretorio();
uint32_t obter_maior_uid(diretorio_vinac *dir);
diretorio_vinac *carregar_diretorio(const char *nome_arquivo);
void liberar_diretorio(diretorio_vinac *dir);
entrada_no *encontrar_entrada(diretorio_vinac *dir, const char *nome);
void adicionar_entrada(diretorio_vinac *dir, const entrada_membro *nova_entrada);
void remover_entrada(diretorio_vinac *dir, const char *nome);

#endif /* ARQUIVO_H */