// Implementação do protocolo da camada de aplicação

#include "application_layer.h"  
#include "link_layer.h"         


#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <termios.h>
#include <unistd.h>


#define FILESZ 0                // Índice para tamanhos de ficheiro
#define FILENM 1                // Índice para nomes de ficheiro
#define DATA 2                  // Índice para dados
#define START 1                 // Identificador para pacote de controlo de início
#define END 3                   // Identificador para pacote de controlo de fim



// Função principal da camada de aplicação
void applicationLayer(const char *serialPort, const char *role, int baudRate,
                      int nTries, int timeout, const char *filename) {
    printf("Initializing applicationLayer with role: %s, baudRate: %d, filename: %s\n", role, baudRate, filename);

    LinkLayer layer; // Cria uma instância da camada de ligação
    strcpy(layer.serialPort, serialPort); // Define a porta série
    printf("Role first character: %c\n", role[0]);
    
    // Configura o Role (transmissor ou receptor)
    switch (role[0]) {
        case 't':
            if (strcmp(role, "tx") == 0) {
                layer.role = LlTx; // Define como transmissor
                printf("Role set to LlTx (Transmitter)\n");
            } else {
                printf("Invalid role: %s\n", role); // Role inválido
                return; // Sai da função
            }
            break;
        case 'r':
            if (strcmp(role, "rx") == 0) {
                layer.role = LlRx; // Define como receptor
                printf("Role set to LlRx (Receiver)\n");
            } else {
                printf("Invalid role: %s\n", role); // Role inválido
                return; // Sai da função
            }
            break;
        default:
            printf("Invalid role: %s\n", role); // Role inválido
            return; // Sai da função
    }
    layer.baudRate = baudRate; // Define a taxa de baud
    layer.nRetransmissions = nTries; // Define o número de tentativas
    layer.timeout = timeout; // Define o tempo limite

    printf("Opening connection...\n");
    int fd = llopen(layer); // Abre a conexão com a camada de ligação
    if (fd < 0) {
        printf("Connection error. fd: %d\n", fd); // Erro ao abrir a conexão
        exit(-1); // Sai com erro
    }
    printf("Connection opened successfully.\n");

    // Verifica se o Role é de receptor
    switch (layer.role) {
        case LlRx: {
            size_t pcktsz;
            char recframe[255]; // Buffer para o quadro recebido
            unsigned char *buf = (unsigned char*) malloc(MAX_PAYLOAD_SIZE); // Buffer para os dados lidos
            printf("Momento Antes de começar a ler START.\n");

            // Lê o pacote de controlo de início
            if(readctrlpckt(START, buf, &pcktsz, recframe) < 0){
                printf("Error reading start control packet.\n"); // Erro ao ler o pacote
                exit(-1); // Sai com erro
            }

            printf("Momento Depois de ler START.\n");

            // Abre o ficheiro para escrita
            FILE *recfile = fopen((char*)filename, "wb+");
            if(recfile == NULL){
                printf("Error opening file.\n"); // Erro ao abrir o ficheiro
                exit(-1); // Sai com erro
            }

            printf("File opened successfully.\n");
            free(buf); // Liberta o buffer

            int dtsz;
            // Lê os pacotes de dados até que um pacote de fim seja recebido
            while((dtsz = llread(buf)) >= 0){
                if(dtsz == 0){
                    continue; // Ignora pacotes vazios
                }
                if(buf[0] == END){ // Se o pacote for de fim
                    printf("End control packet recebido.A terminar...\n");
                    break; // Sai do ciclo
                }
                else {
                    printf("Data packet of size %d received and written to file.\n", dtsz - 3); // Pacote de dados recebido
                    fwrite(buf + 3, 1, buf[1] * 256 + buf[2], recfile); // Escreve os dados no ficheiro
                }
            }

            free(buf); // Liberta o buffer
            fclose(recfile); // Fecha o ficheiro
            printf("Ficheiro recebido e salvo.\n");
            printf("Starting closing connection.\n");
            // Fecha a conexão
            if(llclose(TRUE) < 0){
                printf("Error closing connection.\n"); // Erro ao fechar a conexão
                exit(-1); // Sai com erro
            }
            printf("Connection closed.\n");
            break;
        }
        case LlTx: {
            printf("Entered LlTx case\n");
            // Abre o ficheiro para leitura
            FILE *filetrans = fopen(filename, "rb");
            if(filetrans == NULL){
                printf("Erro ao abrir ficheiro.\n"); // Erro ao abrir o ficheiro
                exit(-1); // Sai com erro
            }

            printf("Ficheiro aberto com sucesso.\n");

            int pre = ftell(filetrans); // Guarda a posição atual no ficheiro
            fseek(filetrans, 0L, SEEK_END); // Move para o final do ficheiro
            long int filesz = ftell(filetrans) - pre; // Calcula o tamanho do ficheiro
            fseek(filetrans, pre, SEEK_SET); // Retorna à posição inicial
            printf("A enviar START PCKT\n");

            // Envia o pacote de controlo de início
            if(sendCtrlPckt(START, filename, filesz) < 0){
                printf("Erro ao enviar start control packet.\n"); // Erro ao enviar o pacote
                exit(-1); // Sai com erro
            }

            printf("Start control packet enviado.\n");

            unsigned char *buf = (unsigned char*) malloc(MAX_PAYLOAD_SIZE-3); // Aloca buffer para os dados
            int dtsz;
            // Lê os dados do ficheiro e envia pacotes de dados
            while ((dtsz = fread(buf, 1, MAX_PAYLOAD_SIZE-3, filetrans)) > 0){
                if(sendDtPckt(buf, dtsz) < 0){
                    printf("Erro ao enviar data packet.\n"); // Erro ao enviar o pacote
                    exit(-1); // Sai com erro
                }
            }

            fclose(filetrans); // Fecha o ficheiro
            printf("Depois de todos dos dados enviados.\n");

            // Envia o pacote de controlo de fim
            if(sendCtrlPckt(END, filename, filesz) < 0){
                printf("Erro ao enviar end control packet.\n"); // Erro ao enviar o pacote
                exit(-1); // Sai com erro
            }

            printf("End control packet enviado.\n");

            // Fecha a conexão
            if(llclose(TRUE) < 0){
                printf("Erro ao fechar conexão.\n"); // Erro ao fechar a conexão
                exit(-1); // Sai com erro
            }
            break;
        }
        default:
            printf("Role desconhecido.\n"); // Role desconhecido
            exit(-1); // Sai com erro
            break;
    }
}


// Função para enviar um pacote de controlo
int sendCtrlPckt(int some, const char* fileName, long int length){
    // Calcula o número de bits necessários para representar o comprimento do ficheiro
    int numB = sizeof(int) * 8 - __builtin_clz(length);
    size_t filelength = (numB + 7) / 8; // Tamanho do comprimento do ficheiro em bytes
    size_t fileNMlength = strlen(fileName) + 1; // Tamanho do nome do ficheiro
    long int cpsz = 3 + filelength + 2 + fileNMlength; // Tamanho total do pacote

    // Aloca memória para o pacote de controlo
    unsigned char* ctrlPckt = (unsigned char *)malloc(cpsz);

    int d = 0;
    ctrlPckt[d++] = some; // Adiciona o identificador do pacote
    ctrlPckt[d++] = 0;     // Adiciona um byte reservado (inicializado a 0)
    memcpy(ctrlPckt + d, &filelength, sizeof(size_t)); // Copia o tamanho do ficheiro
    d += sizeof(size_t);
    ctrlPckt[d++] = 1;     // Indica que o próximo campo será o nome do ficheiro
    memcpy(ctrlPckt + d, fileName, fileNMlength); // Copia o nome do ficheiro

    // Envia o pacote de controlo através da camada de ligação
    if(llwrite(ctrlPckt, cpsz) < 0){
        printf("Erro ao enviar control packet.\n"); // Erro ao enviar o pacote
        free(ctrlPckt);
        return -1; // Retorna erro
    }

    free(ctrlPckt); // Liberta a memória alocada para o pacote de controlo
    return 1; // Retorna sucesso
}

// Função para ler um pacote de controlo
int readctrlpckt(unsigned char control, unsigned char* buf, size_t* file_size, char* filename){
    int bufsz;
    // Lê o pacote através da camada de ligação
    if( (bufsz = llread(buf)) < 0 ){
        printf("Erro ao ler control packet.\n"); // Erro ao ler o pacote
        return -1; // Retorna erro
    }

    // Verifica se o tipo de pacote lido é o esperado
    if (buf[0] != control){
        printf("Erro: Control packet desconhecido.\n"); // Pacote não reconhecido
        return -1; // Retorna erro
    }

    int d = 1;
    unsigned char t;
    // Processa o conteúdo do pacote
    while (d < bufsz){
        t = buf[d++]; // Lê o próximo byte
        if (t == 0){ // Se for 0, indica que segue o tamanho do ficheiro
            printf("Size Check \n");
            *file_size = buf[d]; // Armazena o tamanho do ficheiro
            d += sizeof(size_t);
        }
        else if(t == 1){ // Se for 1, indica que segue o nome do ficheiro
            printf ("Name Check \n");
            *filename = buf[d]; // Armazena o nome do ficheiro
            d += *filename;
        }
        else {
            printf("Erro: Control packet desconhecido.\n"); // Pacote não reconhecido
            return -1; // Retorna erro
        }
    }
    return 1; // Retorna sucesso
}

// Função para enviar um pacote de dados
int sendDtPckt(unsigned char* data, int dataSize){
    size_t pcktSz = dataSize + 3; // Tamanho do pacote, incluindo cabeçalho

    // Aloca memória para o pacote de dados
    unsigned char* pckt = (unsigned char *)malloc(pcktSz);

    // Preenche o cabeçalho do pacote
    pckt[0] = DATA; // Tipo de pacote
    pckt[1] = (unsigned char) ((dataSize >> 8) & 0xFF); // Primeiro byte do tamanho
    pckt[2] = (unsigned char) (dataSize & 0xFF); // Segundo byte do tamanho

    memcpy(pckt + 3, data, dataSize); // Copia os dados para o pacote

    // Envia o pacote através da camada de ligação
    if(llwrite(pckt, pcktSz) < 0){
        printf("Erro ao enviar data packet.\n"); // Erro ao enviar o pacote
        return -1; // Retorna erro
    }

    return 0; // Retorna sucesso
}