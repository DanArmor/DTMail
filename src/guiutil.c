#include "util.h"

int LoadThreadList(void){
    GUI_START_SECTION
        char buff[64];
        char *value = IupGetAttribute(glGUIThreadList, "VALUE");
        int intValue = -1;
        int found = 0;
        if(value != NULL && *value != '0'){
            sscanf(value, "%d", &intValue); //session.id
            intValue = IupGetIntId(glGUIThreadList, "ID", intValue);
        }
        IupSetAttribute(glGUIThreadList, "1", NULL);
        int j = 1;
        for(int i = 0; i  < MAX_CLIENTS; i++){
            if(glPool[i].isFree == 0 && glPool[i].pLocal != NULL && glPool[i].pLocal->isService == 0){
                IupSetStrfId(glGUIThreadList, "", j, "Session #%d ", glPool[i].pLocal->sessionLog.id);
                IupSetIntId(glGUIThreadList, "ID", j, glPool[i].pLocal->sessionLog.id);
                if(found != 1 && intValue != -1 && glPool[i].pLocal->sessionLog.id == intValue){
                    found = 1;
                    intValue = j;
                }
                j++;
            }
        }
        for(int i = 0; i < glDeadSessionsN; i++){
            IupSetStrfId(glGUIThreadList, "", j, "Session (DEAD) #%d ", glDeadSessions[i]);
            IupSetIntId(glGUIThreadList, "ID", j, glDeadSessions[i].id);
            if(found != 1 && intValue != -1 && glDeadSessions[i].id == intValue){
                found = 1;
                intValue = j;
            }
            j++;
        }
        if(found == 0){
            IupSetAttribute(glGUIThreadList, "VALUE", NULL);
        } else {
            sprintf(buff, "%d", intValue);
            IupSetAttribute(glGUIThreadList, "VALUE", buff);
        }
    GUI_END_SECTION;
}

int DisplaySession(void){
    GUI_START_SECTION;
        

    GUI_END_SECTION;
}
