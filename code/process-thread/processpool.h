/**
 * @brief 半同步/半异步并发模式的进程池。为了避免父子进程之间传递文件描述符，将新连接放到子进程中。因此，一个客户连接上的所有任务始终由一个子进程处理
*/
#ifndef PROCESSPOOL_H
#define PROCESSPOOL_H

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/stat.h>

// m_pid是目标子进程的PID，m_pipefd是父子进程通信用的管道
class process{
public:
    process(): m_pid(-1){}

    pid_t m_pid;
    int m_pipefd[2];
};

/**
 * @tparam T 模板参数为处理逻辑任务的类
*/
template <typename T>
class processpool{
private:
    // 将构造函数定义为私有，因此只能通过后面的create静态函数来创建processpool实例
    processpool(int listenfd, int process_number = 8);

public:
    //单体模式，保证程序最多创建一个processpool实例，这是程序正确处理信号的必要条件
    static processpool<T>* create (int listenfd, int process_number = 8){
        if(!m_instance){
            m_instance = new processpool<T>(listenfd, process_number);
        }
        return m_instance;
    }

    ~processpool(){
        delete [] m_sub_process;
    }

    //启动进程池
    void run();

private:
    void setup_sig_pipe();
    void run_parent();
    void run_child();


private:
    static const int MAX_PROCESS_NUMBER = 16;       //进程池允许的最大子进程数目
    static const int USER_PER_PROCESS = 65536;      //每个子进程最多能处理的客户数量
    static const int MAX_EVENT_NUMBER = 10000;      //epoll最多能处理的事件数
    int m_process_number;                           //进程池中的进程总数
    int m_idx;                                      //进程在池中的序号，0开始
    int m_epollfd;                                  //epoll内核事件表
    int m_listenfd;                                 //监听socket
    int m_stop;                                     //进程通过m_stop决定是否停止运行
    process* m_sub_process;                         //保存所有子进程的描述信息
    static processpool<T>* m_instance;              //进程池静态实例
    
};


template <typename T>
processpool<T>* processpool<T>::m_instance = NULL;

// 用于处理信号的管道，以实现统一事件源，后面称之为信号管道
static int sig_pipefd[2];

static int setnonblocking(int fd){
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}

static void addfd(int epollfd, int fd){
    epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN | EPOLLET;
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    setnonblocking(fd);
}

/**
 * @brief 删除注册事件
 * @param epollfd 内核时间表
 * @param fd 待删除的文件描述符
*/
static void removefd(int epollfd, int fd){
    epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, 0);
    close(fd);
}

static void sig_handler(int sig){
    int save_errno = errno;
    int msg = sig;
    send(sig_pipefd[1], (char*)&msg, 1, 0);
    errno = save_errno;
}

static void addsig(int sig, void (handler)(int), bool restart = true){
    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa));
    sa.sa_handler = handler;
    if(restart){
        sa.sa_flags |= SA_RESTART;
    }
    sigfillset(&sa.sa_mask);
    assert(sigaction(sig, &sa, NULL) != -1);
}

template<typename T>
processpool<T>::processpool(int listenfd, int process_number):m_listenfd(listenfd), m_process_number(process_number), m_idx(-1), m_stop(false){
    assert(process_number >0 && process_number <= MAX_PROCESS_NUMBER);

    m_sub_process = new process[process_number];
    assert(m_sub_process);

    // 创建process_number个子进程，并建立他们和父进程之间的管道
    for(int i = 0; i < process_number; i++){
        int ret = socketpair(PF_UNIX, SOCK_STREAM, 0, m_sub_process[i].m_pipefd);
        assert(ret == 0);
        m_sub_process[i].m_pid = fork();
        // 这里尤其注意，不然很可能子进程也会执行fork
        // 主进程
        if(m_sub_process[i].m_pid > 0){
            close(m_sub_process[i].m_pipefd[1]);
            continue;
        }
        // 子进程
        else{
            close(m_sub_process[i].m_pipefd[0]);
            m_idx = i;
            // 注意这是break
            break;
        }
    }
}


// 统一事件源
template< typename T>
void processpool<T>::setup_sig_pipe(){
    m_epollfd = epoll_create(5);
    assert(m_epollfd != -1);
    int ret = socketpair(PF_UNIX, SOCK_STREAM, 0, sig_pipefd);
    assert(ret != -1);

    setnonblocking(sig_pipefd[1]);
    addfd(m_epollfd, sig_pipefd[0]);

    // 设置信号处理函数
    // 发送到sig_pipefd[1]
    addsig(SIGCHLD, sig_handler);
    addsig(SIGTERM, sig_handler);
    addsig(SIGINT, sig_handler);
    addsig(SIGPIPE, SIG_IGN);
}

// 父进程中m_idx值为-1， 子进程中m_idx值大于等于0
template <typename T>
void processpool<T>::run(){
    if(m_idx != -1){
        run_child();
        return;
    }
    run_parent();
}

template <typename T>
void processpool<T>::run_child(){
    setup_sig_pipe();

    // 每个子进程都通过其在进程池中的序号值m_idx找到与父进程通信的管道
    int pipefd = m_sub_process[m_idx].m_pipefd[1];

    // 子进程需要监听文件描述符pipefd，父进程将通过它来通知子进程accept新连接
    addfd(m_epollfd, pipefd);

    epoll_event events[MAX_EVENT_NUMBER];
    T* users = new T[USER_PER_PROCESS];
    assert(users);
    int number = 0;
    int ret = -1;

    while(!m_stop){
        number = epoll_wait(m_epollfd, events, MAX_EVENT_NUMBER, -1);
        if(number < 0 && errno != EINTR){
            printf("epoll failure\n");
            break;
        }

        for(int i = 0; i < number; i++){
            int sockfd = events[i].data.fd;
            if(sockfd == pipefd && events[i].events & EPOLLIN){
                int client = 0;
                // 从父子进程之间的管道中读取数据，并将结果保存在client中
                ret = recv(sockfd, (char*)&client, sizeof(client), 0);
                if((ret < 0 && errno != EAGAIN) || ret == 0){
                    continue;
                }
                else{
                    struct sockaddr_in client_address;
                    socklen_t client_addrlength = sizeof(client_address);
                    int connfd = accept(m_listenfd, (struct sockaddr*)&client_address, &client_addrlength);
                    if(connfd < 0){
                        printf("errno is:%d\n", errno);
                        continue;
                    }
                    addfd(m_epollfd, connfd);
                    
                    // 模板类T必须实现init方法，以初始化一个客户连接，我们直接使用connfd来索引逻辑处理对象
                    users[connfd].init(m_epollfd, connfd, client_address);
                }
            }
            // 处理子进程接受到的信号
            else if(sockfd == sig_pipefd[0] && (events[i].events & EPOLLIN)){
                int sig;
                char signals[1024];
                ret = recv(sig_pipefd[0], signals, sizeof(signals), 0);
                if(ret <= 0){
                    continue;
                }
                else{
                    for(int i = 0; i < ret; i++){
                        switch(signals[i]){
                            case SIGCHLD:{
                                pid_t pid;
                                int stat;
                                while((pid = waitpid(-1, &stat, WNOHANG)) > 0){
                                    continue;
                                }
                                break;
                            }
                            case SIGTERM:
                            case SIGINT:{
                                m_stop = true;
                                break;
                            }
                            default:{
                                break;
                            }
                        }
                    }
                }
            }
            else if(events[i].events & EPOLLIN){
                users[sockfd].process();
            }
            else{
                continue;
            }
        }
    }
    delete []users;
    users = NULL;
    close(pipefd);
    close(m_epollfd);
}


template <typename T>
void processpool<T>::run_parent(){
    setup_sig_pipe();

    // 父进程监听
    addfd(m_epollfd, m_listenfd);

    epoll_event events[MAX_EVENT_NUMBER];
    int sub_process_counter = 0;
    int new_conn = 1;
    int number = 0;
    int ret = -1;

    while (!m_stop)
    {
        number = epoll_wait(m_epollfd, events, MAX_EVENT_NUMBER, -1);
        if(number < 0 && errno != EINTR){
            printf("epoll failure\n");
            break;
        }
        
        for(int i = 0; i < number; i++){
            int sockfd = events[i].data.fd;
            if(sockfd == m_listenfd){
                // 如果有新连接，就采用Round Robin方式将其分配给一个子进程处理
                int i = sub_process_counter;
                do{
                    if(m_sub_process[i].m_pid != -1){
                        break;
                    }
                    i = (i+1)%m_process_number;
                }
                while(i != sub_process_counter);

                if(m_sub_process[i].m_pid == -1){
                    m_stop = true;
                    break;
                }
                sub_process_counter = (i+1)%m_process_number;
                send(m_sub_process[i].m_pipefd[0], (char*)&new_conn, sizeof(new_conn), 0);
                printf("send request to child %d\n", i);
            }
            // 父进程接受到的信号
            else if(sockfd == sig_pipefd[0] && (events[i].events & EPOLLIN)){
                int sig;
                char signals[1024];
                ret = recv(sig_pipefd[0], signals, sizeof(signals), 0);
                if(ret <= 0){
                    continue;
                }
                else{
                    for(int i = 0; i < ret; ++i){
                        switch(signals[i]){
                            case SIGCHLD:{
                                pid_t pid;
                                int stat;
                                while((pid = waitpid(-1, &stat, WNOHANG)) > 0){
                                    for(int i = 0; i < m_process_number; i++){
                                        // 如果进程池中第i个子进程退出了，则主进程关闭响应的通信信道
                                        if(m_sub_process[i].m_pid == pid){
                                            printf("child %d join\n", i);
                                            close(m_sub_process[i].m_pipefd[0]);
                                            m_sub_process[i].m_pid = -1;
                                        }
                                    }
                                }
                                // 如果所有子进程都已退出，则父进程退出
                                m_stop = true;
                                for(int i = 0; i < m_process_number; i++){
                                    if(m_sub_process[i].m_pid != -1){
                                        m_stop = false;
                                    }
                                }
                                break;
                            }
                            case SIGTERM:
                            case SIGINT:{
                                // 父进程收到终止信号，那么杀死所有子进程，并等待他们全部结束
                                printf("kill all the child now\n");
                                for(int i = 0; i < m_process_number; i++){
                                    int pid = m_sub_process[i].m_pid;
                                    if(pid != -1){
                                        kill(pid, SIGTERM);
                                    }
                                }
                                break;
                            }
                            default:{
                                break;
                            }
                        }
                    }
                }
            }
            else{
                continue;
            }
        }
    }
    close(m_epollfd);

}





#endif