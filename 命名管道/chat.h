#ifndef _CHAT_H
#define _CHAT_H 

#include<errno.h>
#include<unistd.h>
#include<stdlib.h>
#include<string.h>
#include<sys/types.h>
#include<sys/stat.h>
#include<fcntl.h>
#include<signal.h>
#include<sys/select.h>
#include<iostream>
#include<string>
#include<regex>
#include<fstream>
#include<map>
#include<pthread.h>
using namespace std;

#define SERVER_FIFO "/home/jiageng2016150026/Chat/server/pipe/"
#define CLIENT_FIFO "/home/jiageng2016150026/Chat/client/pipe/"
#define TMP_FIFO "/home/jiageng2016150026/Chat/tmp/"
#define CONFIGFILE "/home/jiageng2016150026/Chat/config/chat.cnf"

#define CHECK_NAME 1
#define CHECK_IN 2

#define REGISTER  3
#define LOGIN  4
#define CHAT  5
#define TMP  6
#define MYFIFO  7

typedef struct{
    char myfifo[64];
    char name[32];
    char password[32];
    int type;
    int pid;
} CLIENT, *PCLIENT;

typedef struct{
    int flag;
    char message[256];
} REPLY, *PREPLY;

typedef struct{
    char from[32];
    char message[128];
} OFFMES, *POFFMES;

typedef struct{
    int num;
    char from[32];
    char to[4][32];
    char message[128];
} MESSAGE, *PMESSAGE;
#endif