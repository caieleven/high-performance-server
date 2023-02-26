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
#include <pthread.h>

#define MAX_EVENT_NUMBER 1024
#define BUFFER_SIZE 10

/**
 * @brief 将文件描述符设置为非阻塞
*/
int setnonblocking(int fd) {
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}

/**
 * @brief 将文件描述符fd上的EPOLLIN注册到epollfd指示的epoll内核事件表中
 * @param enable_et 是否启用ET模式
*/
void addfd(int epollfd, int fd, bool enable_et) {
    epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN;
    if(enable_et) {
        event.events |= EPOLLET;
    }
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
}

/**
 * @brief LT模式
*/
void lt(epoll_event* events, int number, int epollfd, int listenfd) {
    char buf[BUFFER_SIZE];
    for(int i = 0; i < number; i++) {
        int sockfd = events[i].data.fd;
        if(sockfd == listenfd) {
            struct sockaddr_in client_address;
            socklen_t client_addrlength = sizeof(client_address);
            int connfd = accept(listenfd, (struct sockaddr*)&client_address, &client_addrlength);
            addfd(epollfd, connfd, false);  //使用LT模式
        }
        else if(events[i].events & EPOLLIN) {
            // 这段输出语句可能重复执行，与EL区分
            printf("event trigger once\n");
            memset(buf, '\0', BUFFER_SIZE);
            int ret = recv(sockfd, buf, BUFFER_SIZE-1, 0);
            if(ret <= 0) {
                close(sockfd);
                continue;
            }
            printf("get %d bytes of content: %s\n", ret, buf);
        }
        else{
            printf("something else happened\n");
        }
    }
}

/**
 * @brief ET模式
*/
void et(epoll_event* events, int number, int epollfd, int listenfd){
    char buf[BUFFER_SIZE];
    for(int i = 0; i < number; i++) {
        int sockfd = events[i].data.fd;
        if(sockfd == listenfd) {
            struct sockaddr_in client_address;
            socklen_t client_addrlength = sizeof(client_address);
            int connfd = accept(listenfd, (struct sockaddr*)&client_address, &client_addrlength);
            addfd(epollfd, connfd, true);
        }
        else if(events[i].events & EPOLLIN) {
            // 不会重复执行，所以需要循环读取数据，确保把所有socket读缓存中的数据读取
            printf("event trigger once\n");
            while(1) {
                memset(buf, '\0', BUFFER_SIZE);
                int ret = recv(sockfd, buf, BUFFER_SIZE-1, 0);
                if(ret < 0) {
                    //非阻塞IO，此时数据全部读写完毕，此后，epoll就能再次触发sockfd上的EPOLL事件
                    if((errno == EAGAIN) || (errno == EWOULDBLOCK)) {
                        printf("read later\n");
                        break;
                    } 
                    close(sockfd);
                    break;
                }
                else if(ret == 0) {
                    close(sockfd);
                }
                else{
                    printf("get %d bytes of content: %s\n", ret, buf);
                }
            }
        }
        else{
            printf("something else happened\n");
        }
    }
}


int main(int argc, char* argv[]){
    if(argc <= 2) {
        printf("usage: %s ip_address port_number\n", basename(argv[0]));
        return 1;
    }
    const char* ip =  argv[1];
    int port = atoi(argv[2]);

    int ret = 0;
    struct sockaddr_in address;
    bzero(&address, sizeof(address));
    address.sin_family = AF_INET;
    inet_pton(AF_INET, ip, &address.sin_addr);
    address.sin_port = htons(port);

    int listenfd = socket(PF_INET, SOCK_STREAM, 0);
    assert(listenfd >= 0);

    ret = bind(listenfd, (struct sockaddr*)& address, sizeof(address));
    assert(ret != -1);

    epoll_event events[MAX_EVENT_NUMBER];
    int epollfd = epoll_create(5);
    assert(epollfd != -1);
    addfd(epollfd, listenfd, true);

    while(1) {
        int ret = epoll_wait(epollfd, events, MAX_EVENT_NUMBER, -1);
        if(ret < 0) {
            printf("epoll failure\n");
            break;
        }
        lt(events, ret, epollfd, listenfd); //LT模式
        // et(events, ret, epollfd, listenfd);  //ET模式
    }
    close(listenfd);
    return 0;
}