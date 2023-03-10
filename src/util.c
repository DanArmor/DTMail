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

void InitLocalThreadInfo(LocalThreadInfo *lThInfo, ServerThread *thInfo, int protocol){
    LOCK_TH();
    lThInfo->pthreadInfo = thInfo;
    lThInfo->threadInfo = *thInfo;
    thInfo->pLocal = lThInfo;
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
    LoadThreadList();
}

void StopProcessingClient(LocalThreadInfo *lThInfo){
    // Лучше обращаться по указателю к глобальному пулу потоков
    // Т.к. мы все равно блочим мьютекс для обновления данных по доступности потока
    PLTH_REPORT(lThInfo, "Terminating.\nBuffer data:\n===\n%s===\n", lThInfo->buff);

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
    LoadThreadList();
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
