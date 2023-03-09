#include "guiutil.h"

int LoadThreadList(void){
    LOCK_TH();
    LOCK_GUI();
    if(isGuiRunning){
        IupSetAttribute(glGUIThreadList, "1", NULL);
        int j = 1;
        for(int i = 0; i  < MAX_CLIENTS; i++){
            if(glPool[i].isFree == 0){
                IupSetStrfId(glGUIThreadList, "", j, "Thread #%d ", glPool[i].id);
                IupSetIntId(glGUIThreadList, "ID", j, glPool[i].id);
                j++;
            }
        }
        IupSetAttribute(glGUIThreadList, "VALUE", NULL);
    }
    UNLOCK_GUI();
    UNLOCK_TH();
    return IUP_DEFAULT;
}
