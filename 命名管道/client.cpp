#include "chat.h"
#include<stdio.h>

char tmpFIFO[64];
char buffer[64];
char FIFO_REGISTER[86];
char FIFO_LOGIN[86];
char FIFO_CHAT[86];

string name, myfifo;
pthread_t readThread;
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

void initialFIFO(const char *FIFO_NAME, int authority, int fid)
{
    int res, fifo_fd;
    if(access(FIFO_NAME, F_OK) == -1)
    {
        if(fid == REGISTER || fid == LOGIN || fid == CHAT){
            printf("\033[33mServer do not open.\n\033[0m");
            exit(EXIT_FAILURE);
        }
        else{
            res = mkfifo(FIFO_NAME, 0666);
            if(res != 0){
                cout << FIFO_NAME << endl;
                cout << "\033[33mCan not create temperate fifo.\033[0m" << endl;
                exit(EXIT_FAILURE);
            }
        }
    }

    fifo_fd = open(FIFO_NAME, authority);
    if(fifo_fd == -1){
        printf("\033[33mCould not open %s.\n\033[0m", FIFO_NAME);
        exit(EXIT_FAILURE);
    }
    dup2(fifo_fd, fid);
}

void Register()
{
    int length;
    CLIENT client;
    REPLY reply;
    reply.flag=0;
    strcpy(client.myfifo, tmpFIFO);
    cout << "\033[33mInput the name or input 'q' to quit:\033[0m" << endl;

    while(!reply.flag)
    {
        cin >> client.name;
        if(strlen(client.name)==1 && client.name[0]=='q')
            return ;
        client.type = CHECK_NAME;
        length = write(REGISTER, &client, sizeof(CLIENT));
        
        while(1)
        {
            int length = read(TMP, &reply, sizeof(REPLY));
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
    length = write(REGISTER, &client, sizeof(CLIENT));

    while(1)
    {
        int length = read(TMP, &reply, sizeof(REPLY));
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
    client.pid=getpid();
    strcpy(client.name, name.c_str());
    client.type = 0;
    write(LOGIN, &client, sizeof(CLIENT));
    name=myfifo="";

    pthread_cancel(readThread);
    pthread_join(readThread, NULL);
    return ;
}

void Chat()
{
    int type;
    MESSAGE message;
    REPLY reply;
    initialFIFO(myfifo.c_str(), O_RDWR, MYFIFO);
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
            write(CHAT, &message, sizeof(MESSAGE));
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
            write(CHAT, &message, sizeof(MESSAGE));
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
    strcpy(client.myfifo, tmpFIFO);
    client.pid=getpid();

    cout << "\033[33mInput the name or input 'q' to quit:\033[0m" << endl;
    cin >> client.name;
    if(strlen(client.name) == 1 && client.name[0]=='q')
        return ;
    cout << "\033[33mInput your password: \033[0m";
    getchar();
    getPassword(client.password);

    write(LOGIN, &client, sizeof(CLIENT));

    while(1)
    {
        int length = read(TMP, &reply, sizeof(REPLY));
        if(length>0)
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
        myfifo = string(CLIENT_FIFO);
        name = string(client.name);
        myfifo = myfifo+name+".pipe";
        Chat();
    }
    return ;
}

void * readMess(void * arg)
{
    while(true)
    {
        OFFMES message;
        int length = read(MYFIFO, &message, sizeof(message));

        if(length>0)
        {
            if(strlen(message.from)==0)
                printf("\033[31m%s\033[0m\n", message.message);
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
    if(myfifo.size()!=0)
        Logout();

    unlink(tmpFIFO);
    if(myfifo.size()!=0)
        unlink(myfifo.c_str());
    exit(1);
}

void initialFile()
{
    string str;
	regex e("(\\w+)\\W*=\\W*([\\w.]+)");
	smatch m;
	ifstream in(CONFIGFILE);
	map<string, string> tmp;
    string prefix = SERVER_FIFO;

	while (getline(in, str))
	{
		if (std::regex_search(str, m, e)) {
			string key = m[1];
			transform(key.begin(), key.end(), key.begin(), ::tolower);
			tmp[key] = m[2];
		}
	}

    if(tmp.find("fifo_register")==tmp.end())
        sprintf(FIFO_REGISTER, (prefix+"register.pipe").c_str());
    else
        sprintf(FIFO_REGISTER, (prefix+tmp["fifo_register"]).c_str());

    if(tmp.find("fifo_login")==tmp.end())
        sprintf(FIFO_LOGIN, (prefix+"login.pipe").c_str());
    else
        sprintf(FIFO_LOGIN, (prefix+tmp["fifo_login"]).c_str());

    if(tmp.find("fifo_chat")==tmp.end())
        sprintf(FIFO_CHAT, (prefix+"chat.pipe").c_str());
    else
        sprintf(FIFO_CHAT, (prefix+tmp["fifo_chat"]).c_str());

    in.close();
}

int main()
{
    //cout << "\033[33mInitialing and connecting to the server......\033[0m" << endl;
    umask(0);
    initialFile();
    myfifo=name="";
    signal(SIGKILL, handler);
    signal(SIGINT, handler);
    signal(SIGTERM, handler);

    initialFIFO(FIFO_REGISTER, O_WRONLY, REGISTER);
    initialFIFO(FIFO_LOGIN, O_WRONLY, LOGIN);
    initialFIFO(FIFO_CHAT, O_WRONLY, CHAT);
    sprintf(tmpFIFO, "/home/jiageng2016150026/Chat/client/tmp/%d.pipe", getpid());
    initialFIFO(tmpFIFO, O_RDWR, TMP);
    //signal(SIGUSR1, readMess);

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