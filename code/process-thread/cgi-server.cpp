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

#include "processpool.h"

// 用于客户CGI请求的类，它可以作为processpool类的模板类
class cgi_conn{
public:
    cgi_conn(){}
    ~cgi_conn(){}

    // 初始化客户连接，清空读缓冲区
    void init(int epollfd, int sockfd, const sockaddr_in& client_addr){
        m_epollfd = epollfd;
        m_sockfd = sockfd;
        m_address = client_addr;
        memset(m_buf, '\0', BUFFER_SIZE);
        m_read_idx = 0;
    }

    void process(){
        int idx = 0;
        int ret = -1;
        // 循环读取和分析客户数据
        while (true){
            idx = m_read_idx;
            ret = recv(m_sockfd, m_buf + idx, BUFFER_SIZE-1-idx, 0);
            // 读错误，关闭客户连接，若是暂无数据可读，退出循环
            if(ret < 0){
                if(errno != EAGAIN){
                    removefd(m_epollfd, m_sockfd);
                }
                break;
            }
            // 对方关闭连接，则服务器也关闭来连接
            else if(ret == 0){
                removefd(m_epollfd, m_sockfd);
                break;
            }
            else{
                m_read_idx += ret;
                printf("user content is %s\n", m_buf);
                // 若遇到"\r\n"，则开始处理客户请求
                for(; idx < m_read_idx; ++idx){
                    if(idx >= 1 && m_buf[idx-1] == '\r' && m_buf[idx] == '\n'){
                        break;
                    }
                }
            }
        }
        
    }


private:
    static const int BUFFER_SIZE = 1024;
    static int m_epollfd;
    int m_sockfd;
    sockaddr_in m_address;
    char m_buf[BUFFER_SIZE];
    // 标记读缓冲中已经读入的客户数据的最后一个字节的下一个位置
    int m_read_idx;
};

