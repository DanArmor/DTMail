#ifndef _INC_UTIL_H
#define _INC_UTIL_H

#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <stdlib.h>

#include <WinSock2.h>
#include <Windows.h>
#include <iphlpapi.h>

#include "iup.h"

#define MY_GUI_ERROR(msg, status)\
    do{\
        sprintf(errorBuffer, "%s %d", msg, status);\
        IupMessage("Error!", errorBuffer);\
        IupClose();\
        exit(status);\
    }while(0)

#define MAX_CLIENTS 100
#define SMTP_SERVICE (MAX_CLIENTS-1)
#define POP3_SERVICE (MAX_CLIENTS-2)
#define LOCK_OUT() WaitForSingleObject(glOutputMutex, INFINITE)
#define UNLOCK_OUT() ReleaseMutex(glOutputMutex)
#define LOCK_TH() WaitForSingleObject(glThreadMutex, INFINITE)
#define UNLOCK_TH() ReleaseMutex(glThreadMutex)
#define LOCK_FL() WaitForSingleObject(glFileMutex, INFINITE)
#define UNLOCK_FL() ReleaseMutex(glFileMutex)
#define LOCK_GUI() WaitForSingleObject(glGUIMutex, INFINITE)
#define UNLOCK_GUI() ReleaseMutex(glGUIMutex)

#define REPORT_FORMAT "%s%s\033[0m: TH#%d: "
#define REPORT_FORMAT_USER "%s%s\033[0m: TH#%d(%s): "

#define PLTH_PICKPREFIXCOLOR(lThInfo) ((lThInfo)->protocol == PROTOCOL_SMTP? "\033[31m" : "\033[32m")
#define PLTH_PICKPREFIX(lThInfo) ((lThInfo)->protocol == PROTOCOL_SMTP? "SMTP" : "POP3")

#define PLTH_REPORT(lThInfo, format, ...)\
    do{\
    LOCK_OUT();\
        if((lThInfo)->haveUser){\
            fprintf(stderr, REPORT_FORMAT_USER format, PLTH_PICKPREFIXCOLOR(lThInfo), PLTH_PICKPREFIX(lThInfo), (lThInfo)->threadInfo.id, (lThInfo)->user.name, ##__VA_ARGS__);\
        }else{\
            fprintf(stderr, REPORT_FORMAT format, PLTH_PICKPREFIXCOLOR(lThInfo), PLTH_PICKPREFIX(lThInfo), (lThInfo)->threadInfo.id, ##__VA_ARGS__);\
        }\
    UNLOCK_OUT();\
    }while(0)

int maxFunc(int a, int b);

#define POP3_SERVER_PORT 110
#define SMTP_SERVER_PORT 25

#define PROTOCOL_SMTP 1
#define PROTOCOL_POP3 2

#define C_CR '\015'
#define C_LF '\013'
#define S_CRLF "\015\012"

#define GUI_START_SECTION \
    LOCK_TH();\
    LOCK_GUI();\
    if(isGuiRunning){

#define GUI_END_SECTION \
    }\
    UNLOCK_GUI();\
    UNLOCK_TH();\
    return IUP_DEFAULT

#define MY_ERROR(msg, status)\
    do{\
        fprintf(stderr, "ERROR: %s. Status: %d\n", msg, status);\
        exit(status);\
    }while(0)

#define MAIL_DATA_FILE "mail.data"
#define MAIL_SUPPORT_FILE "mail.data.tmp"
#define BUFF_SIZE 26548
#define NAME_MAX_SIZE 256
#define PASS_MAX_SIZE 256
#define USER_DATA_FILE "users.data"

typedef struct SMTPData{
    char *FROM;
    char *TO;
    char *DATA;
    int dataSize;
    int buffSize;
} SMTPData;

typedef struct SessionLog{
    int size;
    int buffSize;
    char *session;
    int protocol;
    int id;
    int isDead;
} SessionLog;

void WriteToSession(SessionLog *sessionLog, char *msg, int size);
void WriteToSessionPrefix(SessionLog *sessionLog, int isServer);

typedef struct LocalThreadInfo LocalThreadInfo;

typedef struct ServerThread{
    HANDLE handle;
    BOOL isFree;
    SOCKET client;
    int id;
    LocalThreadInfo *pLocal;
} ServerThread;

// В файл будут сбрасываться только первые 3 поля
typedef struct UserInfo{
    char name[NAME_MAX_SIZE];
    char pass[PASS_MAX_SIZE];
    int id;
    int isLogged;
    int index;
} UserInfo;

struct LocalThreadInfo{
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
    // Сервисный поток. Не нужно выводить лог в GUI
    int isService;
    // Запись SMTP/POP3 сессии
    SessionLog sessionLog;
    // для SMTP
    char *domainName; // Доменное имя отправителя
    SMTPData *smtpData;

};

void MigrateDeadSession(SessionLog *sessionLog);
void InitSMTPData(SMTPData *smtpData);
void InitServerThread(ServerThread *serverThread);
void InitLocalThreadInfo(LocalThreadInfo *lThInfo, ServerThread *thInfo, int protocol, int isService);
void StopProcessingClient(LocalThreadInfo *lThInfo);

void GetCommandCRLF(char *str, int *size);
void GetCommand(char *str, int *size);
int ReadUntilCRLF(SOCKET sock, char *buff, int *size);
int ReadUntilDotCRLF(SOCKET sock, char *buff, int*size);

/// @brief Анализирует, лежит ли в буфере потока lThInfo команда command
/// @param lThInfo Указатель на данные потока
/// @param command Команда
/// @return Истина, если команда лежит в буфере - иначе ложь
BOOL WINAPI IsServerCommand(LocalThreadInfo *lThInfo, char *command);

extern int keepRunning;
extern int isGuiRunning;
extern int flagIsPrefix;

extern HANDLE glOutputMutex;
extern HANDLE glThreadMutex;
extern HANDLE glFileMutex;
extern HANDLE glGUIMutex;
extern ServerThread glPool[MAX_CLIENTS];

// GUI
extern Ihandle *glGUIThreadList;
extern Ihandle *glGUIMainBox;
extern Ihandle *glGUIMainDlg;
extern Ihandle *glGUISessionText;
extern Ihandle *glGUIHorizontalMainBox;

extern UserInfo *glUserList;
extern int usersInList;

extern char errorBuffer[512];

extern SessionLog *glDeadSessions;
extern int glDeadSessionsN;
extern int glSessionsN;

extern Ihandle *glGUISessionBox;
extern Ihandle *glGUISessionButtonsBox;
extern Ihandle *glGUISessionTextIncreaseSizeButton;
extern Ihandle *glGUISessionTextDecreaseSizeButton;

// GUI
int LoadThreadList(void);
int DisplaySession(void);
int IncreaseTextSize(void);
int DecreaseTextSize(void);

#endif