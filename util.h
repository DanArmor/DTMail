#ifndef _INC_UTIL_H
#define _INC_UTIL_H

#include <stdio.h>
#include <string.h>
#include <signal.h>

#include <WinSock2.h>
#include <Windows.h>
#include <iphlpapi.h>

#define POP3_SERVER_PORT 110
#define SMTP_SERVER_PORT 25

#define MY_ERROR(msg, status)\
    do{\
        fprintf(stderr, "ERROR: %s. Status: %d\n", msg, status);\
        exit(status);\
    }while(0)

#define USER_DATA_FILE "users.data"
#define MAIL_DATA_FILE "mail.data"
#define MAIL_SUPPORT_FILE "mail.data.tmp"
#define BUFF_SIZE 26548
#define NAME_MAX_SIZE 256
#define PASS_MAX_SIZE 256


// В файл будут сбрасываться только первые 3 поля
typedef struct UserInfo{
    char name[NAME_MAX_SIZE];
    char pass[PASS_MAX_SIZE];
    int id;
    int isLogged;
    int index;
} UserInfo;

typedef struct FileHeader{
    int nMessages;
} FileHeader;

typedef struct MessageData{
    int len;
    char TO[256];
    char *buff;
} MessageData;

void GetCommandCRLF(char *str, int *size);
void GetCommand(char *str, int *size);
void SendERR(SOCKET sock, char *msg);
void SendOK(SOCKET sock, char *msg);
BOOL CheckStatus(char *buff);
int StatusLineEnd(char *buff, int size);
int ReadUntilCRLF(SOCKET sock, char *buff, int *size);
int ReadUntilDotCRLF(SOCKET sock, char *buff, int*size);

void SMTPSendOK(SOCKET sock, char *msg);
void SMTPSendNeedMoreData(SOCKET sock, char *msg);
void SMTPSendTempError(SOCKET sock, char *msg);
void SMTPSendServerError(SOCKET sock, char *msg);

char *Base64Encode(const unsigned char *data,
                    int input_length,
                    int *output_length);

char *Base64BuildDecodeTable();

char *Base64Decode(const char *data,
                             int input_length,
                             int *output_length);

#endif