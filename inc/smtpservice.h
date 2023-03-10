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

void SMTPSendOK(LocalThreadInfo *lThInfo, char *msg);
void SMTPSendNeedMoreData(LocalThreadInfo *lThInfo, char *msg);
void SMTPSendTempError(LocalThreadInfo *lThInfo, char *msg);
void SMTPSendServerError(LocalThreadInfo *lThInfo, char *msg);

void SMTPSocketSendOK(SOCKET sock, char *msg);
void SMTPSocketSendNeedMoreData(SOCKET sock, char *msg);
void SMTPSocketSendTempError(SOCKET sock, char *msg);
void SMTPSocketSendServerError(SOCKET sock, char *msg);



#endif