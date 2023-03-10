#include "util.h"

#include <stdint.h>
#include <stdlib.h>

int maxFunc(int a, int b){
    return a > b ? a : b;
}

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

void WriteToSession(SessionLog *sessionLog, char *msg, int size){
    if(sessionLog->size + size < sessionLog->buffSize){
        sessionLog->buffSize = sessionLog->buffSize + size + BUFF_SIZE * 4;
        sessionLog->session = realloc(sessionLog->session, sessionLog->buffSize);
    }
    memcpy(sessionLog->session+sessionLog->size, msg, size);
    sessionLog->size = sessionLog->size + size;
    sessionLog->session[sessionLog->size] = '\0';
}

void WriteToSessionPrefix(SessionLog *sessionLog, int isServer){
    if(isServer){
        WriteToSession(sessionLog, "SERVER: ", 8);
    } else{
        WriteToSession(sessionLog, "CLIENT: ", 8);
    }
}

void InitServerThread(ServerThread *serverThread){
    serverThread->handle = 0;
    serverThread->client = 0;
    serverThread->isFree = 1;
}

void InitSMTPData(SMTPData *smtpData){
    smtpData->FROM = NULL;
    smtpData->TO = NULL;
    smtpData->DATA = NULL;
    smtpData->dataSize = 0;
    smtpData->buffSize = 0;
}

void InitSessionLog(LocalThreadInfo *lThInfo){
    lThInfo->sessionLog.buffSize = BUFF_SIZE * 4;
    lThInfo->sessionLog.size = 0;
    lThInfo->sessionLog.session = malloc(lThInfo->sessionLog.buffSize);
    
    LOCK_TH();
    lThInfo->sessionLog.id = glSessionsN;
    glSessionsN++;
    UNLOCK_TH();

    lThInfo->sessionLog.isDead = 0;
    lThInfo->sessionLog.protocol = lThInfo->protocol;
}

void InitLocalThreadInfo(LocalThreadInfo *lThInfo, ServerThread *thInfo, int protocol, int isService){
    LOCK_TH();
    memset(lThInfo, 0x0, sizeof(LocalThreadInfo));

    lThInfo->pthreadInfo = thInfo;
    lThInfo->threadInfo = *thInfo;
    lThInfo->isService = isService;
    thInfo->pLocal = lThInfo;

    lThInfo->havePass = 0;
    lThInfo->haveUser = 0;
    lThInfo->size = 0;

    lThInfo->buff = (char*)calloc(BUFF_SIZE, sizeof(char));

    lThInfo->protocol = protocol;
    if(protocol == PROTOCOL_SMTP){
        lThInfo->domainName = calloc(NAME_MAX_SIZE, sizeof(char));
        lThInfo->smtpData = calloc(1, sizeof(SMTPData));
        InitSMTPData(lThInfo->smtpData);
    }

    InitSessionLog(lThInfo);

    WriteToSession(&lThInfo->sessionLog, "======START OF SESSION======\015\012", 30);

    UNLOCK_TH();

    LoadThreadList();
    DisplaySession();
}

void MigrateDeadSession(SessionLog *sessionLog){
    glDeadSessionsN++;
    glDeadSessions = realloc(glDeadSessions, sizeof(SessionLog) * glDeadSessionsN);
    glDeadSessions[glDeadSessionsN-1] = *sessionLog;
}

void StopProcessingClient(LocalThreadInfo *lThInfo){
    // Лучше обращаться по указателю к глобальному пулу потоков
    // Т.к. мы все равно блочим мьютекс для обновления данных по доступности потока
    LOCK_TH();
    PLTH_REPORT(lThInfo, "Terminating.\nBuffer data:\n===\n%s===\n", lThInfo->buff);

    lThInfo->sessionLog.isDead = 1;
    WriteToSession(&lThInfo->sessionLog, "======END OF SESSION======\015\012", 28);
    MigrateDeadSession(&lThInfo->sessionLog);
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

    LoadThreadList();
    DisplaySession();
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
