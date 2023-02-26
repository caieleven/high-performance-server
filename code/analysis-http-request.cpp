/*
 * 有限状态机的使用
 * HTTP协议的读取与分析
*/

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#define BUFFER_SIZE 4096  // 读缓冲区大小

// 主状态机
// 该状态机分别表示正在分析请求行或头部字段
enum CHECK_STATUS {
    CHECK_STATUS_REQUESTING = 0,
    CHECK_STATUS_HEADER
};


// 从状态机
// 行的读取状态，分别为读到完整行，行出错，行数据不完整
enum LINE_STATUS {
    LINE_OK = 0, 
    LINE_BAD,
    LINE_OPEN
};

// 服务其处理http请求的结果
enum HTTP_CODE {
    NO_REQUEST,             //请求不完整
    GET_REQUEST,            //获得了一个完整的客户请求
    BAD_REQUEST,            //客户请求有语法错误
    FORBIDDEN_REQUEST,      //没有足够的访问权限
    INTERNAL_ERROR,         //服务器内部错误
    CLOSED_CONNECTION       //客户端已经关闭连接
};

static const char* szret[] = {"I get a correct result\n", "Something wrong\n"};


/**
 * @param buffer 应用程序读缓冲区
 * @param checked_index 执行buffer中正在分析的字节
 * @param read_index 指向buffer中客户数据尾部的下一字节
*/
LINE_STATUS parse_line(char* buffer, int& checked_index, int& read_index) {
    char temp;
    for ( ; checked_index < read_index; ++checked_index) {
        //获取当前要分析的字节
        temp = buffer[checked_index];
        //若为回车符，则说明可能读取到一个完整的行
        if (temp == '\r') {
            // 如果回车符是buffer中最后一个已被读入的客户数据，那么没有读取到完整行
            if ((checked_index + 1) == read_index) {
                return LINE_OPEN;
            }
            // 下一个字符为换行符，则成功读取到完整的行
            else if (buffer[checked_index + 1] == '\n') {
                buffer[checked_index++] = '\0';
                buffer[checked_index++] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }
        // 为换行符，可能读取到一个完整的行
        else if (temp == '\n') {
            if ((checked_index > 1) && buffer[checked_index - 1] == '\r') {
                buffer[checked_index-1] = '\0';
                buffer[checked_index++] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }
    }
    return LINE_OPEN;
}

// 分析请求行
HTTP_CODE parse_requestline(char* temp, CHECK_STATUS& checkstate) {
    char* url = strpbrk(temp, " \t");
    //若请求行中没有空白字符或回车符，则请求有错误
    if (!url) {
        return BAD_REQUEST;
    }
    *url++ = '\0';

    char* method = temp;
    // 不区分大小写
    if (strcasecmp(method, "GET") == 0) {
        printf("The request methon is GET\n");
    }
    else {
        return BAD_REQUEST;
    }

    url += strspn(url, " \t");
}

