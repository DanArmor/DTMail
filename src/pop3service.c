#include "pop3service.h"

BOOL CheckStatus(char *buff){
    if(strncmp(buff, "+OK", 3) == 0){
        return TRUE;
    } else if(strncmp(buff, "-ERR", 4) == 0){
        return FALSE;
    }
}

void SocketSendERR(SOCKET client, char *msg){
    send(client, "-ERR", 4, 0x0);
    if(msg != NULL){
        send(client, msg, strlen(msg), 0x0);
    }
    send(client, "\015\012", 2, 0x0);
}

void SocketSendOK(SOCKET client, char *msg){
    send(client, "+OK", 3, 0x0);
    if(msg != NULL){
        send(client, msg, strlen(msg), 0x0);
    }
    send(client, "\015\012", 2, 0x0);
}

void SendERR(LocalThreadInfo *lThInfo, char *msg){
    send(lThInfo->threadInfo.client, "-ERR", 4, 0x0);
    if(msg != NULL){
        send(lThInfo->threadInfo.client, msg, strlen(msg), 0x0);
    }
    send(lThInfo->threadInfo.client, "\015\012", 2, 0x0);

    WriteToSessionPrefix(&lThInfo->sessionLog, 1);
    WriteToSession(&lThInfo->sessionLog, "-ERR", 4);
    if(msg != NULL){
        WriteToSession(&lThInfo->sessionLog, msg, strlen(msg));
    }
    WriteToSession(&lThInfo->sessionLog, "\015\012", 2);

    DisplaySession();
}

void SendOK(LocalThreadInfo *lThInfo, char *msg){
    send(lThInfo->threadInfo.client, "+OK", 3, 0x0);
    if(msg != NULL){
        send(lThInfo->threadInfo.client, msg, strlen(msg), 0x0);
    }
    send(lThInfo->threadInfo.client, "\015\012", 2, 0x0);

    WriteToSessionPrefix(&lThInfo->sessionLog, 1);
    WriteToSession(&lThInfo->sessionLog, "+OK", 3);
    if(msg != NULL){
        WriteToSession(&lThInfo->sessionLog, msg, strlen(msg));
    }
    WriteToSession(&lThInfo->sessionLog, "\015\012", 2);

    DisplaySession();
}

DWORD POP3CommandUSER(LocalThreadInfo *lThInfo){
    lThInfo->havePass = 0;
    lThInfo->haveUser = 0;
    if(lThInfo->size < 8){
        PLTH_REPORT(lThInfo, "No username in message\n");
        SendERR(lThInfo, " should provide username");
        return 1;
    }
    int index = 0;
    for(index = 4; lThInfo->buff[index] != '\015' && lThInfo->buff[index] == ' '; index++){
        ;
    }
    if(lThInfo->buff[index] == '\015'){
        PLTH_REPORT(lThInfo, "No username in message\n");
        SendERR(lThInfo, " should provide user name");
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
        if(nameLen == strlen(glUserList[index].name) && strncmp(glUserList[index].name, &lThInfo->buff[startOfName], nameLen) == 0){
            if(glUserList[index].isLogged){
                PLTH_REPORT(lThInfo, "User is already logged in(%s)\n", glUserList[index].name);
                SendERR(lThInfo, " User already logged in");
                return 1;
            }
            SendOK(lThInfo, NULL);
            lThInfo->user = glUserList[index];
            lThInfo->haveUser = TRUE;
            PLTH_REPORT(lThInfo, "User was found\n");
            break;
        }
    }
    UNLOCK_TH();
    if(index == usersInList){
        char back = lThInfo->buff[endOfName];
        lThInfo->buff[endOfName] = '\0';
        PLTH_REPORT(lThInfo, "No such user(%s)\n", lThInfo->buff+startOfName);
        lThInfo->buff[endOfName] = back;
        SendERR(lThInfo, " no such user");
        return 1;
    }
    return 0;
}

DWORD POP3CommandPASS(LocalThreadInfo *lThInfo){
    if(lThInfo->haveUser == 0){
        SendERR(lThInfo, " Send USER first");
        return 1;
    }
    if(lThInfo->size < 8){
        PLTH_REPORT(lThInfo, "No password in message\n");
        SendERR(lThInfo, " should provide password");
        return 1;
    }
    int index = 0;
    for(index = 4; lThInfo->buff[index] != '\015' && lThInfo->buff[index] == ' '; index++){
        ;
    }
    if(lThInfo->buff[index] == '\015'){
        PLTH_REPORT(lThInfo, "No password in message\n");
        SendERR(lThInfo, " should provide password");
        return 1;
    }
    int startOfPass = index;
    for(index = startOfPass; index < lThInfo->size && lThInfo->buff[index] > ' '; index++){
        ;
    }
    int endOfPass = index;
    int passLen = endOfPass - startOfPass;
    if(strncmp(&lThInfo->buff[startOfPass], lThInfo->user.pass, maxFunc(strlen(lThInfo->user.pass), passLen)) == 0){
        PLTH_REPORT(lThInfo, "AUTH OK\n");
        SendOK(lThInfo, " Logged in");
        return 0;
    }
    PLTH_REPORT(lThInfo, "Wrong password\n");
    SendERR(lThInfo, " Wrong password");
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
    SendOK(lThInfo, buff);
    int index = 1;
    for(int i = 0; i < nMessages; i++){
        fread(&dataSize, sizeof(int), 1, f);
        fread(buff, NAME_MAX_SIZE, 1, f);
        if(strcmp(lThInfo->user.name, buff) == 0){
            sprintf(buff, "%d %d\015\012", index, dataSize);
            index++;
            send(lThInfo->threadInfo.client, buff, strlen(buff), 0x0);
            WriteToSession(&lThInfo->sessionLog, buff, strlen(buff));
        }
        fseek(f, dataSize, SEEK_CUR);
    }

    fclose(f);
    UNLOCK_FL();

    send(lThInfo->threadInfo.client, ".\015\012", 3, 0x0);
    WriteToSession(&lThInfo->sessionLog, ".\015\012", 3);
    DisplaySession();

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
                SendOK(lThInfo, buff);
                char *dataP = malloc(dataSize);
                fread(dataP, dataSize, 1, f);
                send(lThInfo->threadInfo.client, dataP, dataSize, 0x0);
                WriteToSession(&lThInfo->sessionLog, dataP, dataSize);
                DisplaySession();
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
        SendERR(lThInfo, " No msg with such index");
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
        SendOK(lThInfo, NULL);
    } else{
        remove(MAIL_SUPPORT_FILE);
        SendERR(lThInfo, " No msg with such index");
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
    SendOK(lThInfo, buff);
    PLTH_REPORT(lThInfo, "STAT OK -%s\n", buff);

    return 0;
}

DWORD WINAPI ProcessPOP3(LPVOID lpParameter){
    LocalThreadInfo lThInfo;
    InitLocalThreadInfo(&lThInfo, (ServerThread*)lpParameter, PROTOCOL_POP3, 0);

    PLTH_REPORT(&lThInfo, "Connected new POP3 client\n");

    SendOK(&lThInfo, NULL);
    while(1){
        if(ReadUntilCRLF(lThInfo.threadInfo.client, lThInfo.buff, &lThInfo.size) != 0){
            PLTH_REPORT(&lThInfo, "Socket was closed\n");
            break;
        }

        PLTH_REPORT(&lThInfo, "New message: %s", lThInfo.buff);
        WriteToSessionPrefix(&lThInfo.sessionLog, 0);
        WriteToSession(&lThInfo.sessionLog, lThInfo.buff, lThInfo.size);

        if(IsServerCommand(&lThInfo, "USER")){
            POP3CommandUSER(&lThInfo);
        } else if(IsServerCommand(&lThInfo, "PASS")){
            if(lThInfo.haveUser == 0){
                SendERR(&lThInfo, " Provide USER first");
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
                SendOK(&lThInfo, NULL);
                break;
            } else if(IsServerCommand(&lThInfo, "RETR")){
                POP3CommandRETR(&lThInfo);
            } else if(IsServerCommand(&lThInfo, "DELE")){
                POP3CommandDELE(&lThInfo);
            } else if(IsServerCommand(&lThInfo, "TOP")){
                SendERR(&lThInfo, " not supported");
            } else{
                SendERR(&lThInfo, " Unknown command");
            } 
        } else{
            SendERR(&lThInfo, " AUTH first");
        }
    }

    StopProcessingClient(&lThInfo);

    return 0;
}

DWORD WINAPI POP3Service(LPVOID lpParameter){
    LocalThreadInfo lThInfo;
    InitLocalThreadInfo(&lThInfo, (ServerThread*)lpParameter, PROTOCOL_POP3, 1);

    PLTH_REPORT(&lThInfo, "POP3 service is ON\n");
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
                    glPool[i].handle = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)ProcessPOP3, (LPVOID)&glPool[i], 0, NULL);
                    wasFound = 1;
                    break;
                }
            }
            UNLOCK_TH();
            // Не нашелся свободный поток в пуле - отклоняем соединение
            if(wasFound == 0){
                SocketSendERR(client, " Server thread glPool is full - try later");
                closesocket(client);
            }
        }
    }
    PLTH_REPORT(&lThInfo, "POP3 service stopped accept new connections\n");
    StopProcessingClient(&lThInfo);

    return 0;
}

void POP3Up(void){
    glPool[POP3_SERVICE].isFree = 0;
    glPool[POP3_SERVICE].client = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if(glPool[POP3_SERVICE].client == INVALID_SOCKET){
        MY_GUI_ERROR("Error during creating TCP socket", glPool[POP3_SERVICE].client);
    }
    SOCKADDR_IN serverAddr = {.sin_family = AF_INET, .sin_port = htons(POP3_SERVER_PORT), .sin_addr.S_un.S_addr = INADDR_ANY};
    int status = bind(glPool[POP3_SERVICE].client, (PSOCKADDR)&serverAddr, sizeof(SOCKADDR_IN));
    if(status == SOCKET_ERROR){
        MY_GUI_ERROR("Error during binding POP3 socket - check port 110", status);
    }

    status = listen(glPool[POP3_SERVICE].client, MAX_CLIENTS);
    if(status == SOCKET_ERROR){
        MY_GUI_ERROR("Error during start of listening POP3", status);
    }
}