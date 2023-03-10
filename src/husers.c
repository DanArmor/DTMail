#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MY_ERROR(msg, status)\
    do{\
        fprintf(stderr, "ERROR: %s. Status: %d\n", msg, status);\
        exit(status);\
    }while(0)

#define BUFF_SIZE 26548
#define NAME_MAX_SIZE 256
#define PASS_MAX_SIZE 256
#define USER_DATA_FILE "users.data"

typedef struct UserInfo{
    char name[NAME_MAX_SIZE];
    char pass[PASS_MAX_SIZE];
    int id;
    int isLogged;
    int index;
} UserInfo;

void GetCommand(char *str, int *size) {
    char buffer[BUFF_SIZE];
    fgets(buffer, BUFF_SIZE, stdin);
    int index = strcspn(buffer, "\n");
    buffer[index] = '\0';
    strcpy(str, buffer);
    *size = index;
}

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
    printf("Size of the file: %d\n", filesize);
    char *fbuff = malloc(filesize);
    fseek(f, 0L, SEEK_SET);
    int readed = fread(fbuff, filesize, 1, f);
    fclose(f);

    printf("What you want to do:\n1. Create user\n2. Delete user\n3. Show list of users\n");
    char buff[512];
    int buffsize = 0;
    int nUsers = *((int*)fbuff);
    GetCommand(buff, &buffsize);
    if(buff[0] == '1'){
        UserInfo user;
        memset(&user, 0x0, sizeof(UserInfo));
        printf("Enter username: ");
        GetCommand(buff, &buffsize);
        memcpy(user.name, buff, buffsize);
        printf("Enter password: ");
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