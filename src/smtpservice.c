#include "smtpservice.h"

#include "base64.h"

void SMTPSocketSendOK(SOCKET sock, char *msg){
    send(sock, "250", 3, 0x0);
    if(msg != NULL){
        send(sock, msg, strlen(msg), 0x0);
    }
    send(sock, "\015\012", 2, 0x0);
}

void SMTPSocketSendNeedMoreData(SOCKET sock, char *msg){
    send(sock, "350", 3, 0x0);
    if(msg != NULL){
        send(sock, msg, strlen(msg), 0x0);
    }
    send(sock, "\015\012", 2, 0x0);
}

void SMTPSocketSendTempError(SOCKET sock, char *msg){
    send(sock, "450", 3, 0x0);
    if(msg != NULL){
        send(sock, msg, strlen(msg), 0x0);
    }
    send(sock, "\015\012", 2, 0x0);
}

void SMTPSocketSendServerError(SOCKET sock, char *msg){
    send(sock, "550", 3, 0x0);
    if(msg != NULL){
        send(sock, msg, strlen(msg), 0x0);
    }
    send(sock, "\015\012", 2, 0x0);
}

void SMTPSendOK(LocalThreadInfo *lThInfo, char *msg){
    send(lThInfo->threadInfo.client, "250", 3, 0x0);
    if(msg != NULL){
        send(lThInfo->threadInfo.client, msg, strlen(msg), 0x0);
    }
    send(lThInfo->threadInfo.client, "\015\012", 2, 0x0);

    WriteToSessionPrefix(&lThInfo->sessionLog, 1);
    WriteToSession(&lThInfo->sessionLog, "250", 3);
    if(msg != NULL){
        WriteToSession(&lThInfo->sessionLog, msg, strlen(msg));
    }
    WriteToSession(&lThInfo->sessionLog, "\015\012", 2);

    DisplaySession();
}

void SMTPSendNeedMoreData(LocalThreadInfo *lThInfo, char *msg){
    send(lThInfo->threadInfo.client, "350", 3, 0x0);
    if(msg != NULL){
        send(lThInfo->threadInfo.client, msg, strlen(msg), 0x0);
    }
    send(lThInfo->threadInfo.client, "\015\012", 2, 0x0);

    WriteToSessionPrefix(&lThInfo->sessionLog, 1);
    WriteToSession(&lThInfo->sessionLog, "350", 3);
    if(msg != NULL){
        WriteToSession(&lThInfo->sessionLog, msg, strlen(msg));
    }
    WriteToSession(&lThInfo->sessionLog, "\015\012", 2);

    DisplaySession();
}

void SMTPSendTempError(LocalThreadInfo *lThInfo, char *msg){
    send(lThInfo->threadInfo.client, "450", 3, 0x0);
    if(msg != NULL){
        send(lThInfo->threadInfo.client, msg, strlen(msg), 0x0);
    }
    send(lThInfo->threadInfo.client, "\015\012", 2, 0x0);

    WriteToSessionPrefix(&lThInfo->sessionLog, 1);
    WriteToSession(&lThInfo->sessionLog, "450", 3);
    if(msg != NULL){
        WriteToSession(&lThInfo->sessionLog, msg, strlen(msg));
    }
    WriteToSession(&lThInfo->sessionLog, "\015\012", 2);

    DisplaySession();
}

void SMTPSendServerError(LocalThreadInfo *lThInfo, char *msg){
    send(lThInfo->threadInfo.client, "550", 3, 0x0);
    if(msg != NULL){
        send(lThInfo->threadInfo.client, msg, strlen(msg), 0x0);
    }
    send(lThInfo->threadInfo.client, "\015\012", 2, 0x0);

    WriteToSessionPrefix(&lThInfo->sessionLog, 1);
    WriteToSession(&lThInfo->sessionLog, "550", 3);
    if(msg != NULL){
        WriteToSession(&lThInfo->sessionLog, msg, strlen(msg));
    }
    WriteToSession(&lThInfo->sessionLog, "\015\012", 2);

    DisplaySession();
}


DWORD WINAPI SMTPCommandEHLO(LocalThreadInfo *lThInfo){
    int index = 0;
    for(index = 4; lThInfo->buff[index] != '\015' && lThInfo->buff[index] == ' '; index++){
        ;
    }
    if(lThInfo->buff[index] == '\015'){
        SMTPSendNeedMoreData(lThInfo, "should provide user name");
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
        SMTPSendNeedMoreData(lThInfo, " should provide mailbox FROM");
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
    PLTH_REPORT(lThInfo, "Mail from <%s>\n", lThInfo->smtpData->FROM);

    return 0;
}

DWORD WINAPI SMTPCommandRCPTTO(LocalThreadInfo *lThInfo){
    int index = 0;
    for(index = 9; lThInfo->buff[index] != '\015' && (lThInfo->buff[index] == ' ' || lThInfo->buff[index] == '<'); index++){
        ;
    }
    if(lThInfo->buff[index] == '\015'){
        SMTPSendNeedMoreData(lThInfo, " should provide mailbox TO");
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
    PLTH_REPORT(lThInfo, "Mail to <%s>\n", lThInfo->smtpData->TO);

    return 0;
}

DWORD WINAPI SMTPCommandDATA(LocalThreadInfo *lThInfo){
    send(lThInfo->threadInfo.client, "354 provide message data. End with CRLF.CRLF\015\012", 46, 0x0);
    WriteToSessionPrefix(&lThInfo->sessionLog, 1);
    WriteToSession(&lThInfo->sessionLog, "354 provide message data. End with CRLF.CRLF\015\012", 46);
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
                PLTH_REPORT(lThInfo, "Got mail. No preview - mail is too big\n");
            } else {
                PLTH_REPORT(lThInfo, "Got mail. Preview:\n%s", lThInfo->smtpData->DATA);
            }

            WriteToSessionPrefix(&lThInfo->sessionLog, 0);
            WriteToSession(&lThInfo->sessionLog, lThInfo->smtpData->DATA, lThInfo->smtpData->dataSize);

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
    WriteToSessionPrefix(&lThInfo->sessionLog, 1);
    WriteToSession(&lThInfo->sessionLog, "334 VXNlcm5hbWU6\015\012", 18);
    DisplaySession();

    ReadUntilCRLF(lThInfo->threadInfo.client, lThInfo->buff, &lThInfo->size);

    int nameLen = 0;
    char *name = Base64Decode(lThInfo->buff, lThInfo->size-2, &nameLen);
    PLTH_REPORT(lThInfo, "Decoded username: %*s\n", nameLen, name);

    send(lThInfo->threadInfo.client, "334 UGFzc3dvcmQ6\015\012", 18, 0x0);
    WriteToSessionPrefix(&lThInfo->sessionLog, 1);
    WriteToSession(&lThInfo->sessionLog, "334 UGFzc3dvcmQ6\015\012", 18);
    DisplaySession();

    ReadUntilCRLF(lThInfo->threadInfo.client, lThInfo->buff, &lThInfo->size);
    int passLen = 0;
    char *pass = Base64Decode(lThInfo->buff, lThInfo->size-2, &passLen);
    PLTH_REPORT(lThInfo, "Decoded password: %*s\n", passLen, pass);

    int index = 0;
    int retStatus = 0;
    LOCK_TH();
    for(index = 0; index < usersInList; index++){
        if(strncmp(glUserList[index].name, name, max(strlen(glUserList[index].name), nameLen)) == 0){
            if(glUserList[index].isLogged){
                PLTH_REPORT(lThInfo, "User already logged in(%s)\n", glUserList[index].name);
                SMTPSendTempError(lThInfo, " User already logged in");
            }
            lThInfo->haveUser = 1;
            lThInfo->user = glUserList[index];
            if(strncmp(pass, lThInfo->user.pass, max(strlen(glUserList[index].name), passLen)) == 0){
                PLTH_REPORT(lThInfo, "AUTH OK\n");
                lThInfo->havePass = 1;
            } else{
                PLTH_REPORT(lThInfo, "Wrong password\n");
                SMTPSendTempError(lThInfo, " wrong password");
                retStatus = 1;
            }
            break;
        }
    }
    UNLOCK_TH();
    if(index == usersInList){
        PLTH_REPORT(lThInfo, "No such user(%*s)\n", nameLen, name);
        SMTPSendTempError(lThInfo, " no such user");
        retStatus = 1;
    }
    free(pass);
    free(name);

    return retStatus;
}

DWORD WINAPI ProcessSMTP(LPVOID lpParameter){
    LocalThreadInfo lThInfo;
    InitLocalThreadInfo(&lThInfo, (ServerThread*)lpParameter, PROTOCOL_SMTP, 0);

    PLTH_REPORT(&lThInfo, "Connected new SMTP client\n");

    send(lThInfo.threadInfo.client, "220\015\012", 5, 0x0);
    WriteToSessionPrefix(&lThInfo.sessionLog, 1);
    WriteToSession(&lThInfo.sessionLog, "220\015\012", 5);
    DisplaySession();

    while(1){
        if(ReadUntilCRLF(lThInfo.threadInfo.client, lThInfo.buff, &lThInfo.size) != 0){
            PLTH_REPORT(&lThInfo, "Socket was closed\n");
            break;
        }
        PLTH_REPORT(&lThInfo, "New message: %s", lThInfo.buff);

        if(IsServerCommand(&lThInfo, "EHLO")){
            if(SMTPCommandEHLO(&lThInfo) == 0){
                PLTH_REPORT(&lThInfo, "Client domain name: %s\n", lThInfo.domainName);
                SMTPSendOK(&lThInfo, "-Mailserver");
                SMTPSendOK(&lThInfo, "-AUTH LOGIN");
                SMTPSendOK(&lThInfo, "-AUTH=LOGIN");
                SMTPSendOK(&lThInfo, " HELP");
            }
        } else if(IsServerCommand(&lThInfo, "AUTH LOGIN")){
            if(SMTPCommandAUTHLOGIN(&lThInfo) == 0){
                send(lThInfo.threadInfo.client, "235\015\012", 5, 0x0);
                WriteToSessionPrefix(&lThInfo.sessionLog, 1);
                WriteToSession(&lThInfo.sessionLog, "235\015\012", 5);
                DisplaySession();
            }
        } else if(IsServerCommand(&lThInfo, "QUIT")){
            SMTPSendOK(&lThInfo, NULL);
            break;
        } else if(lThInfo.havePass){
            if(IsServerCommand(&lThInfo, "MAIL FROM:")){
                if(SMTPCommandMAILFROM(&lThInfo) == 0){
                    SMTPSendOK(&lThInfo, NULL);
                }
            } else if(IsServerCommand(&lThInfo, "RCPT TO:")){
                if(SMTPCommandRCPTTO(&lThInfo) == 0){
                    SMTPSendOK(&lThInfo, NULL);
                }
            } else if(IsServerCommand(&lThInfo, "DATA")){
                if(SMTPCommandDATA(&lThInfo) == 0){
                    SMTPSendOK(&lThInfo, NULL);
                }
            } else{
                SMTPSendServerError(&lThInfo, " Unknown command");
            }
        } else{
            LOCK_OUT();
            SMTPSendTempError(&lThInfo, " Autorize first");
            UNLOCK_OUT();
        }
    }

    StopProcessingClient(&lThInfo);
    return 0;
}

DWORD WINAPI SMTPService(LPVOID lpParameter){
    LocalThreadInfo lThInfo;
    InitLocalThreadInfo(&lThInfo, (ServerThread*)lpParameter, PROTOCOL_SMTP, 1);

    PLTH_REPORT(&lThInfo, "SMTP service is ON\n");
    while(keepRunning){
        SOCKET client = accept(lThInfo.threadInfo.client, NULL, NULL);
        int wasFound = 0;
        if(client != INVALID_SOCKET){
            LOCK_TH();
            for(int i = 0; i < MAX_CLIENTS; i++){
                if(glPool[i].isFree){
                    memset(&glPool[i], 0x0, sizeof(ServerThread));
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
                SMTPSocketSendTempError(client, " Server thread glPool is full - try later");
                closesocket(client);
            }
        }
    }
    PLTH_REPORT(&lThInfo, "SMTP service stopped accept new connections\n");
    StopProcessingClient(&lThInfo);

    return 0;
}

void SMTPUp(void){
    glPool[SMTP_SERVICE].isFree = 0;
    glPool[SMTP_SERVICE].client = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    SOCKADDR_IN serverAddr = {.sin_family = AF_INET, .sin_port = htons(SMTP_SERVER_PORT), .sin_addr.S_un.S_addr = INADDR_ANY};
    int status = bind(glPool[SMTP_SERVICE].client, (PSOCKADDR)&serverAddr, sizeof(SOCKADDR_IN));
    if(status == SOCKET_ERROR){
        MY_GUI_ERROR("Error during binding SMTP socket - check port 25", status);
    }
    status = listen(glPool[SMTP_SERVICE].client, MAX_CLIENTS);
    if(status == SOCKET_ERROR){
        MY_GUI_ERROR("Error during start of listening SMTP", status);
    }
}