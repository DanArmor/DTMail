#ifndef _INC_SMTP_SERVICE_H
#define _INC_SMTP_SERVICE_H

#include "util.h"

DWORD WINAPI SMTPCommandEHLO(LocalThreadInfo *lThInfo);

DWORD WINAPI SMTPCommandMAILFROM(LocalThreadInfo *lThInfo);

DWORD WINAPI SMTPCommandRCPTTO(LocalThreadInfo *lThInfo);

DWORD WINAPI SMTPCommandDATA(LocalThreadInfo *lThInfo);

DWORD WINAPI SMTPCommandAUTHLOGIN(LocalThreadInfo *lThInfo);

DWORD WINAPI ProcessSMTP(LPVOID lpParameter);

DWORD WINAPI SMTPService(LPVOID lpParameter);

void SMTPUp(void);

void SMTPSendOK(SOCKET sock, char *msg);
void SMTPSendNeedMoreData(SOCKET sock, char *msg);
void SMTPSendTempError(SOCKET sock, char *msg);
void SMTPSendServerError(SOCKET sock, char *msg);


#endif