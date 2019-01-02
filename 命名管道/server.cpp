#include "chat.h"
#include<vector>
#include<set>

char buffer[128];
char FIFO_REGISTER[86];
char FIFO_LOGIN[86];
char FIFO_CHAT[86];
int maxOnlineNum;

map<string, string> np_data;
set<string> online;
map<string, vector<OFFMES>> offLine;
string prefix = string(CLIENT_FIFO);
pthread_mutex_t mutex_np;
pthread_mutex_t mutex_online;
pthread_mutex_t mutex_offline;

void handler(int sig){
    unlink(FIFO_REGISTER);
    unlink(FIFO_CHAT);
    unlink(FIFO_LOGIN);
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
	chdir("/home/jiageng2016150026/Chat/server/");

	/*忽略SIGCHLD信号*/
	signal(SIGCHLD,SIG_IGN); 
	
	return 0;
}

void createFIFO(const char *FIFO_NAME, int fid = -1)
{   //初始化服务器管道
    int res, fifo_fd;
    if(access(FIFO_NAME, F_OK) == -1)
    {
        res = mkfifo(FIFO_NAME, 0666);
        if(res != 0){
            perror("Create_FIFO");
            exit(errno);
        }
    }

    if(fid > 0)
    {
        //fifo_fd = open(FIFO_NAME, O_RDWR | O_NONBLOCK);
        fifo_fd = open(FIFO_NAME, O_RDWR);
        if(fifo_fd == -1){
            perror("Open_FIFO");
            exit(errno);
        }
        dup2(fifo_fd, fid);
    }
}

void send2all(string s)
{
    OFFMES messages;
    messages.from[0] = '\0';
    strcpy(messages.message, s.c_str());

    for(string name : online)
    {
        string rfifo = prefix + name +".pipe";
        createFIFO(rfifo.c_str());
        int fd = open(rfifo.c_str(), O_RDWR);

        if(fd < 0)
        {
            perror("Open_Client_FIFO");
            return;
        }
        else{
            write(fd, &messages, sizeof(messages));
            close(fd);
        }
    }
}

void* registerThread(void *arg)
{
    CLIENT info;
    REPLY reply;
    while(true)
    {
        int res = read(REGISTER, &info, sizeof(CLIENT));

        if(res <= 0)
            continue;
    
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

        int fd1 = open(info.myfifo, O_WRONLY);
        if(fd1 < 0)
        {
            perror("Open_Client_tmpFIFO");
            continue;
        }
        int i = write(fd1, &reply, sizeof(REPLY));
        close(fd1);
    }
}

void* loginThread(void *arg)
{
    while(true)
    {
        CLIENT info;
        REPLY reply;
        int res = read(LOGIN, &info, sizeof(CLIENT));
        if(res == 0)
            continue;

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
            online.insert(name);
            pthread_mutex_unlock(&mutex_online);

            reply.flag=1;
            cout << name << " has logged in." << endl;
        }
        pthread_mutex_unlock(&mutex_np); //解锁

        int fd1 = open(info.myfifo, O_WRONLY);
        write(fd1, &reply, sizeof(REPLY));
        close(fd1);
  
        pthread_mutex_lock(&mutex_offline);   //查看、修改offline哈希表，对其上锁，防止其它线程进行访问
        if(offLine.find(name) != offLine.end())
        {
            string reciever = name;
            string reciever_fifo = prefix+reciever+".pipe";
            memcpy(buffer, reciever_fifo.c_str(), reciever_fifo.size()+1);
            usleep(1000);

            int fd = open(buffer, O_RDWR);
            if(fd < 0)
            {
                perror("Open_Client_FIFO");
                return NULL;
            }

            for(OFFMES message : offLine[name])
            {
                write(fd, &message, sizeof(OFFMES));
                //kill(online[reciever], SIGUSR1);
            }
            close(fd);
            offLine.erase(name);
        }
        pthread_mutex_unlock(&mutex_offline);
    }
}

void multiple(MESSAGE message)
{
    OFFMES reply;
    int n=message.num;
    bool flag;
    string rmes = "";

    string sender = string(message.from);
    string sender_fifo = prefix+sender+".pipe";
    string reciever[4];
    string reciever_fifo[4];
    for(int i=0; i<n; i++)
    {
        reciever[i] = string(message.to[i]);
        reciever_fifo[i] = prefix + reciever[i] +".pipe";
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
            strcpy(mes.from, message.from);
            strcpy(mes.message, message.message);

            pthread_mutex_lock(&mutex_online);  //对online集合上锁，防止login线程修改
            if(online.find(reciever[i]) != online.end())
            {
                createFIFO(reciever_fifo[i].c_str());
                int fd1 = open(reciever_fifo[i].c_str(), O_RDWR);
                if(fd1 < 0)
                {
                    perror("Open_Client_FIFO");
                    flag = 0;
                }
                else{
                    write(fd1, &mes, sizeof(mes));
                    close(fd1);
                    //kill(online[reciever[i]], SIGUSR1);
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

    createFIFO(sender_fifo.c_str());
    int fd2 = open(sender_fifo.c_str(), O_WRONLY);
    if(fd2 < 0)
    {
        perror("Open_Client_FIFO");
        return;
    }
    else{
        reply.from[0] = '\0';
        strcpy(reply.message, rmes.c_str());
        write(fd2, &reply, sizeof(reply));
        close(fd2);
    }

    cout << message.from << " has sent a message." << endl;
}


void* chatThread(void *arg)
{
    while(true)
    {
        MESSAGE message;
        int res = read(CHAT, &message, sizeof(MESSAGE));
        if(res == 0)
            continue;

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

    if(tmp.find("max_online_num")==tmp.end())
        maxOnlineNum = 4;
    else
        maxOnlineNum = stoi(tmp["max_online_num"]);

    cout << FIFO_REGISTER << endl;
    cout << FIFO_LOGIN << endl;
    cout << FIFO_CHAT << endl;
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

    createFIFO(FIFO_REGISTER, REGISTER);
    createFIFO(FIFO_LOGIN, LOGIN);
    createFIFO(FIFO_CHAT, CHAT);

    pthread_t rid, lid;
    pthread_create(&rid, NULL, &registerThread, NULL);
    pthread_create(&lid, NULL, &loginThread, NULL);
    chatThread(NULL);

    return 0;
}
