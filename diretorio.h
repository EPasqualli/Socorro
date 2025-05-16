#ifndef DIRETORIO_H
#define DIRETORIO_H

#define NOME_MAXIMO 1024  
#define TAMANHO_BUFFER 8192
#define VINAC_SIGNATURE 0x56494E4143ULL 

extern uint32_t uid_atual;

#include <stdint.h>
#include <time.h>
#include <errno.h>

// Estruturas de dados
typedef struct cabecalho_vinac {
    uint64_t assinatura;    
    uint8_t versao;      
    uint32_t num_membros;   
    uint64_t offset_dir;   
    uint64_t tam_cabecalho; 
} cabecalho_vinac;

typedef struct entrada_membro {
    char nome[NOME_MAXIMO]; 
    uint32_t uid;           
    uint64_t tamanho_orig;  
    uint64_t tamanho_disco; 
    time_t data_mod;        
    uint32_t ordem;        
    uint64_t offset;       
    uint8_t comprimido;     
} entrada_membro;

// Nó da lista encadeada
typedef struct entrada_no {
    entrada_membro membros;       
    struct entrada_no *prox;      
    struct entrada_no *anterior;  
} entrada_no;

// Diretório VINAc com lista encadeada 
typedef struct diretorio_vinac {
    cabecalho_vinac cabecalho; 
    entrada_no *inicio;         
    entrada_no *fim;           
} diretorio_vinac;

// ------- Funções auxiliares de acesso ao diretório -------
// Verifica se um arquivo de archive existe
int diretorio_existe(const char *nome_diretorio);
// Cria uma nova estrutura de diretório vazia
diretorio_vinac *criar_diretorio();
// Carrega o diretório de um archive existente em memória
diretorio_vinac *carregar_diretorio(const char *nome_arquivo);
// Libera a estrutura de diretório e a lista encadeada
void liberar_diretorio(diretorio_vinac *dir);
// Encontra uma entrada de membro por nome
entrada_no *encontrar_entrada(diretorio_vinac *dir, const char *nome);
// Adiciona uma entrada de membro ao final da lista encadeada
void adicionar_entrada(diretorio_vinac *dir, const entrada_membro *nova_entrada);
// Remove uma entrada de membro por nome
int remover_entrada(diretorio_vinac *dir, const char *nome);
//  Escreve o diretório e o cabeçalho atualizado no archive
void finalizar_diretorio(FILE *f, diretorio_vinac *dir) ;

#endif /* DIRETORIO_H */