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
#include "pop3service.h"
#include "smtpservice.h"

#include "iup.h"

char errorBuffer[512];

int keepRunning = 1;
int isGuiRunning = 1;
int flagNOGUI = 0;

HANDLE glOutputMutex;
HANDLE glThreadMutex;
HANDLE glFileMutex;
HANDLE glGUIMutex;
ServerThread glPool[MAX_CLIENTS];

// GUI
Ihandle *glGUIThreadList;
Ihandle *glGUIMainBox;
Ihandle *glGUIMainDlg;
Ihandle *glGUIUpdateListButton;

UserInfo *glUserList;
int usersInList;

void intHandler(int notUsed){
    LOCK_OUT();
    fprintf(stderr, "Server is stopping. . .\n");
    UNLOCK_OUT();

    LOCK_TH();
    keepRunning = 0;
    closesocket(glPool[SMTP_SERVICE].client);
    closesocket(glPool[POP3_SERVICE].client);
    UNLOCK_TH();
}


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

// Нам не нужно синхронизировать приходы от SMTP и POP3 сессии, т.к. мы не будем выгружать в буферы
// сообщения пользователей во время старта сессии - а читать их по требованию
int main(int argc, char **argv){
    if(argc != 1){
        if(strncmp(argv[1], "-NO-GUI", 8) == 0){
            flagNOGUI = 1;
            isGuiRunning = 0;
        }
    }
    if(flagNOGUI == 0){
        if(IupOpen(&argc, &argv) == IUP_ERROR){
            fprintf(stderr, "Error opening IUP.A");
            return 1;
        }
    }

    glUserList = ReadUsersFromFile(USER_DATA_FILE, &usersInList);
    if(glUserList == NULL){
        MY_GUI_ERROR("No users!", -1);
    }
    fprintf(stderr, "User list was loaded\n");
    
    signal(SIGINT, intHandler);
    WSADATA wsaData;
	int status = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if(status != 0){
		MY_GUI_ERROR("WSAStartup failed", status);
	}


    for(int i = 0; i < MAX_CLIENTS; i++){
        InitServerThread(&glPool[i]);
        glPool[i].id = i;
    }
    glOutputMutex = CreateMutex(NULL, FALSE, NULL);
    glThreadMutex = CreateMutex(NULL, FALSE, NULL);
    glFileMutex = CreateMutex(NULL, FALSE, NULL);
    glGUIMutex = CreateMutex(NULL, FALSE, NULL);

    POP3Up();
    SMTPUp();

    glPool[SMTP_SERVICE].handle = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)SMTPService, (LPVOID)&glPool[SMTP_SERVICE], 0, NULL);
    glPool[POP3_SERVICE].handle = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)POP3Service, (LPVOID)&glPool[POP3_SERVICE], 0, NULL);

    LOCK_TH();
    HANDLE POP3_handle = glPool[POP3_SERVICE].handle;
    HANDLE SMTP_handle = glPool[SMTP_SERVICE].handle;
    UNLOCK_TH();

    if(flagNOGUI == 0){
        glGUIThreadList = IupList(NULL);
        IupSetAttribute(glGUIThreadList, "NAME", "LST_THREAD");
        IupSetAttribute(glGUIThreadList, "EXPAND", "YES");
        IupSetAttribute(glGUIThreadList, "SIZE", "300x");
        IupSetAttribute(glGUIThreadList, "VISIBLELINES", "6");

        glGUIUpdateListButton = IupButton("Update", NULL);
        IupSetAttribute(glGUIUpdateListButton, "SIZE", "30");
        IupSetCallback(glGUIUpdateListButton, "ACTION", (Icallback)LoadThreadList);

        glGUIMainBox = IupVbox(glGUIThreadList, glGUIUpdateListButton, NULL);

        IupSetAttribute(glGUIMainBox, "NMAGRIN", "10x10");

        glGUIMainDlg = IupDialog(glGUIMainBox);
        IupSetAttribute(glGUIMainDlg, "TITLE", "DTMail");
        IupSetAttribute(glGUIMainDlg, "GAP", "10");

        IupShowXY(glGUIMainDlg, IUP_CENTER, IUP_CENTER);

        IupMainLoop();
        LOCK_GUI();
        LOCK_TH();
        isGuiRunning = 0;
        UNLOCK_GUI();
        UNLOCK_TH();
    }

    WaitForSingleObject(POP3_handle, INFINITE);
    WaitForSingleObject(SMTP_handle, INFINITE);

    // В конце мы можем проходиться по пулу потоков, если встретили занятый - ждем его окончания
    // Так до конца - это гарантированно закончит все потоки без проблем, т.к. мы не принимаем
    // новые соединения

    LOCK_OUT();
    printf("Waiting for all threads to terminate. . .\n");
    UNLOCK_OUT();
    int foundRunning = 0;
    while(1){
        for(int i = 0; i < MAX_CLIENTS; i++){
            LOCK_TH();
            if(glPool[i].isFree == 0){
                UNLOCK_TH();
                foundRunning = 1;
                WaitForSingleObject(glPool[i].handle, INFINITE);
            }
            UNLOCK_TH();
        }
        if(foundRunning == 0){
            break;
        }
    }
    LOCK_OUT();
    printf("Server was stopped successfully\n");
    UNLOCK_OUT();

    CloseHandle(glFileMutex);
    CloseHandle(glOutputMutex);
    CloseHandle(glThreadMutex);
    CloseHandle(glGUIMutex);
    WSACleanup();

    if(flagNOGUI == 0){
        IupClose();
    }
    return 0;
}