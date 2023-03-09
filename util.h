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

#define PROTOCOL_SMTP 1
#define PROTOCOL_POP3 2

#define C_CR '\015'
#define C_LF '\013'
#define S_CRLF "\015\012"

typedef struct SMTPData{
    char *FROM;
    char *TO;
    char *DATA;
    int dataSize;
    int buffSize;
} SMTPData;

typedef struct ServerThread{
    HANDLE handle;
    BOOL isFree;
    SOCKET client;
    int id;
} ServerThread;

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

typedef struct LocalThreadInfo{
    // Вид протокола
    int protocol;
    // Мы локально храним данные потока, чтобы не блокировать мьютекс
    // для доступа к ним внутри потоков
    ServerThread threadInfo;
    // Указатель на запись о потоке в пуле. Необходима для окончания работы
    // потока, чтобы обозначить, что он свободен
    ServerThread *pthreadInfo;
    UserInfo user;
    int haveUser;
    int havePass;
    char *buff;
    // Количество полезных данных в буфере
    int size;
    // для SMTP
    char *domainName; // Доменное имя отправителя
    SMTPData *smtpData;

} LocalThreadInfo;

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

/// @brief Анализирует, лежит ли в буфере потока lThInfo команда command
/// @param lThInfo Указатель на данные потока
/// @param command Команда
/// @return Истина, если команда лежит в буфере - иначе ложь
BOOL WINAPI IsServerCommand(LocalThreadInfo *lThInfo, char *command);

/// @brief Возвращает данные, закодированные в Base64
/// @details Немного модифицированная реализация https://stackoverflow.com/a/6782480
/// @param data Кодируемые данные
/// @param input_length Длина входных данных
/// @param output_length Длина выходных данныъ
/// @return Возвращает указатель на закодированные данные в динамической памяти
char *Base64Encode(const unsigned char *data,
                    int input_length,
                    int *output_length);

/// @brief Возвращает таблицу декодирования в динамической памяти
/// @details Немного модифицированная реализация https://stackoverflow.com/a/6782480
/// @return Возвращает таблицу декодирования в динамической памяти
char *Base64BuildDecodeTable();

/// @brief Возвращает данные, декодированные из Base64
/// @details Немного модифицированная реализация https://stackoverflow.com/a/6782480
/// @param data Декодируемые данные
/// @param input_length Длина входных данных
/// @param output_length Длина выходных данныъ
/// @return Возвращает указатель на декодированные данные в динамической памяти
char *Base64Decode(const char *data,
                             int input_length,
                             int *output_length);

#endif