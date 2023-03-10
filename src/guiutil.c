#include "guiutil.h"

int LoadThreadList(void){
    GUI_START_SECTION
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
    GUI_END_SECTION;
}

int DisplaySession(void){
    GUI_START_SECTION;
        

    GUI_END_SECTION;
}
