#include "util.h"

#include <stdint.h>
#include <stdlib.h>

void GetCommand(char *str, int *size) {
    char buffer[BUFF_SIZE];
    fgets(buffer, BUFF_SIZE, stdin);
    int index = strcspn(buffer, "\n");
    buffer[index] = '\0';
    strcpy(str, buffer);
    *size = index;
}

void GetCommandCRLF(char *str, int *size) {
    char buffer[BUFF_SIZE];
    fgets(buffer, BUFF_SIZE, stdin);
    int index = strcspn(buffer, "\n");
    buffer[index] = '\015';
    buffer[index+1] = '\012';
    buffer[index+2] = '\0';
    strcpy(str, buffer);
    *size = index+2;
}

void SendERR(SOCKET sock, char *msg){
    send(sock, "-ERR", 4, 0x0);
    if(msg != NULL){
        send(sock, msg, strlen(msg), 0x0);
    }
    send(sock, "\015\012", 2, 0x0);
}

void SendOK(SOCKET sock, char *msg){
    send(sock, "+OK", 3, 0x0);
    if(msg != NULL){
        send(sock, msg, strlen(msg), 0x0);
    }
    send(sock, "\015\012", 2, 0x0);
}

void SMTPSendOK(SOCKET sock, char *msg){
    send(sock, "250", 3, 0x0);
    if(msg != NULL){
        send(sock, msg, strlen(msg), 0x0);
    }
    send(sock, "\015\012", 2, 0x0);
}

void SMTPSendNeedMoreData(SOCKET sock, char *msg){
    send(sock, "350", 3, 0x0);
    if(msg != NULL){
        send(sock, msg, strlen(msg), 0x0);
    }
    send(sock, "\015\012", 2, 0x0);
}

void SMTPSendTempError(SOCKET sock, char *msg){
    send(sock, "450", 3, 0x0);
    if(msg != NULL){
        send(sock, msg, strlen(msg), 0x0);
    }
    send(sock, "\015\012", 2, 0x0);
}

void SMTPSendServerError(SOCKET sock, char *msg){
    send(sock, "550", 3, 0x0);
    if(msg != NULL){
        send(sock, msg, strlen(msg), 0x0);
    }
    send(sock, "\015\012", 2, 0x0);
}

BOOL CheckStatus(char *buff){
    if(strncmp(buff, "+OK", 3) == 0){
        return TRUE;
    } else if(strncmp(buff, "-ERR", 4) == 0){
        return FALSE;
    }
}

int StatusLineEnd(char *buff, int size){
    buff[size] = '\0';
    char *endLine = strstr(buff, "\013\010");
    if(endLine == NULL){
        return -1;
    } else{
        return endLine - buff;
    }
}

int ReadUntilCRLF(SOCKET sock, char *buff, int *size){
    int off = 0;
    while(1){
        int ret = recv(sock, buff+off, BUFF_SIZE, 0x0);
        if(ret == SOCKET_ERROR || ret == 0){
            return 1;
        }
        off += ret;
        buff[off] = '\0';
        if(strstr((off-ret-3 >= 0 ? buff+off-ret-3 : buff+off-ret), "\015\012") != NULL){
            *size = off;
            return 0;
        }
    }
}

int ReadUntilDotCRLF(SOCKET sock, char *buff, int *size){
    if(*size != 0){
        buff[*size] = '\0';
        if(strstr(buff, "\015\012.\015\012") != NULL){
            return 0;
        }
    }
    int off = *size;
    while(1){
        int ret = recv(sock, buff+off, BUFF_SIZE, 0x0);
        if(ret == SOCKET_ERROR || ret == 0){
            return 1;
        }
        off += ret;
        buff[off] = '\0';
        if(strstr((off-ret-5 >= 0 ? buff+off-ret-5 : buff+off-ret), "\015\012.\015\012") != NULL){
            *size = off;
            return 0;
        }
    }
}

BOOL WINAPI IsServerCommand(LocalThreadInfo *lThInfo, char *command){
    int commandSize = strlen(command);
    if(lThInfo->size-2 >= commandSize &&
      (lThInfo->buff[commandSize] == ' ' || strncmp(lThInfo->buff+commandSize, S_CRLF, 2) == 0)){
        return strncmp(lThInfo->buff, command, commandSize) == 0;
    } else{
        return FALSE;
    }
}

char *Base64Encode(const unsigned char *data,
                    int input_length,
                    int *output_length) {

    int mod_table[] = {0, 2, 1};
    char encoding_table[] = {'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H',
                                'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
                                'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X',
                                'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f',
                                'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n',
                                'o', 'p', 'q', 'r', 's', 't', 'u', 'v',
                                'w', 'x', 'y', 'z', '0', '1', '2', '3',
                                '4', '5', '6', '7', '8', '9', '+', '/'};
    *output_length = 4 * ((input_length + 2) / 3);
    char *encoded_data = malloc(*output_length);
    if (encoded_data == NULL) return NULL;

    for (int i = 0, j = 0; i < input_length;) {

        uint32_t octet_a = i < input_length ? (unsigned char)data[i++] : 0;
        uint32_t octet_b = i < input_length ? (unsigned char)data[i++] : 0;
        uint32_t octet_c = i < input_length ? (unsigned char)data[i++] : 0;

        uint32_t triple = (octet_a << 0x10) + (octet_b << 0x08) + octet_c;

        encoded_data[j++] = encoding_table[(triple >> 3 * 6) & 0x3F];
        encoded_data[j++] = encoding_table[(triple >> 2 * 6) & 0x3F];
        encoded_data[j++] = encoding_table[(triple >> 1 * 6) & 0x3F];
        encoded_data[j++] = encoding_table[(triple >> 0 * 6) & 0x3F];
    }

    for (int i = 0; i < mod_table[input_length % 3]; i++)
        encoded_data[*output_length - 1 - i] = '=';

    return encoded_data;
}

char *Base64BuildDecodeTable() {
    char encoding_table[] = {'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H',
                                'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
                                'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X',
                                'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f',
                                'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n',
                                'o', 'p', 'q', 'r', 's', 't', 'u', 'v',
                                'w', 'x', 'y', 'z', '0', '1', '2', '3',
                                '4', '5', '6', '7', '8', '9', '+', '/'};
    char *decoding_table = malloc(256);

    for (int i = 0; i < 64; i++)
        decoding_table[(unsigned char) encoding_table[i]] = i;
    return decoding_table;
}


char *Base64Decode(const char *data,
                             int input_length,
                             int *output_length) {

    char *decoding_table = Base64BuildDecodeTable();

    if (input_length % 4 != 0) return NULL;

    *output_length = input_length / 4 * 3;
    if (data[input_length - 1] == '=') (*output_length)--;
    if (data[input_length - 2] == '=') (*output_length)--;

    unsigned char *decoded_data = calloc(*output_length+1, sizeof(char));
    if (decoded_data == NULL) return NULL;

    for (int i = 0, j = 0; i < input_length;) {

        uint32_t sextet_a = data[i] == '=' ? 0 & i++ : decoding_table[data[i++]];
        uint32_t sextet_b = data[i] == '=' ? 0 & i++ : decoding_table[data[i++]];
        uint32_t sextet_c = data[i] == '=' ? 0 & i++ : decoding_table[data[i++]];
        uint32_t sextet_d = data[i] == '=' ? 0 & i++ : decoding_table[data[i++]];

        uint32_t triple = (sextet_a << 3 * 6)
        + (sextet_b << 2 * 6)
        + (sextet_c << 1 * 6)
        + (sextet_d << 0 * 6);

        if (j < *output_length) decoded_data[j++] = (triple >> 2 * 8) & 0xFF;
        if (j < *output_length) decoded_data[j++] = (triple >> 1 * 8) & 0xFF;
        if (j < *output_length) decoded_data[j++] = (triple >> 0 * 8) & 0xFF;
    }
    free(decoding_table);
    return (char*)decoded_data;
}
