#ifndef _INC_POP3_SERVICE_H
#define _INC_POP3_SERVICE_H

#include "util.h"

DWORD POP3CommandUSER(LocalThreadInfo *lThInfo);

DWORD POP3CommandPASS(LocalThreadInfo *lThInfo);

DWORD WINAPI POP3CommandLIST(LocalThreadInfo *lThInfo);

DWORD WINAPI POP3CommandRETR(LocalThreadInfo *lThInfo);

DWORD WINAPI POP3CommandDELE(LocalThreadInfo *lThInfo);

DWORD WINAPI POP3CommandSTAT(LocalThreadInfo *lThInfo);

DWORD WINAPI ProcessPOP3(LPVOID lpParameter);

DWORD WINAPI POP3Service(LPVOID lpParameter);

void POP3Up(void);

#endif