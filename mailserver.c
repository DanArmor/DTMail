/// @file mailserver.c
/// @brief Главный файл почтового сервера
/// @author @DanArmor

#include <stdio.h>
#include <string.h>
#include <signal.h>

#include <WinSock2.h>
#include <Windows.h>
#include <iphlpapi.h>

#include "util.h"

#define MAX_CLIENTS 100
#define SMTP_SERVICE (MAX_CLIENTS-1)
#define LOCK_OUT() WaitForSingleObject(glOutputMutex, INFINITE)
#define UNLOCK_OUT() ReleaseMutex(glOutputMutex)
#define LOCK_TH() WaitForSingleObject(glThreadMutex, INFINITE)
#define UNLOCK_TH() ReleaseMutex(glThreadMutex)
#define LOCK_FL() WaitForSingleObject(glFileMutex, INFINITE)
#define UNLOCK_FL() ReleaseMutex(glFileMutex)

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

int maxFunc(int a, int b){
    return a > b ? a : b;
}

volatile int keepRunning = 1;
HANDLE glOutputMutex;
HANDLE glThreadMutex;
HANDLE glFileMutex;
SOCKET pop3service;
ServerThread glPool[MAX_CLIENTS];

void intHandler(int notUsed){
    LOCK_OUT();
    fprintf(stderr, "Сервер останавливается. . .\n");
    UNLOCK_OUT();

    LOCK_TH();
    keepRunning = 0;
    closesocket(glPool[SMTP_SERVICE].client);
    closesocket(pop3service);
    UNLOCK_TH();
}

UserInfo *glUserList;
int usersInList;

UserInfo *ReadUsersFromFile(char *filename, int *nUsers){
    FILE *f = fopen(filename, "rb");
    fread(nUsers, sizeof(int), 1, f);
    if(*nUsers == 0){
        fprintf(stderr, "No users in file!\n");
        return NULL;
    }
    UserInfo *users = calloc(*nUsers, sizeof(UserInfo));
    for(int i = 0; i < *nUsers; i++){
        fread(users[i].name, NAME_MAX_SIZE, 1, f);
        fread(users[i].pass, PASS_MAX_SIZE, 1, f);
        fread(&users[i].id, sizeof(int), 1, f);
        users[i].index = i;
    }
    fclose(f);
    return users;
}

void InitSMTPData(SMTPData *smtpData){
    smtpData->FROM = NULL;
    smtpData->TO = NULL;
    smtpData->DATA = NULL;
    smtpData->dataSize = 0;
    smtpData->buffSize = 0;
}

void InitServerThread(ServerThread *serverThread){
    serverThread->handle = 0;
    serverThread->client = 0;
    serverThread->isFree = 1;
}

void InitLocalThreadInfo(LocalThreadInfo *lThInfo, ServerThread *thInfo, int protocol){
    LOCK_TH();
    lThInfo->pthreadInfo = thInfo;
    lThInfo->threadInfo = *thInfo;
    UNLOCK_TH();
    lThInfo->havePass = 0;
    lThInfo->haveUser = 0;
    memset(&lThInfo->user, 0x0, sizeof(UserInfo));
    lThInfo->buff = (char*)malloc(BUFF_SIZE);
    memset(lThInfo->buff, 0x0, BUFF_SIZE);
    lThInfo->size = 0;
    lThInfo->protocol = protocol;
    if(protocol == PROTOCOL_SMTP){
        lThInfo->domainName = calloc(256, sizeof(char));
        lThInfo->smtpData = calloc(1, sizeof(SMTPData));
        InitSMTPData(lThInfo->smtpData);
    }
}

void StopProcessingClient(LocalThreadInfo *lThInfo){
    // Лучше обращаться по указателю к глобальному пулу потоков
    // Т.к. мы все равно блочим мьютекс для обновления данных по доступности потока
    PLTH_REPORT(lThInfo, "Окончил работу.\nПоследнее содержимое буфера:\n===\n%s===\n", lThInfo->buff);

    LOCK_TH();
    closesocket(lThInfo->pthreadInfo->client);
    free(lThInfo->buff);
    glUserList[lThInfo->user.index].isLogged = 0;
    lThInfo->pthreadInfo->isFree = 1;
    if(lThInfo->protocol == PROTOCOL_SMTP){
        if(lThInfo->smtpData->DATA != NULL){
            free(lThInfo->smtpData->DATA);
        }
        if(lThInfo->smtpData->TO != NULL){
            free(lThInfo->smtpData->TO);
        }
        if(lThInfo->smtpData->FROM != NULL){
            free(lThInfo->smtpData->FROM);
        }
        if(lThInfo->domainName != NULL){
            free(lThInfo->domainName);
        }
        free(lThInfo->smtpData);
    }
    CloseHandle(lThInfo->threadInfo.handle);
    UNLOCK_TH();
}

DWORD POP3CommandUSER(LocalThreadInfo *lThInfo){
    lThInfo->havePass = 0;
    lThInfo->haveUser = 0;
    int index = 0;
    for(index = 4; lThInfo->buff[index] != '\015' && lThInfo->buff[index] == ' '; index++){
        ;
    }
    if(lThInfo->buff[index] == '\015'){
        PLTH_REPORT(lThInfo, "Отсутствует имя пользователя в сообщении\n");
        SendERR(lThInfo->threadInfo.client, " should provide user name");
        return 1;
    }
    int startOfName = index;
    for(index = startOfName; index < lThInfo->size && lThInfo->buff[index] > ' '; index++){
        ;
    }
    int endOfName = index;
    int nameLen = endOfName - startOfName;

    LOCK_TH();
    for(index = 0; index < usersInList; index++){
        if(strncmp(glUserList[index].name, &lThInfo->buff[startOfName], maxFunc(strlen(glUserList[index].name), nameLen)) == 0){
            if(glUserList[index].isLogged){
                PLTH_REPORT(lThInfo, "Попытка авторизации под именем активного на данный момент пользователя(%s)\n", glUserList[index].name);
                SendERR(lThInfo->threadInfo.client, " User already logged in");
                return 1;
            }
            SendOK(lThInfo->threadInfo.client, NULL);
            lThInfo->user = glUserList[index];
            lThInfo->haveUser = TRUE;
            PLTH_REPORT(lThInfo, "Пользователь найден в базе\n");
            break;
        }
    }
    UNLOCK_TH();
    if(index == usersInList){
        char back = lThInfo->buff[endOfName];
        lThInfo->buff[endOfName] = '\0';
        PLTH_REPORT(lThInfo, "Ошибка - пользователь(%s) не найден\n", lThInfo->buff+startOfName);
        lThInfo->buff[endOfName] = back;
        SendERR(lThInfo->threadInfo.client, " no such user");
        return 1;
    }
    return 0;
}

DWORD POP3CommandPASS(LocalThreadInfo *lThInfo){
    if(lThInfo->haveUser == 0){
        SendERR(lThInfo->threadInfo.client, " Send USER first");
        return 1;
    }
    int index = 0;
    for(index = 4; lThInfo->buff[index] != '\015' && lThInfo->buff[index] == ' '; index++){
        ;
    }
    if(lThInfo->buff[index] == '\015'){
        PLTH_REPORT(lThInfo, "Пароль не предоставлен\n");
        SendERR(lThInfo->threadInfo.client, " should provide password");
        return 1;
    }
    int startOfPass = index;
    for(index = startOfPass; index < lThInfo->size && lThInfo->buff[index] > ' '; index++){
        ;
    }
    int endOfPass = index;
    int passLen = endOfPass - startOfPass;
    if(strncmp(&lThInfo->buff[startOfPass], lThInfo->user.pass, maxFunc(strlen(lThInfo->user.pass), passLen)) == 0){
        PLTH_REPORT(lThInfo, "Авторизация успешна\n");
        SendOK(lThInfo->threadInfo.client, " Logged in");
        return 0;
    }
    PLTH_REPORT(lThInfo, "Неверный пароль\n");
    SendERR(lThInfo->threadInfo.client, " Wrong password");
    return 1;
}

DWORD WINAPI POP3CommandLIST(LocalThreadInfo *lThInfo){
    LOCK_FL();

    int nMessages = 0;
    FILE *f = fopen(MAIL_DATA_FILE, "rb");
    fread(&nMessages, sizeof(int), 1, f);
    int dataSize = 0;
    char buff[NAME_MAX_SIZE];
    memset(buff, 0x0, NAME_MAX_SIZE);
    int userMessages = 0;
    for(int i = 0; i < nMessages; i++){
        fread(&dataSize, sizeof(int), 1, f);
        fread(buff, NAME_MAX_SIZE, 1, f);
        if(strcmp(lThInfo->user.name, buff) == 0){
            userMessages++;
        }
        fseek(f, dataSize, SEEK_CUR);
    }
    fseek(f, sizeof(int), SEEK_SET);
    sprintf(buff, " %d:", userMessages);
    SendOK(lThInfo->threadInfo.client, buff);
    int index = 1;
    for(int i = 0; i < nMessages; i++){
        fread(&dataSize, sizeof(int), 1, f);
        fread(buff, NAME_MAX_SIZE, 1, f);
        if(strcmp(lThInfo->user.name, buff) == 0){
            sprintf(buff, "%d %d\015\012", index, dataSize);
            index++;
            send(lThInfo->threadInfo.client, buff, strlen(buff), 0x0);
        }
        fseek(f, dataSize, SEEK_CUR);
    }

    fclose(f);
    UNLOCK_FL();

    send(lThInfo->threadInfo.client, ".\015\012", 3, 0x0);

    return 0;
}

DWORD WINAPI POP3CommandRETR(LocalThreadInfo *lThInfo){
    LOCK_FL();

    int nMessages = 0;
    FILE *f = fopen(MAIL_DATA_FILE, "rb");
    fread(&nMessages, sizeof(int), 1, f);
    int dataSize = 0;
    char buff[NAME_MAX_SIZE];
    memset(buff, 0x0, NAME_MAX_SIZE);
    int userMessages = 0;
    int index = 0;
    sscanf(lThInfo->buff+4, "%d", &index);
    int wasFound = 0;
    for(int i = 0; i < nMessages; i++){
        fread(&dataSize, sizeof(int), 1, f);
        fread(buff, NAME_MAX_SIZE, 1, f);
        if(strcmp(lThInfo->user.name, buff) == 0){
            userMessages++;
            if(userMessages == index){
                sprintf(buff, " %d octets", dataSize);
                SendOK(lThInfo->threadInfo.client, buff);
                char *dataP = malloc(dataSize);
                fread(dataP, dataSize, 1, f);
                send(lThInfo->threadInfo.client, dataP, dataSize, 0x0);
                free(dataP);
                wasFound = 1;
                break;
            }
        }
        fseek(f, dataSize, SEEK_CUR);
    }

    fclose(f);
    UNLOCK_FL();
    if(wasFound == 0){
        SendERR(lThInfo->threadInfo.client, " No msg with such index");
    }

    return 0;
}

DWORD WINAPI POP3CommandDELE(LocalThreadInfo *lThInfo){
    LOCK_FL();

    int nMessages = 0;
    FILE *f = fopen(MAIL_DATA_FILE, "rb");
    FILE *r = fopen(MAIL_SUPPORT_FILE, "wb");

    fread(&nMessages, sizeof(int), 1, f);
    nMessages--;
    fwrite(&nMessages, sizeof(int), 1, r);
    nMessages++;

    int wasCorrect = 0;

    int dataSize = 0;
    char buff[NAME_MAX_SIZE];
    memset(buff, 0x0, NAME_MAX_SIZE);
    int userMessages = 0;
    int index = 0;
    sscanf(lThInfo->buff+4, "%d", &index);
    for(int i = 0; i < nMessages; i++){
        fread(&dataSize, sizeof(int), 1, f);
        fread(buff, NAME_MAX_SIZE, 1, f);
        if(strcmp(lThInfo->user.name, buff) == 0){
            userMessages++;
            if(userMessages == index){
                wasCorrect = 1;
                fseek(f, dataSize, SEEK_CUR);
                continue;
            }
        }
        char *dataP = malloc(dataSize);
        fread(dataP, dataSize, 1, f);
        fwrite(&dataSize, sizeof(int), 1, r);
        fwrite(buff, NAME_MAX_SIZE, 1, r);
        fwrite(dataP, dataSize, 1, r);
        free(dataP);
    }

    fclose(f);
    fclose(r);
    if(wasCorrect){
        remove(MAIL_DATA_FILE);
        rename(MAIL_SUPPORT_FILE, MAIL_DATA_FILE);
        SendOK(lThInfo->threadInfo.client, NULL);
    } else{
        remove(MAIL_SUPPORT_FILE);
        SendERR(lThInfo->threadInfo.client, " No msg with such index");
    }
    UNLOCK_FL();

    return 0;
}

DWORD WINAPI POP3CommandSTAT(LocalThreadInfo *lThInfo){
    LOCK_FL();

    int nMessages = 0;
    FILE *f = fopen(MAIL_DATA_FILE, "rb");
    fread(&nMessages, sizeof(int), 1, f);
    int dataSize = 0;
    char buff[NAME_MAX_SIZE];
    memset(buff, 0x0, NAME_MAX_SIZE);
    int userMessages = 0;
    int totalSize = 0;
    for(int i = 0; i < nMessages; i++){
        fread(&dataSize, sizeof(int), 1, f);
        fread(buff, NAME_MAX_SIZE, 1, f);
        if(strcmp(lThInfo->user.name, buff) == 0){
            userMessages++;
            totalSize += dataSize;
        }
        fseek(f, dataSize, SEEK_CUR);
    }

    fclose(f);
    UNLOCK_FL();

    sprintf(buff, " %d %d", userMessages, totalSize);
    SendOK(lThInfo->threadInfo.client, buff);
    PLTH_REPORT(lThInfo, "STAT OK -%s\n", buff);

    return 0;
}

DWORD WINAPI ProcessPOP3(LPVOID lpParameter){
    LocalThreadInfo lThInfo;
    InitLocalThreadInfo(&lThInfo, (ServerThread*)lpParameter, PROTOCOL_POP3);


    PLTH_REPORT(&lThInfo, "Подключился новый клиент POP3\n");

    SendOK(lThInfo.threadInfo.client, NULL);
    while(1){
        if(ReadUntilCRLF(lThInfo.threadInfo.client, lThInfo.buff, &lThInfo.size) != 0){
            PLTH_REPORT(&lThInfo, "Сокет закрыт\n");
            break;
        }

        PLTH_REPORT(&lThInfo, "Новое сообщение: %s", lThInfo.buff);

        if(IsServerCommand(&lThInfo, "USER")){
            POP3CommandUSER(&lThInfo);
        } else if(IsServerCommand(&lThInfo, "PASS")){
            if(lThInfo.haveUser == 0){
                SendERR(lThInfo.threadInfo.client, " Provide USER first");
                continue;
            }
            if(POP3CommandPASS(&lThInfo) == 0){
                lThInfo.havePass = 1;
            }
        } else if(lThInfo.havePass){
            if(IsServerCommand(&lThInfo, "STAT")){
                POP3CommandSTAT(&lThInfo);
            } else if(IsServerCommand(&lThInfo, "LIST")){
                POP3CommandLIST(&lThInfo);
            } else if(IsServerCommand(&lThInfo, "QUIT")){
                SendOK(lThInfo.threadInfo.client, NULL);
                break;
            } else if(IsServerCommand(&lThInfo, "RETR")){
                POP3CommandRETR(&lThInfo);
            } else if(IsServerCommand(&lThInfo, "DELE")){
                POP3CommandDELE(&lThInfo);
            } else if(IsServerCommand(&lThInfo, "TOP")){
                SendERR(lThInfo.threadInfo.client, " not supported");
            } else{
                SendERR(lThInfo.threadInfo.client, " Unknown command");
            } 
        } else{
            SendERR(lThInfo.threadInfo.client, " AUTH first");
        }
    }

    StopProcessingClient(&lThInfo);

    return 0;
}

DWORD WINAPI SMTPCommandEHLO(LocalThreadInfo *lThInfo){
    int index = 0;
    for(index = 4; lThInfo->buff[index] != '\015' && lThInfo->buff[index] == ' '; index++){
        ;
    }
    if(lThInfo->buff[index] == '\015'){
        SMTPSendNeedMoreData(lThInfo->threadInfo.client, "should provide user name");
        return 1;
    }
    int startOfName = index;
    for(index = startOfName; index < lThInfo->size && lThInfo->buff[index] > ' '; index++){
        ;
    }
    int endOfName = index;
    int nameLen = endOfName - startOfName;
    memcpy(lThInfo->domainName, &lThInfo->buff[startOfName], nameLen);

    return 0;
}

DWORD WINAPI SMTPCommandMAILFROM(LocalThreadInfo *lThInfo){
    int index = 0;
    for(index = 11; lThInfo->buff[index] != '\015' && (lThInfo->buff[index] == ' ' || lThInfo->buff[index] == '<'); index++){
        ;
    }
    if(lThInfo->buff[index] == '\015'){
        SMTPSendNeedMoreData(lThInfo->threadInfo.client, " should provide mailbox FROM");
        return 1;
    }
    int startOfName = index;
    for(index = startOfName; index < lThInfo->size && lThInfo->buff[index] > ' ' && lThInfo->buff[index] != '>'; index++){
        ;
    }
    int endOfName = index;
    int nameLen = endOfName - startOfName;
    if(lThInfo->smtpData->FROM != NULL){
        free(lThInfo->smtpData->FROM);
    }
    lThInfo->smtpData->FROM = calloc(nameLen+1, sizeof(char));
    memcpy(lThInfo->smtpData->FROM, &lThInfo->buff[startOfName], nameLen);
    PLTH_REPORT(lThInfo, "Письмо от <%s>\n", lThInfo->smtpData->FROM);

    return 0;
}

DWORD WINAPI SMTPCommandRCPTTO(LocalThreadInfo *lThInfo){
    int index = 0;
    for(index = 9; lThInfo->buff[index] != '\015' && (lThInfo->buff[index] == ' ' || lThInfo->buff[index] == '<'); index++){
        ;
    }
    if(lThInfo->buff[index] == '\015'){
        SMTPSendNeedMoreData(lThInfo->threadInfo.client, " should provide mailbox TO");
        return 1;
    }
    int startOfName = index;
    for(index = startOfName; index < lThInfo->size && lThInfo->buff[index] > ' ' && lThInfo->buff[index] != '>'; index++){
        ;
    }
    int endOfName = index;
    int nameLen = endOfName - startOfName;
    if(lThInfo->smtpData->TO != NULL){
        free(lThInfo->smtpData->TO);
    }
    lThInfo->smtpData->TO = calloc(nameLen+1, sizeof(char));
    memcpy(lThInfo->smtpData->TO, &lThInfo->buff[startOfName], nameLen);
    PLTH_REPORT(lThInfo, "Письмо для <%s>\n", lThInfo->smtpData->TO);

    return 0;
}

DWORD WINAPI SMTPCommandDATA(LocalThreadInfo *lThInfo){
    send(lThInfo->threadInfo.client, "354 provide message data. End with CRLF.CRLF\015\012", 46, 0x0);
    if(lThInfo->smtpData->DATA == NULL){
        lThInfo->smtpData->DATA = calloc(BUFF_SIZE, sizeof(char));
        lThInfo->smtpData->buffSize = BUFF_SIZE;
    }
    lThInfo->smtpData->dataSize = 0;
    while(1){
        int ret = recv(lThInfo->threadInfo.client, lThInfo->smtpData->DATA+lThInfo->smtpData->dataSize, BUFF_SIZE, 0x0);
        if(ret == SOCKET_ERROR || ret == 0){
            return 1;
        }
        lThInfo->smtpData->dataSize += ret;
        if(lThInfo->smtpData->buffSize - lThInfo->smtpData->dataSize < BUFF_SIZE){
            lThInfo->smtpData->buffSize = lThInfo->smtpData->buffSize + BUFF_SIZE*4;
            lThInfo->smtpData->DATA = (char*)realloc(lThInfo->smtpData->DATA, lThInfo->smtpData->buffSize);
        }
        lThInfo->smtpData->DATA[lThInfo->smtpData->dataSize] = '\0';
        if(strstr((lThInfo->smtpData->dataSize-ret-5 >= 0 ? lThInfo->smtpData->DATA+lThInfo->smtpData->dataSize-ret-5 : lThInfo->smtpData->DATA+lThInfo->smtpData->dataSize-ret), "\015\012.\015\012") != NULL){
            if(lThInfo->smtpData->dataSize > BUFF_SIZE*2){
                PLTH_REPORT(lThInfo, "Получено письмо. Оно не будет отображено, т.к имеет слишком большой размер");
            } else {
                PLTH_REPORT(lThInfo, "Получено письмо:\n%s", lThInfo->smtpData->DATA);
            }

            LOCK_FL();

            char buff[NAME_MAX_SIZE];
            memset(buff, 0x0, NAME_MAX_SIZE);
            memcpy(buff, lThInfo->smtpData->TO, strlen(lThInfo->smtpData->TO));
            FILE *f = fopen(MAIL_DATA_FILE, "rb");
            int nMessages = 0;
            if(f == NULL){
                fclose(f);
                f = fopen(MAIL_DATA_FILE, "wb");
                fwrite(&nMessages, sizeof(int), 1, f);
            }
            fclose(f);
            f = fopen(MAIL_DATA_FILE, "ab");
            fwrite(&lThInfo->smtpData->dataSize, sizeof(int), 1, f);
            fwrite(buff, NAME_MAX_SIZE, 1, f);
            fwrite(lThInfo->smtpData->DATA, lThInfo->smtpData->dataSize, 1, f);
            fclose(f);

            f = fopen(MAIL_DATA_FILE, "r+b");
            fread(&nMessages, sizeof(int), 1, f);
            nMessages++;
            fseek(f, 0, SEEK_SET);
            fwrite(&nMessages, sizeof(int), 1, f);
            fclose(f);

            UNLOCK_FL();
            return 0;
        }
    }
    return 1;
}

DWORD WINAPI SMTPCommandAUTHLOGIN(LocalThreadInfo *lThInfo){
    lThInfo->havePass = 0;
    lThInfo->haveUser = 0;
    send(lThInfo->threadInfo.client, "334 VXNlcm5hbWU6\015\012", 18, 0x0);
    ReadUntilCRLF(lThInfo->threadInfo.client, lThInfo->buff, &lThInfo->size);

    int nameLen = 0;
    char *name = Base64Decode(lThInfo->buff, lThInfo->size-2, &nameLen);
    PLTH_REPORT(lThInfo, "Декодированное имя: %*s\n", nameLen, name);

    send(lThInfo->threadInfo.client, "334 UGFzc3dvcmQ6\015\012", 18, 0x0);
    ReadUntilCRLF(lThInfo->threadInfo.client, lThInfo->buff, &lThInfo->size);
    int passLen = 0;
    char *pass = Base64Decode(lThInfo->buff, lThInfo->size-2, &passLen);
    PLTH_REPORT(lThInfo, "Декодированный пароль: %*s\n", passLen, pass);

    int index = 0;
    int retStatus = 0;
    LOCK_TH();
    for(index = 0; index < usersInList; index++){
        if(strncmp(glUserList[index].name, name, max(strlen(glUserList[index].name), nameLen)) == 0){
            if(glUserList[index].isLogged){
                PLTH_REPORT(lThInfo, "Попытка авторизации под именем активного на данный момент пользователя(%s)\n", glUserList[index].name);
                SMTPSendTempError(lThInfo->threadInfo.client, " User already logged in");
            }
            lThInfo->haveUser = 1;
            lThInfo->user = glUserList[index];
            if(strncmp(pass, lThInfo->user.pass, max(strlen(glUserList[index].name), passLen)) == 0){
                PLTH_REPORT(lThInfo, "Авторизация успешна\n");
                lThInfo->havePass = 1;
            } else{
                PLTH_REPORT(lThInfo, "Неверный пароль\n");
                SMTPSendTempError(lThInfo->threadInfo.client, " wrong password");
                retStatus = 1;
            }
            break;
        }
    }
    UNLOCK_TH();
    if(index == usersInList){
        PLTH_REPORT(lThInfo, "Пользователь(%*s) отсутствует\n", nameLen, name);
        SMTPSendTempError(lThInfo->threadInfo.client, " no such user");
        retStatus = 1;
    }
    free(pass);
    free(name);

    return retStatus;
}

DWORD WINAPI ProcessSMTP(LPVOID lpParameter){
    LocalThreadInfo lThInfo;
    InitLocalThreadInfo(&lThInfo, (ServerThread*)lpParameter, PROTOCOL_SMTP);

    PLTH_REPORT(&lThInfo, "Подключился новый клиент SMTP\n");

    send(lThInfo.threadInfo.client, "220\015\012", 5, 0x0);

    while(1){
        if(ReadUntilCRLF(lThInfo.threadInfo.client, lThInfo.buff, &lThInfo.size) != 0){
            PLTH_REPORT(&lThInfo, "Сокет закрыт\n");
            break;
        }
        PLTH_REPORT(&lThInfo, "Новое сообщение: %s", lThInfo.buff);

        if(IsServerCommand(&lThInfo, "EHLO")){
            if(SMTPCommandEHLO(&lThInfo) == 0){
                PLTH_REPORT(&lThInfo, "Доменное имя клиента: %s\n", lThInfo.domainName);
                SMTPSendOK(lThInfo.threadInfo.client, "-Mailserver");
                SMTPSendOK(lThInfo.threadInfo.client, "-AUTH LOGIN");
                SMTPSendOK(lThInfo.threadInfo.client, "-AUTH=LOGIN");
                SMTPSendOK(lThInfo.threadInfo.client, " HELP");
            }
        } else if(IsServerCommand(&lThInfo, "AUTH LOGIN")){
            if(SMTPCommandAUTHLOGIN(&lThInfo) == 0){
                send(lThInfo.threadInfo.client, "235\015\012", 5, 0x0);
            }
        } else if(IsServerCommand(&lThInfo, "QUIT")){
            SMTPSendOK(lThInfo.threadInfo.client, NULL);
            break;
        } else if(lThInfo.havePass){
            if(IsServerCommand(&lThInfo, "MAIL FROM:")){
                if(SMTPCommandMAILFROM(&lThInfo) == 0){
                    SMTPSendOK(lThInfo.threadInfo.client, NULL);
                }
            } else if(IsServerCommand(&lThInfo, "RCPT TO:")){
                if(SMTPCommandRCPTTO(&lThInfo) == 0){
                    SMTPSendOK(lThInfo.threadInfo.client, NULL);
                }
            } else if(IsServerCommand(&lThInfo, "DATA")){
                if(SMTPCommandDATA(&lThInfo) == 0){
                    SMTPSendOK(lThInfo.threadInfo.client, NULL);
                }
            } else{
                SMTPSendServerError(lThInfo.threadInfo.client, " Unknown command");
            }
        } else{
            LOCK_OUT();
            SMTPSendTempError(lThInfo.threadInfo.client, " Autorize first");
            UNLOCK_OUT();
        }
    }

    StopProcessingClient(&lThInfo);
    return 0;
}

DWORD WINAPI SMTPService(LPVOID lpParameter){
    LocalThreadInfo lThInfo;
    InitLocalThreadInfo(&lThInfo, (ServerThread*)lpParameter, PROTOCOL_SMTP);
    PLTH_REPORT(&lThInfo, "SMTP служба работает\n");
    while(keepRunning){
        SOCKET client = accept(lThInfo.threadInfo.client, NULL, NULL);
        int wasFound = 0;
        if(client != INVALID_SOCKET){
            LOCK_TH();
            for(int i = 0; i < MAX_CLIENTS; i++){
                if(glPool[i].isFree){
                    glPool[i].client = client;
                    glPool[i].isFree = 0;
                    glPool[i].handle = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)ProcessSMTP, (LPVOID)&glPool[i], 0, NULL);
                    wasFound = 1;
                    break;
                }
            }
            UNLOCK_TH();
            // Не нашелся свободный поток в пуле - отклоняем соединение
            if(wasFound == 0){
                SendERR(client, " Server thread glPool is full - try later");
                closesocket(client);
            }
        }
    }
    PLTH_REPORT(&lThInfo, "SMTP остановила прием новых сообщений\n");
    StopProcessingClient(&lThInfo);

    return 0;
}


// Нам не нужно синхронизировать приходы от SMTP и POP3 сессии, т.к. мы не будем выгружать в буферы
// сообщения пользователей во время старта сессии - а читать их по требованию
int main(int argc, char **argv){
    SetConsoleCP(CP_UTF8);
    SetConsoleOutputCP(CP_UTF8);
    glUserList = ReadUsersFromFile(USER_DATA_FILE, &usersInList);
    if(glUserList == NULL){
        MY_ERROR("No users!", -1);
    }
    printf("Список пользователей загружен\n");

    signal(SIGINT, intHandler);
    WSADATA wsaData;
	int status = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if(status != 0){
		MY_ERROR("WSAStartup failed", status);
	}

    pop3service = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if(pop3service == INVALID_SOCKET){
        MY_ERROR("Error during creating TCP socket", pop3service);
    }
    SOCKADDR_IN serverAddr = {.sin_family = AF_INET, .sin_port = htons(POP3_SERVER_PORT), .sin_addr.S_un.S_addr = INADDR_ANY};
    status = bind(pop3service, (PSOCKADDR)&serverAddr, sizeof(SOCKADDR_IN));
    if(status == SOCKET_ERROR){
        MY_ERROR("Error during binding POP3 socket - check port 110", status);
    }

    status = listen(pop3service, MAX_CLIENTS);
    if(status == SOCKET_ERROR){
        MY_ERROR("Error during start of listening POP3", status);
    }

    for(int i = 0; i < MAX_CLIENTS; i++){
        InitServerThread(&glPool[i]);
        glPool[i].id = i;
    }
    glOutputMutex = CreateMutex(NULL, FALSE, NULL);
    glThreadMutex = CreateMutex(NULL, FALSE, NULL);
    glFileMutex = CreateMutex(NULL, FALSE, NULL);
    printf("POP3 служба работает\n");

    glPool[SMTP_SERVICE].isFree = 0;
    glPool[SMTP_SERVICE].client = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    serverAddr.sin_port = htons(SMTP_SERVER_PORT);
    status = bind(glPool[SMTP_SERVICE].client, (PSOCKADDR)&serverAddr, sizeof(SOCKADDR_IN));
    if(status == SOCKET_ERROR){
        MY_ERROR("Error during binding SMTP socket - check port 25", status);
    }
    status = listen(glPool[SMTP_SERVICE].client, MAX_CLIENTS);
    if(status == SOCKET_ERROR){
        MY_ERROR("Error during start of listening SMTP", status);
    }
    glPool[SMTP_SERVICE].handle = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)SMTPService, (LPVOID)&glPool[SMTP_SERVICE], 0, NULL);

    while(keepRunning){
        SOCKET client = accept(pop3service, NULL, NULL);
        int wasFound = 0;
        if(client != INVALID_SOCKET){
            LOCK_TH();
            for(int i = 0; i < MAX_CLIENTS; i++){
                if(glPool[i].isFree){
                    glPool[i].client = client;
                    glPool[i].isFree = 0;
                    glPool[i].handle = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)ProcessPOP3, (LPVOID)&glPool[i], 0, NULL);
                    wasFound = 1;
                    break;
                }
            }
            UNLOCK_TH();
            // Не нашелся свободный поток в пуле - отклоняем соединение
            if(wasFound == 0){
                SendERR(client, " Server thread glPool is full - try later");
                closesocket(client);
            }
        }
    }
    // В конце мы можем проходиться по пулу потоков, если встретили занятый - ждем его окончания
    // Так до конца - это гарантированно закончит все потоки без проблем, т.к. мы не принимаем
    // новые соединения
    // Нужно учесть, что при завершении нужно также остановить прием SMTP сессией, когда они добавятся
    LOCK_OUT();
    printf("POP3 служба остановила прием новых соединений\n");
    printf("Ожидание завершения всех потоков. . .\n");
    UNLOCK_OUT();
    for(int i = 0; i < MAX_CLIENTS; i++){
        LOCK_TH();
        if(glPool[i].isFree == 0){
            UNLOCK_TH();
            WaitForSingleObject(glPool[i].handle, INFINITE);
        }
        UNLOCK_TH();
    }
    LOCK_OUT();
    printf("Сервер остановлен успешно\n");
    UNLOCK_OUT();

    CloseHandle(glFileMutex);
    CloseHandle(glOutputMutex);
    CloseHandle(glThreadMutex);
    WSACleanup();
    return 0;
}