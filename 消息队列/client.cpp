#include "chat.h"
#include<stdio.h>

char buffer[64];

string name;
pthread_t readThread;
int msgkey, msgid;
int registerID, loginID, chatID;
void handler(int sig);
void * readMess(void * arg);

char getch()
{
    char c;
    system("stty -echo");
    system("stty -icanon");
    c=getchar();
    system("stty icanon");
    system("stty echo");
    return c;
}

void getPassword(char *array)
{
    int i=0;
    while((array[i]=getch()) != '\n')
	{
        if(array[i]>31 && array[i]<127)
        {
            putchar('*');
		    i++;
        }
	}
    array[i]='\0';
    cout << endl;
}

void initKey(key_t key, int msgflg, int * link)
{
    int res = msgget(key, msgflg);
    if(res==-1)
    {
        perror("Init_Key");
        exit(errno);
    }
    if(link!=NULL)
        *link = res;
}

void Register()
{
    int length;
    CLIENT client;
    REPLY reply;
    reply.flag=0;
    client.msgkey = msgkey;
    cout << "\033[33mInput the name or input 'q' to quit:\033[0m" << endl;

    while(!reply.flag)
    {
        cin >> client.name;
        if(strlen(client.name)==1 && client.name[0]=='q')
            return ;
        client.type = CHECK_NAME;
        client.TYPE = 1;

        if(msgsnd(registerID, &client, sizeof(CLIENT), 0) == -1)
        {
            perror("Register_Send_Mes");
            exit(errno);
        }
        
        while(1)
        {
            int length = msgrcv(msgid, &reply, sizeof(REPLY), 0, 0);
            if(length>0)
                break;
            if(length < 0)
            {
                cout << "\033[33mRead error.\033[0m" << endl;
                handler(0);
            }
        }
        
        if(!reply.flag)
            printf("\033[33m%s\n\033[0m", reply.message);
    }

    getchar();
    cout << "\033[33mInput your password: \033[0m";
    getPassword(client.password);
    cout << "\033[33mComfirm your password: \033[0m";
    getPassword(buffer);
    if(strcmp(client.password, buffer)!=0)
    {
        cout << "\033[33mPasswords are diffrent.\033[0m" << endl;
        return;
    }

    client.type=CHECK_IN;
    client.TYPE = 1;
    if(msgsnd(registerID, &client, sizeof(client), 0) == -1)
    {
        perror("Register_Send_Mes");
        exit(errno);
    }

    while(1)
    {
        int length = msgrcv(msgid, &reply, sizeof(REPLY), 0, 0);
        if(length>0)
            break;
        if(length < 0)
        {
            cout << "\033[33mRead error.\033[0m" << endl;
            handler(0);
        }
    }
    printf("\033[33m%s\n\033[0m", reply.message);
}

void Logout()
{
    CLIENT client;
    client.msgkey=msgkey;
    strcpy(client.name, name.c_str());
    client.type = 0;
    client.TYPE = 1;
    if(msgsnd(loginID, &client, sizeof(client), 0) == -1)
    {
        perror("Logout_Send_Mes");
        exit(errno);
    }
    name="";

    pthread_cancel(readThread);
    pthread_join(readThread, NULL);
    return ;
}

void Chat()
{
    int type;
    MESSAGE message;
    message.TYPE = 1;
    REPLY reply;
    pthread_create(&readThread, NULL, &readMess, NULL);

    const char *menu2="\n*************************\n* 1. Send Message;      *\n* 2. Multiple message;  *\n* 3. Logout;            *\n*************************\n";
    
    while(true)
    {
        printf("\33[1m\033[34m%s\033[0m", menu2);
        cout << "\033[33mChoose the service you need: \033[0m";
        cin >> type;
        cin.clear();

        if(type==1)
        {
            message.num=1;

            cout << "\033[33mInput the reciever: \033[0m";
            cin >> message.to[0];

            strcpy(message.from, name.c_str());
            cout << "\033[33mInput the text: \033[0m" << endl;
            string str;

            cin.clear();
            while(str.size()==0)
                getline(cin, str);
            
            strcpy(message.message, str.c_str());
            if(msgsnd(chatID, &message, sizeof(MESSAGE), 0) == -1)
            {
                perror("Chat_Send_Mes");
                exit(errno);
            }
            usleep(1000);
        }
        else if(type == 2)
        {
            cout << "\033[33mInput the number reciever(bigger than 0 and smaller than 5): \033[0m";
            cin >> message.num;

            if(message.num >= 1 && message.num<5)
            {
                for(int i=0; i<message.num; i++)
                {
                    cout << "\033[33mInput the No." << i+1 << " reciever: \033[0m";
                    cin >> message.to[i];
                }
            }
            else{
                cout << "\033[33mPlease input the right number.\033[0m" << endl;
                continue;
            }

            strcpy(message.from, name.c_str());
            cout << "\033[33mInput the text: \033[0m" << endl;
            string str;

            cin.clear();
            while(str.size()==0)
                getline(cin, str);
            
            strcpy(message.message, str.c_str());
            if(msgsnd(chatID, &message, sizeof(MESSAGE), 0) == -1)
            {
                perror("Chat_Send_Mes");
                exit(errno);
            }
            usleep(1000);
        }
        else if(type == 3)
        {
            Logout();
            return;
        }
        else{
            cout << "\033[33mPlease input the right number.\033[0m" << endl;
            cin.clear();
        }
    }
}

void Login()
{
    CLIENT client;
    REPLY reply;
    client.type = 1;
    client.msgkey = msgkey;
    client.TYPE = 1;

    cout << "\033[33mInput the name or input 'q' to quit:\033[0m" << endl;
    cin >> client.name;
    if(strlen(client.name) == 1 && client.name[0]=='q')
        return ;
    cout << "\033[33mInput your password: \033[0m";
    getchar();
    getPassword(client.password);

    if(msgsnd(loginID, &client, sizeof(client), 0) == -1)
    {
        perror("Login_Send_Mes");
        exit(errno);
    }

    while(1)
    {
        int length = msgrcv(msgid, &reply, sizeof(REPLY), 0, 0);
        if(length > 0)
            break;
        if(length < 0)
        {
            cout << "\033[33mRead error.\033[0m" << endl;
            handler(0);
        }
    }
    printf("\033[33m%s\n\033[0m", reply.message);
    
    if(reply.flag)
    {
        name = string(client.name);
        Chat();
    }
    return ;
}

void * readMess(void * arg)
{
    while(true)
    {
        OFFMES message;
        int length = msgrcv(msgid, &message, sizeof(message), 0, 0);

        if(length>0)
        {
            if(strlen(message.from)==0)
                printf("\033[31m%s\033[0m", message.message);
            else{
                cout << endl;
                printf("\n\033[34m%s\033[0m", message.from);
                cout << "\033[33m sent a message to you: \033[0m" << endl;
                printf(" >>  \033[32m%s\033[0m\n", message.message);
                cout.flush();
            }
        }
    }
}

void handler(int sig){
    if(name.size()!=0)
        Logout();
    msgctl (msgid, IPC_RMID, NULL);
    exit(1);
}

void initialKey()
{
    msgkey = CLIENTKEY;
    while((msgid = msgget(msgkey, 0666 | IPC_EXCL | IPC_CREAT)) < 0)
        msgkey++;
    if(msgid == -1)
    {
        perror("Init_Key");
        exit(errno);
    }
}

int main()
{
    //cout << "\033[33mInitialing and connecting to the server......\033[0m" << endl;
    umask(0);
    name="";
    signal(SIGKILL, handler);
    signal(SIGINT, handler);
    signal(SIGTERM, handler);

    initialKey();
    initKey((key_t)REGISTER, 0666, &registerID);
    initKey((key_t)LOGIN, 0666, &loginID);
    initKey((key_t)CHAT, 0666, &chatID);
    if((registerID < 0) || (loginID < 0) || (chatID < 0))
    {
        printf("\033[33mServer do not open.\n\033[0m");
        handler(0);
        exit(EXIT_FAILURE);
    }

    const char *menu="\n*************************\n* 1. Register;          *\n* 2. Login;             *\n* 3. Quit.              *\n*************************\n";
    while(true)
    {
        int type;
        printf("\33[1m\033[34m%s\033[0m", menu);
        cout << "\033[33mChoose the service you need: \033[0m";
        cin >> type;
        cin.clear();

        if(type == 1)
            Register();
        else if(type == 2)
            Login();
        else if(type == 3)
        {
            handler(0);
            return 0;
        }
        else cout << "\033[33mPlease input the right number.\033[0m" << endl;
    }
}