// Link layer protocol implementation

#include "link_layer.h"
#include "serial_port.h"


#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <termios.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>

#define FLAG 0x7E
#define ESC 0x7D
#define ADRESSTRANS 0x03
#define ADRESSREC 0x01

#define ADRESS 0x03
#define SET 0X03
#define UA 0X07
#define BCC1 ADRESS^UA


#define STATE_START 0
#define STATE_FLAG 1
#define STATE_ADRESS 2
#define STATE_CTRL 3
#define STATE_BCC 4
#define STATE_READ 5
#define STATE_FOUND 6
#define STATE_STOP 7


#define RR_0 0xAA
#define RR_1 0xAB
#define REJ_0 0x54
#define REJ_1 0X55
#define DISC 0x0B

 
#define BUF_SIZE 5


unsigned char tramaTrans = 0; 
unsigned char tramaRec = 1;


volatile int STOP = FALSE;

int alarmEnabled = FALSE;
int alarmCount = 0;
extern int fd;
int retransmi=0;
int timeou=0;
int role;

int frameI, frameO, frameD = 0;
time_t starttime, endtime;
// MISC
#define _POSIX_SOURCE 1 // POSIX compliant source


void alarmHandler(int signal)
{
    alarmEnabled = FALSE;
    alarmCount++;

    printf("Alarm #%d\n", alarmCount);
}

////////////////////////////////////////////////
// LLOPEN
////////////////////////////////////////////////

int llopen(LinkLayer connectionParameters)
{
    printf("Opening serial port: %s with baud rate: %d\n", connectionParameters.serialPort, connectionParameters.baudRate);
    fd = openSerialPort(connectionParameters.serialPort, connectionParameters.baudRate);

    // Verifica se a porta serial foi aberta com sucesso
    if (fd < 0)
    {
        perror("Failed to open serial port");
        return -1;
    }
    printf("Serial port opened successfully. File descriptor: %d\n", fd);

    // Define o número de retransmissões e timeout a partir dos parâmetros
    retransmi = connectionParameters.nRetransmissions;
    timeou = connectionParameters.timeout;
    printf("Number of retransmissions: %d, Timeout: %d\n", retransmi, timeou);

    // Configuração do modo de Transmissor (LlTx)
    if (connectionParameters.role == LlTx) {
        printf("Setting up as transmitter (LlTx)\n");
        (void)signal(SIGALRM, alarmHandler);
        role = 0;

        // Prepara mensagem para envio
        unsigned char buf2[BUF_SIZE] = {0};
        buf2[0]= FLAG;
        buf2[1]= ADRESSTRANS;
        buf2[2]= SET;
        buf2[3]= SET ^ ADRESSTRANS;
        buf2[4]= FLAG;

        unsigned char buf[BUF_SIZE] = {0};
        int state = STATE_START;
        alarmEnabled = FALSE;

        // Envia e aguarda resposta, com limite de retransmissões
        while(connectionParameters.nRetransmissions > 0 && state != STATE_STOP) {
            if(alarmEnabled == FALSE) {
                int bytes = write(fd , buf2, BUF_SIZE);
                printf("%d bytes written\n", bytes);
                alarmEnabled = TRUE;
                alarm(connectionParameters.timeout);
                connectionParameters.nRetransmissions--;
            }

            // Lê resposta e verifica o estado de cada byte recebido
            int bytes = read(fd, &buf, 1);
            if(bytes > 0) {
               printf("Lido!\n");
               switch (state) {
                   case STATE_START:
                       if (buf[0] == FLAG) {
                           state = STATE_FLAG;
                           printf("FLAG recebido, novo estado: FLAG\n");
                       }
                       break;

                   case STATE_FLAG:
                       if(buf[0] == ADRESSREC) {
                           state = STATE_ADRESS;
                           printf("ADRESS recebido, novo estado: ADRESS\n");
                       } else if(buf[0] == FLAG) {
                           state = STATE_FLAG;
                           printf("FLAG recebido, mesmo estado\n");
                       } else {
                           state = STATE_START;
                           printf("Outro recebido, novo estado: START\n");
                       }
                       break;

                   case STATE_ADRESS:
                       if(buf[0] == UA) {
                           state = STATE_CTRL;
                           printf("UA recebido, novo estado: CTRL\n");
                       } else if(buf[0] == FLAG) {
                           state = STATE_FLAG;
                           printf("FLAG recebido, novo estado: FLAG\n");
                       } else {
                           state = STATE_START;
                           printf("Outro recebido, novo estado: START\n");
                       }
                       break;

                   case STATE_CTRL:
                       if(buf[0] == (ADRESSREC ^ UA)) {
                           state = STATE_BCC;
                           printf("BCC recebido, novo estado: BCC\n");
                       } else if(buf[0] == FLAG) {
                           state = STATE_FLAG;
                           printf("FLAG recebido, novo estado: FLAG\n");
                       } else {
                           state = STATE_START;
                           printf("Outro recebido, novo estado: START\n");
                       }
                       break;

                   case STATE_BCC:
                       if(buf[0] == FLAG) {
                           state = STATE_STOP;
                           printf("FLAG recebido, novo estado: STOP\n");
                       } else {
                           state = STATE_START;
                           printf("Outro recebido, novo estado: START\n");
                       }
                       break;
               }
           }
        }
        frameO++;
       sleep(1);
    }

    // Configuração do modo de Recetor (LlRx)
    if (connectionParameters.role == LlRx) {
        printf("Setting up as receiver (LlRx)\n");
        role = 1;
        unsigned char buf[BUF_SIZE] = {0};
        int state = STATE_START;

        // Aguarda pela receção de dados e processa cada byte
        while (state != STATE_STOP) {
            int bytes = read(fd, buf, 1);
            if(bytes > 0) {
                printf("Lido!\n");
                switch(state) {
                    case STATE_START:
                        if(buf[0] == FLAG) {
                            state = STATE_FLAG;
                            printf("FLAG recebida, novo estado: FLAG\n");
                        }
                        break;

                    case STATE_FLAG:
                        if(buf[0] == ADRESSTRANS) {
                            state = STATE_ADRESS;
                            printf("ADRESS recebido, novo estado: ADRESS\n");
                        } else if (buf[0] == FLAG) {
                            state = STATE_FLAG;
                            printf("FLAG recebida, mesmo estado\n");
                        } else {
                            state = STATE_START;
                            printf("Outro recebido novo estado: START\n");
                        }
                        break;

                    case STATE_ADRESS:
                        if(buf[0] == SET) {
                            state = STATE_CTRL;
                            printf("SET recebido, novo estado: CONTROL\n");
                        } else if(buf[0] == FLAG) {
                            state = STATE_FLAG;
                            printf("FLAG recebida, novo estado: FLAG\n");
                        } else {
                            state = STATE_START;
                            printf("Outro recebido novo estado: START\n");
                        }
                        break;

                    case STATE_CTRL:
                        if(buf[0] == (ADRESSTRANS ^ SET)) {
                            state = STATE_BCC;
                            printf("BCC recebido, novo estado: BCC\n");
                        } else if(buf[0] == FLAG) {
                            state = STATE_FLAG;
                            printf("FLAG recebida, novo estado: FLAG\n");
                        } else {
                            state = STATE_START;
                            printf("Outro recebido novo estado: START\n");
                        }
                        break;

                    case STATE_BCC:
                        if(buf[0] == FLAG) {
                            state = STATE_STOP;
                            printf("FLAG recebida, novo estado: STOP\n");
                        } else {
                            state = STATE_START;
                            printf("Outro recebido novo estado: START\n");
                        }
                        break;
                }
            }
        }

        // Envia mensagem de resposta ao transmissor
        buf[0] = FLAG;
        buf[1] = ADRESSREC;
        buf[2] = UA;
        buf[3] = ADRESSREC ^ UA;
        buf[4] = FLAG;

        int bytes = writeBytesSerialPort(buf, BUF_SIZE);
        printf("%d bytes written \n", bytes);
        frameO++;
    }

    printf("Connection established, fd: %d \n", fd);
    time(&starttime);
    return fd;
}





////////////////////////////////////////////////
// LLWRITE
////////////////////////////////////////////////
int llwrite(const unsigned char *buf, int bufSize)
{
    // Calcula o tamanho total da trama, incluindo 6 bytes adicionais para controlo e flags
    int framesz = bufSize + 6;
    unsigned char *frameinfo = (unsigned char *)malloc(framesz);

    // Configura o cabeçalho da trama com flag, endereço e tipo de controlo
    frameinfo[0] = FLAG;
    frameinfo[1] = ADRESSTRANS;
    if(tramaTrans % 2 == 0) frameinfo[2] = 0x00;  // Define controlo para trama
    else frameinfo[2] = 0x80;  // Define controlo para trama ímpar
    frameinfo[3] = frameinfo[1] ^ frameinfo[2];  // BCC1 (verificação de erros para o cabeçalho)

    // Calcula o BCC2 para o campo de dados (verificação de erros)
    unsigned char BCC2 = buf[0];
    for(int i = 1; i < bufSize; i++){
        BCC2 = BCC2 ^ buf[i];
    }

    // Insere dados no frame com stuffing (FLAG e ESC)
    int framepos = 4;
    for(int i = 0; i < bufSize; i++){
        if(buf[i] == FLAG || buf[i] == ESC){
            frameinfo = realloc(frameinfo, ++framesz);  // Realoca espaço para o stuffing
            frameinfo[framepos++] = ESC;
            frameinfo[framepos++] = buf[i] ^ 0x20;  // Aplica o byte stuffing
        }
        else{
            frameinfo[framepos++] = buf[i];
        }
    }

    // Finaliza o frame com BCC2 e FLAG
    frameinfo[framepos++] = BCC2;
    frameinfo[framepos++] = FLAG;

    int currenttran = 0;  // Contador de tentativas de retransmissão
    int accept = 0;
    alarmEnabled = FALSE;
    alarmCount = 0;

    // Loop de retransmissão até atingir o limite
    while (currenttran < retransmi){
        if(alarmEnabled == FALSE){
            write(fd, frameinfo, framesz);  // Envia o frame pela porta série
            alarmEnabled = TRUE;
            alarm(timeou);  // Define o temporizador para o tempo limite
            currenttran++;
        }
        
        // Lê e analisa a resposta de controlo recebida
        unsigned char res = readControlFrame();


        if(res == 0){
            continue;
        }
        // Verifica se a resposta foi um reconhecimento positivo (RR) ou rejeição (REJ)
        if(res == RR_0 || res == RR_1){
            accept = 1;
            tramaTrans = (tramaTrans + 1) % 2;  // Alterna o indicie do transmissor
            break;
        }
        else if(res == REJ_0 || res == REJ_1){
            alarmEnabled = FALSE;  // Reenvia o frame caso haja rejeição
        }
        else continue;
    }

    free(frameinfo);
    alarmCount = 0;
    if(accept){
        printf("Frame enviado com sucesso\n");
        frameI++;
        return framesz;
    }
    else{
        printf("O programa excedeu o número de retransmissões\n");
        return -1;
    }
}



unsigned char readControlFrame (){
    unsigned char act;
    unsigned char ictrl;
    int state = STATE_START;
    int bytes;

    // Loop para ler a trama de controlo, estado por estado
    while(state != STATE_STOP){
        bytes = read(fd, &act, 1);

        if(bytes > 0){
            switch (state)
            {
                case STATE_START:
                    if(act == FLAG){
                        state = STATE_FLAG;  // Recebeu FLAG, avança para o próximo estado
                    }
                    break;
                
                case STATE_FLAG:
                    if(act == ADRESSREC){
                        state = STATE_ADRESS;  // Recebeu o endereço, avança para o próximo estado
                    }
                    else if(act == FLAG){
                        state = STATE_FLAG;  // Continua no estado FLAG se receber outra FLAG
                    }
                    else{
                        state = STATE_START;  // Reinicia o estado se receber outro valor
                    }
                    break;

                case STATE_ADRESS:
                    if(act == RR_0 || act == RR_1 || act == REJ_0 || act == REJ_1 || act == DISC){
                        state = STATE_CTRL;  // Reconhece um controlo válido e avança
                        ictrl = act;
                    }
                    else if(act == FLAG){
                        state = STATE_FLAG;
                    }
                    else{
                        state = STATE_START;
                    }
                    break;

                case STATE_CTRL:
                    if(act == (ADRESSREC ^ ictrl)){
                        state = STATE_BCC;  // Valida BCC e avança para o próximo estado
                    }
                    else if(act == FLAG){
                        state = STATE_FLAG;
                    }
                    else{
                        state = STATE_START;
                    }
                    break;

                case STATE_BCC:
                    if(act == FLAG){
                        state = STATE_STOP;  // Recebeu FLAG final, termina o estado
                    }
                    else{
                        state = STATE_START;
                    }
                    break;
                default:
                    break;
            }
        }
        else if (bytes == 0){
            return 0;
        }
        else{
            printf("Erro ao ler frame de controlo\n");
            return -1;
        }
    }

    printf("Leitura do frame de controlo concluída, ictrl: 0x%02X\n", ictrl);
    return ictrl;  // Retorna o controlo identificado
}


////////////////////////////////////////////////
// LLREAD
////////////////////////////////////////////////
int llread(unsigned char *packet)
{   
    unsigned char act;  // Variável para armazenar cada byte lido
    unsigned char ictrl;  // Variável para armazenar o campo de controlo
    int d = 0;  // Índice para posicionamento no array `packet`
    int state = STATE_START;  // Estado inicial da máquina de estados

    // Ciclo que lê dados enquanto o estado não é STATE_STOP
    while (state != STATE_STOP){
        // Lê um byte da porta serial
        int bytes = read(fd , &act , 1);
        
        if(bytes > 0){
            // Verificação do estado atual e transição para o próximo
            switch(state){
            
            case STATE_START: {  // Estado inicial de espera
                if(act == FLAG){
                    state = STATE_FLAG;  // Transição para o estado FLAG ao receber FLAG
                    printf("FLAG recebida, novo estado: FLAG\n");
                }
                break;
            }

            case STATE_FLAG: {  // Estado de verificação de endereço
                if(act == ADRESSTRANS){
                    state = STATE_ADRESS;  // Transição para estado ADRESS ao receber o endereço do transmissor
                    printf("ADRESS recebido, novo estado: ADRESS\n");
                }
                else if (act == FLAG){
                    state = STATE_FLAG;  // Mantém o estado FLAG se outra FLAG for recebida
                    printf("FLAG recebida, mesmo estado\n");
                }
                else{
                    state = STATE_START;  // Reinicia para START se outro valor for recebido
                    printf("Outro recebido, novo estado: START\n");
                }
                break;
            }

            case STATE_ADRESS: {  // Estado de verificação do campo de controlo
                if(act == 0x00 || act == 0x80){
                    state = STATE_CTRL;  // Transição para o estado CONTROL se o valor do índice do frame for válido
                    ictrl = act;  // Armazena o índice do frame
                    printf("Indice Frame recebido, novo estado: CONTROL\n");
                }
                else if(act == FLAG){
                    state = STATE_FLAG;  // Mantém o estado FLAG se outra FLAG for recebida
                    printf("FLAG recebida, novo estado: FLAG\n");
                }
                else{
                    state = STATE_START;  // Reinicia para START se outro valor for recebido
                    printf("Outro recebido, novo estado: START\n");
                }
                break;
            }

            case STATE_CTRL: {  // Estado de verificação de paridade
                if(act == (ictrl^ADRESSTRANS)){
                    state = STATE_READ;  // Transição para READ se BCC1 for válido
                    printf("Read, novo estado: Read\n");
                }
                else if(act == FLAG){
                    state = STATE_FLAG;  // Mantém o estado FLAG se outra FLAG for recebida
                    printf("FLAG recebida, novo estado: FLAG\n");
                }
                else {
                    state = STATE_START;  // Reinicia para START se BCC1 for inválido
                    printf("Outro recebido, novo estado: START\n");
                }
                break;
            }

            case STATE_READ: {  // Estado de leitura do conteúdo do pacote
                if(act == ESC){
                    state = STATE_FOUND;  // Transição para o estado FOUND ao encontrar um caractere ESC (byte escape)
                    printf("Esc encontrado, novo estado: FOUND\n");
                }
                else if (act == FLAG) {
                    unsigned char bcc2 = packet[d-1];  // Guarda o último byte para comparação com BCC2
                    d--;  // Ajusta o índice `d`
                    packet[d] = '\0';  // Termina o array `packet`
                    unsigned char dataacc = packet[0];  // Inicializa o acumulador para verificar BCC2

                    // Loop para calcular BCC2
                    for (unsigned int j = 1; j < d; j++) {
                        dataacc ^= packet[j];
                    }

                    if(bcc2 == dataacc){  // Verificação do BCC2 para integridade
                        state = STATE_STOP;  // Estado de parada ao sucesso na receção do pacote
                        int b;

                        // Definição de RR (Ready to Receive) conforme a trama esperada
                        if(tramaRec % 2 == 0) {
                            b = RR_0;
                        }
                        else {
                            b = RR_1;
                        }

                        // Envia confirmação de receção para o transmissor
                        unsigned char env[5] = {FLAG, ADRESSREC, b, (ADRESSREC^b), FLAG};
                        write(fd, env, 5);

                        // Atualiza `tramaRec` conforme o controlo da sequência
                        if((tramaRec % 2 == 1 && ictrl == 0x00) || (tramaRec % 2 == 0 && ictrl == 0x80)) {
                            tramaRec = (tramaRec + 1) % 2;
                        } else {
                            d = 0;  // Reinicia `d` se não houver alteração na sequência
                        }
                        frameI++;
                        return d;  // Retorna o tamanho do pacote lido com sucesso
                    }
                    else {
                        // Caso o BCC2 não coincida, envia REJ (Reject) ao transmissor para retransmissão
                        int b;
                        if(tramaRec % 2 == 0) {
                            b = REJ_0;
                        } else {
                            b = REJ_1;
                        }
                        printf("Erro: retransmissão do transmissor\n");
                        unsigned char env[5] = {FLAG, ADRESSREC, b, (ADRESSREC^b), FLAG};
                        write(fd, env, 5);
                        state = STATE_START;  // Reinicia para START
                        d = 0;  // Reinicia `d`
                        continue;

                    }
                }
                else {
                    packet[d++] = act;  // Armazena o byte lido em `packet` se não for FLAG nem ESC
                }
                break;
            }

            case STATE_FOUND: {  // Estado para processamento de byte ESC
                state = STATE_READ;
                packet[d++] = act ^ 0x20;  // Armazena o byte ESC após desfazer o byte stuffing
                break;
            }
            }
        }
    }
    printf("Leitura concluída, tamanho do pacote: %d\n", d);
    return 1;  // Retorna 1 indicando sucesso
}


void printStats(int roles) {
    switch (role) {
        case 0:
            printf("\n");
            printf("╔════════════════════════════════════════════════════════╗\n");
            printf("║          Estatísticas para o transmissor (LlTx)        ║\n");
            printf("╠═════════════════════════╦══════════════════════════════╣\n");
            printf("║   Frames Open Enviados  ║        %10d            ║\n", frameO);
            printf("╠═════════════════════════╬══════════════════════════════╣\n");
            printf("║   Frames Info Enviados  ║        %10d            ║\n", frameI);
            printf("╠═════════════════════════╬══════════════════════════════╣\n");
            printf("║   Frames Disc Enviados  ║        %10d            ║\n", frameD);
            printf("╠═════════════════════════╬══════════════════════════════╣\n");
            printf("║ Tempo de Transferência  ║       %10ld segundos    ║\n", endtime - starttime);
            printf("╚═════════════════════════╩══════════════════════════════╝\n\n");
            break;

        case 1:
            printf("\n");
            printf("╔════════════════════════════════════════════════════════╗\n");
            printf("║            Estatísticas para o recetor (LlRx)          ║\n");
            printf("╠═════════════════════════╦══════════════════════════════╣\n");
            printf("║  Frames Open Recebidos  ║        %10d            ║\n", frameO);
            printf("╠═════════════════════════╬══════════════════════════════╣\n");
            printf("║  Frames Info Recebidos  ║        %10d            ║\n", frameI);
            printf("╠═════════════════════════╬══════════════════════════════╣\n");
            printf("║  Frames Disc Recebidos  ║        %10d            ║\n", frameD);
            printf("╠═════════════════════════╬══════════════════════════════╣\n");
            printf("║ Tempo de Transferência  ║       %10ld segundos    ║\n", endtime - starttime);
            printf("╚═════════════════════════╩══════════════════════════════╝\n\n");
            break;

        default:
            break;
    }
}

////////////////////////////////////////////////
// LLCLOSE
////////////////////////////////////////////////
int llclose(int showStatistics)
{
    // Verifica se a função está a ser executada pelo emissor (role = 0) ou pelo recetor (role = 1)
    if (role == 0){
        (void) signal(SIGALRM, alarmHandler);
        unsigned char act;
        int transmidone = 0;   // Contador de tentativas de transmissão de DISC
        int state = STATE_START;   // Inicializa o estado para a máquina de estados
        alarmEnabled = FALSE;
        unsigned char env[5] = {FLAG, ADRESSTRANS, DISC, ADRESSTRANS ^ DISC, FLAG};

        // Tenta enviar DISC até que o número de retransmissões seja atingido ou uma resposta seja recebida
        while(retransmi > transmidone){
            if(alarmEnabled == FALSE){
                write(fd, env, 5);   // Envia a trama DISC ao recetor
                printf("Enviar DISC para o recetor\n");
                transmidone++;
                alarmEnabled = TRUE;
                alarm(timeou);   // Define o temporizador para retransmissão
            }

            // Máquina de estados para aguardar a receção de uma resposta DISC
            while(state != STATE_STOP){
                int bytes = read(fd, &act, 1);   // Lê um byte da ligação série
                if(bytes > 0){
                    switch(state){
                        case STATE_START:
                            if(act == FLAG){
                                state = STATE_FLAG;
                            }
                            break;
                        case STATE_FLAG:
                            if(act == ADRESSREC){
                                state = STATE_ADRESS;
                            }
                            else if(act == FLAG){
                                state = STATE_FLAG;
                            }
                            else{
                                state = STATE_START;
                            }
                            break;
                        case STATE_ADRESS:
                            if(act == DISC){
                                state = STATE_CTRL;
                            }
                            else if(act == FLAG){
                                state = STATE_FLAG;
                            }
                            else{
                                state = STATE_START;
                            }
                            break;
                        case STATE_CTRL:
                            if(act == (ADRESSREC ^ DISC)){
                                state = STATE_BCC;
                            }
                            else if(act == FLAG){
                                state = STATE_FLAG;
                            }
                            else{
                                state = STATE_START;
                            }
                            break;
                        case STATE_BCC:
                            if(act == FLAG){
                                state = STATE_STOP;
                            }
                            else{
                                state = STATE_START;
                            }
                            break;
                    }
                }
                else if(bytes == 0){
                    break;   // Termina se não houver mais bytes para ler
                }
                else {
                    printf("Erro ao ler a trama\n");
                    return -1;
                }
            }

            if (state == STATE_STOP){
                printf("Recebeu DISC do recetor\n");
                break;   // Sai do loop se receber DISC corretamente
            }
        }

        // Se não receber DISC após o número máximo de tentativas, encerra com erro
        if (state != STATE_STOP) {
            printf("Não recebeu resposta DISC do recetor após %d tentativas\n", retransmi);
            return -1;
        }

        // Envia a trama de confirmação UA após receber DISC do recetor
        unsigned char uaFrame[5] = {FLAG, ADRESSTRANS, UA, ADRESSTRANS ^ UA, FLAG};
        write(fd, uaFrame, 5);
        printf("Escreveu UA do llclose\n");
        frameD++;
    }
    else {
        printf("Recebeu DISC do emissor\n");
        unsigned char act;
        int state = STATE_START;

        // Máquina de estados para aguardar a receção de um DISC correto e responder
        while(state != STATE_STOP){
            int bytes = read(fd, &act, 1);
            if(bytes > 0){
                switch (state){
                    case STATE_START:
                        if(act == FLAG){
                            state = STATE_FLAG;
                        }
                        break;
                    case STATE_FLAG:
                        if(act == ADRESSTRANS){
                            state = STATE_ADRESS;
                        }
                        else if(act == FLAG){
                            state = STATE_FLAG;
                        }
                        else{
                            state = STATE_START;
                        }
                        break;
                    case STATE_ADRESS:
                        if(act == DISC){
                            state = STATE_CTRL;
                        }
                        else if(act == FLAG){
                            state = STATE_FLAG;
                        }
                        else{
                            state = STATE_START;
                        }
                        break;
                    case STATE_CTRL:
                        if(act == (ADRESSTRANS ^ DISC)){
                            state = STATE_BCC;
                        }
                        else if(act == FLAG){
                            state = STATE_FLAG;
                        }
                        else{
                            state = STATE_START;
                        }
                        break;
                    case STATE_BCC:
                        if(act == FLAG){
                            state = STATE_STOP;
                        }
                        else{
                            state = STATE_START;
                        }
                        break;
                }
            }
        } 

        // Envia DISC de resposta ao emissor após receber DISC corretamente
        printf("DISC correto, enviar DISC para o emissor\n");
        unsigned char env[5] = {FLAG, ADRESSREC, DISC, ADRESSREC ^ DISC, FLAG};
        write(fd, env, 5);
        frameD++;
    }

    time(&endtime);
    alarm(0);   // Desativa o alarme

    // Fecha a porta serial
    int res = closeSerialPort();
    if(res < 0){
        printf("Erro ao fechar a porta serial\n");
        return -1;
    }

    printStats(role);   // Imprime as estatísticas da ligação

    return 1;   // Indica sucesso ao fechar a ligação
}
