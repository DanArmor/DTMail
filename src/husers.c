#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "util.h"

int main(void){
    srand(time(NULL) + clock());
    FILE *f = fopen(USER_DATA_FILE, "rb");
    if(f == NULL){
        f = fopen(USER_DATA_FILE, "wb");
        int n = 0;
        fwrite(&n, sizeof(int), 1, f);
        if(f == NULL){
            MY_ERROR("Can't create user data file", -1);
        }
        fclose(f);
        f = fopen(USER_DATA_FILE, "rb");
    }
    fseek(f, 0L, SEEK_END);
    int filesize = ftell(f);
    printf("Размер файла: %d\n", filesize);
    char *fbuff = malloc(filesize);
    fseek(f, 0L, SEEK_SET);
    int readed = fread(fbuff, filesize, 1, f);
    printf("Было прочитано пакетов: %d\n", readed);
    fclose(f);

    printf("Что вы хотите сделать:\n1. Создать пользователя\n2. Удалить пользователя\n3. Показать список пользователей\n");
    char buff[512];
    int buffsize = 0;
    int nUsers = *((int*)fbuff);
    GetCommand(buff, &buffsize);
    if(buff[0] == '1'){
        UserInfo user;
        memset(&user, 0x0, sizeof(UserInfo));
        printf("Введите username: ");
        GetCommand(buff, &buffsize);
        memcpy(user.name, buff, buffsize);
        printf("Введите password: ");
        GetCommand(buff, &buffsize);
        memcpy(user.pass, buff, buffsize);
        user.id = rand()%213412317;
        FILE *f = fopen(USER_DATA_FILE, "wb");
        nUsers++;
        fwrite(&nUsers, sizeof(int), 1, f);
        fwrite(fbuff+sizeof(int), filesize-sizeof(int), 1, f);
        fwrite(user.name, NAME_MAX_SIZE, 1, f);
        fwrite(user.pass, PASS_MAX_SIZE, 1, f);
        fwrite(&user.id, sizeof(int), 1, f);
        fclose(f);
    } else if(buff[0] == '2'){
        if(nUsers == 0){
            free(fbuff);
            MY_ERROR("No users", buff[0]);
        }
        char *dataP = (char*)(fbuff+sizeof(int));
        for(int i = 0; i < nUsers; i++){
            printf("#%d User: %s Pass: %s  ID: %d\n", i, dataP, dataP+NAME_MAX_SIZE, *(int*)(dataP+NAME_MAX_SIZE+PASS_MAX_SIZE));
            dataP += NAME_MAX_SIZE + PASS_MAX_SIZE + sizeof(int);
        }
        GetCommand(buff, &buffsize);
        int index = -1;
        sscanf(buff, "%d", &index);
        if(index < 0 || index >= nUsers){
            free(fbuff);
            MY_ERROR("Error during reading of index", index);
        }
        FILE *f = fopen(USER_DATA_FILE, "wb");
        nUsers--;
        dataP = (char*)(fbuff+sizeof(int));
        fwrite(&nUsers, sizeof(int), 1, f);
        for(int i = 0; i < nUsers; i++){
            if(i == index){
                continue;
            }
            fwrite(dataP, NAME_MAX_SIZE, 1, f);
            fwrite(dataP+NAME_MAX_SIZE, PASS_MAX_SIZE, 1, f);
            fwrite(dataP+NAME_MAX_SIZE+PASS_MAX_SIZE, sizeof(int), 1, f);
            dataP += NAME_MAX_SIZE + PASS_MAX_SIZE + sizeof(int);
        }
        fclose(f);
    } else if(buff[0] == '3'){
        if(nUsers == 0){
            free(fbuff);
            MY_ERROR("No users", buff[0]);
        }
        char *dataP = (char*)(fbuff+sizeof(int));
        for(int i = 0; i < nUsers; i++){
            printf("#%d User: %s Pass: %s  ID: %d\n", i, dataP, dataP+NAME_MAX_SIZE, *(int*)(dataP+NAME_MAX_SIZE+PASS_MAX_SIZE));
            dataP += NAME_MAX_SIZE + PASS_MAX_SIZE + sizeof(int);
        }
    } else{
        free(fbuff);
        MY_ERROR("Wrong option", buff[0]);
    }
}