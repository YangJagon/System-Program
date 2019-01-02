#include "chat.h"
#include<vector>
#include<set>

int registerID, loginID, chatID;

char buffer[128];
int maxOnlineNum;

map<string, string> np_data;
map<string, int> online;
map<string, vector<OFFMES>> offLine;
pthread_mutex_t mutex_np;
pthread_mutex_t mutex_online;
pthread_mutex_t mutex_offline;

void handler(int sig){
    msgctl (registerID, IPC_RMID, NULL);
    msgctl (loginID, IPC_RMID, NULL);
    msgctl (chatID, IPC_RMID, NULL);
    exit(1);
}

int init_daemon() 
{ 
	int pid; 
	int i; 

    /*将文件当时创建屏蔽字设置为0*/
    umask(0);

	/*忽略终端I/O信号，STOP信号*/
	signal(SIGTTOU,SIG_IGN);
	signal(SIGTTIN,SIG_IGN);
	signal(SIGTSTP,SIG_IGN);
	signal(SIGHUP,SIG_IGN);
    signal(SIGKILL, handler);
    signal(SIGINT, handler);
    signal(SIGTERM, handler);
	
	pid = fork();
	if(pid > 0) { exit(0); /*结束父进程，使得子进程成为后台进程*/ }
	else if(pid < 0) {  return -1; }

	/*建立一个新的进程组,在这个新的进程组中,子进程成为这个进程组的首进程,以使该进程脱离所有终端*/
	setsid();

	/*再次新建一个子进程，退出父进程，保证该进程不是进程组长，同时让该进程无法再打开一个新的终端*/
	pid=fork();
	if( pid > 0) { exit(0); }
	else if( pid< 0) { return -1; }

	/*关闭所有从父进程继承的不再需要的文件描述符*/
	for(i=0;i< 3;close(i++));

	/*改变工作目录，使得进程不与任何文件系统联系*/
	chdir("/home/jiageng2016150026/ChatQ/server/");

	/*忽略SIGCHLD信号*/
	signal(SIGCHLD,SIG_IGN); 
	
	return 0;
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

void send2all(string s)
{
    OFFMES messages;
    messages.TYPE = 1;
    messages.from[0] = '\0';
    strcpy(messages.message, s.c_str());

    for(pair<string, int> user : online)
    {
        if(msgsnd(user.second, &messages, sizeof(messages), 0) == -1)
        {
            perror("Send_Mes");
            exit(errno);
        }
    }
}

void* registerThread(void *arg)
{
    CLIENT info;
    REPLY reply;
    reply.TYPE = 1;
    while(true)
    {
        int res = msgrcv(registerID, &info, sizeof(CLIENT), 0, 0);

        if(res == 0)
            continue;
        else if(res < 0)
        {
            perror("Register_Recieve_Mes");
            exit(errno);
        }
    
        if(info.type == CHECK_NAME)
        {
            string name = string(info.name);

            if(np_data.find(name) == np_data.end())
                reply.flag = 1;
            else{
                reply.flag=0;
                sprintf(reply.message, "This name has been used.");
            }
        }
        else if(info.type == CHECK_IN)
        {
            string name=string(info.name);
            string password=string(info.password);
            
            pthread_mutex_lock(&mutex_np);    //修改rp_data哈希表，对其上锁，防止其它线程进行访问
            np_data[name]=password;
            pthread_mutex_unlock(&mutex_np);

            sprintf(reply.message, "Create account sucess.");
            cout << info.name << " has registered." << endl;
        }

        int tid = msgget(info.msgkey, 0666);
        reply.TYPE = 1;
        if(tid < 0)
        {
            perror("Open_Client_Msgkey");
            continue;
        }
        if(msgsnd(tid, &reply, sizeof(REPLY), 0)==-1)
        {
            perror("Send_Mes_Client");
            exit(errno);
        }
    }
}

void* loginThread(void *arg)
{
    while(true)
    {
        CLIENT info;
        REPLY reply;
        reply.TYPE = 1;

        int res = msgrcv(loginID, &info, sizeof(CLIENT), 0, 0);
        if(res == 0)
            continue;
        else if(res < 0)
        {
            perror("Login_Recieve_Mes");
            exit(errno);
        }

        string name=string(info.name);
        if(!info.type)  //如果信息类型是退出请求
        {
            pthread_mutex_lock(&mutex_online);  //修改online集合，对其上锁，防止其它线程进行访问
            online.erase(name);     //将online集合里面的名字去掉
            pthread_mutex_unlock(&mutex_online);
            send2all("\n" + name + " has logged out. Online number is " + to_string(online.size()) + " now." );
            /*发送信息给所有剩下在线的人，提醒该用户已退出*/
            cout << name << " has log out." << endl;
            continue;
        }

        string password=string(info.password);
        int cid = msgget(info.msgkey, 0666);
        if(cid == -1)
        {
            perror("Login_Init_Key");
            continue;
        }

        pthread_mutex_lock(&mutex_np);  //对np_data哈希表上锁，防止register线程修改
        if(online.size() >= maxOnlineNum)
        {
            sprintf(reply.message, "There are too many persons online.");
            reply.flag=0;
        }
        else if(np_data.find(name) == np_data.end())
        {
            sprintf(reply.message, "User not exists!");
            reply.flag=0;
        }
        else if(online.find(name) != online.end())
        {
            sprintf(reply.message, "This account has log in.");
            reply.flag=0;
        }
        else if(password != np_data[name])
        {
            sprintf(reply.message, "Password is wrong");
            reply.flag=0;
        }
        else{
            sprintf(reply.message, "Login sucess.");
            send2all("\n" + name + " has logged in. Online number is " + to_string(online.size()+1) + " now.");

            pthread_mutex_lock(&mutex_online);   //修改online集合，对其上锁，防止其它线程进行访问
            online[name] = cid;
            pthread_mutex_unlock(&mutex_online);

            reply.flag=1;
            cout << name << " has logged in." << endl;
        }
        pthread_mutex_unlock(&mutex_np); //解锁

        if(msgsnd(cid, &reply, sizeof(reply), 0)==-1)
        {
            perror("Login_Send_Mes");
            continue;
        }
  
        pthread_mutex_lock(&mutex_offline);   //查看、修改offline哈希表，对其上锁，防止其它线程进行访问
        if(offLine.find(name) != offLine.end())
        {
            string reciever = name;
            usleep(1000);

            for(OFFMES message : offLine[name])
            {
                if(msgsnd(cid, &message, sizeof(OFFMES), 0)==-1)
                {
                    perror("Login_Send_Mes");
                    break;
                }
            }
            offLine.erase(name);
        }
        pthread_mutex_unlock(&mutex_offline);
    }
}

void multiple(MESSAGE message)
{
    OFFMES reply;
    reply.from[0] = '\0';
    reply.TYPE = 1;
    int n=message.num;
    bool flag;
    string rmes = "";

    string sender = string(message.from);
    int sid = online[sender];
    string reciever[4];
    int rid[4];
    for(int i=0; i<n; i++)
    {
        reciever[i] = string(message.to[i]);
        if(online.find(reciever[i])!=online.end())
            rid[i] = online[reciever[i]];
        else rid[i] = -1;
    }

    for(int i=0; i<n; i++)
    {
        pthread_mutex_lock(&mutex_np);    //对np_data哈希表上锁，防止login线程修改  
        if(np_data.find(reciever[i]) == np_data.end())
        {
            flag=0;
            rmes += reciever[i] + " not exists.\n";
        }
        else{
            flag=1;
            rmes += "Send message to " + reciever[i] + " sucessfully.\n";
        }
        pthread_mutex_unlock(&mutex_np);

        if(flag)
        {
            OFFMES mes;
            mes.TYPE = 1;
            strcpy(mes.from, message.from);
            strcpy(mes.message, message.message);

            pthread_mutex_lock(&mutex_online);  //对online集合上锁，防止login线程修改
            if(rid[i] > 0)
            {
                if(msgsnd(rid[i], &mes, sizeof(mes), 0)==-1)
                {
                    perror("Login_Send_Mes");
                    break;
                }
            }
            else{
                pthread_mutex_lock(&mutex_offline);   //查看、修改offline哈希表，对其上锁，防止其它线程进行访问
                if(offLine.find(reciever[i]) == offLine.end())
                    offLine[reciever[i]] = vector<OFFMES>();
                offLine[reciever[i]].push_back(mes);
                pthread_mutex_unlock(&mutex_offline);
            }
            pthread_mutex_unlock(&mutex_online);
        }
    }

    strcpy(reply.message, rmes.c_str());
    if(msgsnd(sid, &reply, sizeof(reply), 0)==-1)
    {
        perror("Login_Send_Mes");
        exit(errno);
    }

    cout << message.from << " has sent a message." << endl;
}


void* chatThread(void *arg)
{
    while(true)
    {
        MESSAGE message;
        int res = msgrcv(chatID, &message, sizeof(MESSAGE), 0, 0);
        if(res == 0)
            continue;
        else if(res < 0)
        {
            perror("Login_Recieve_Mes");
            exit(errno);
        }
        multiple(message);
    }
}

void initialFile()
{
    string str;
	regex e("(\\w+)\\W*=\\W*([\\w.]+)");
	smatch m;
	ifstream in(CONFIGFILE);
	map<string, string> tmp;

	while (getline(in, str))
	{
		if (std::regex_search(str, m, e)) {
			string key = m[1];
			transform(key.begin(), key.end(), key.begin(), ::tolower);
			tmp[key] = m[2];
		}
	}

    if(tmp.find("max_online_num")==tmp.end())
        maxOnlineNum = 4;
    else
        maxOnlineNum = stoi(tmp["max_online_num"]);

    cout << "Max online num: " << maxOnlineNum << endl;   

    in.close();
}


int main()
{
    init_daemon();
    initialFile();
    pthread_mutex_init(&mutex_np, NULL);
    pthread_mutex_init(&mutex_online, NULL);
    pthread_mutex_init(&mutex_offline, NULL);

    initKey((key_t)REGISTER, 0666 | IPC_EXCL | IPC_CREAT, &registerID);
    initKey((key_t)LOGIN, 0666 | IPC_EXCL | IPC_CREAT, &loginID);
    initKey((key_t)CHAT, 0666 | IPC_EXCL | IPC_CREAT, &chatID);

    pthread_t rid, lid;
    pthread_create(&rid, NULL, &registerThread, NULL);
    pthread_create(&lid, NULL, &loginThread, NULL);
    chatThread(NULL);

    return 0;
}
