#ifdef LST_TIMER
#define LST_TIMER

#include <time.h>
#define BUFFER_SIZE 64
class util_timer;   //前向声明

/**
 * @brief 用户数据结构
*/
struct client_data
{
    sockaddr_in address;
    int sockfd;
    char buf[BUFFER_SIZE];
    util_timer* timer;
};

class util_timer{
public:
    util_timer():prev(NULL), next(NULL){}

    time_t expire;  //任务超时事件（绝对时间）
    void (*cb_func)(client_data*);  //任务回调函数
    client_data* user_data; //回调函数处理的客户数据
    util_timer* prev;   //指向前一个定时器
    util_timer* next;   //指向下一个定时器

}




class sort_timer_lst{
public:
    sort_timer_lst():head(NULL), tail(NULL) {}
    ~sort_timer_lst(){
        util_timer* tmp = head;
        while(tmp){
            head=tmp->next;
            delete tmp;
            tmp = head;
        }
    }


    //插入定时器，需保证顺序正确
    void add_timer(util_timer* timer){
        if(!timer){
            return;
        }
        if(!head){
            head = tail = timer;
            return;
        }
        if(timer->expire < head->expire){
            timer->next = head;
            head->prev = timer;
            head = timer;
        }
        add_timer(timer, head);
    }

    //当某个定时任务发生变化时，调整其在链表中的位置
    //这个函数只考虑被调整的定时器超时时间延长的情况
    void adjust_timer(util_timer* timer){
        if(!timer)
            return;
        util_timer* temp = timer->next;
        // 在链表尾部，或加时后，排序不变
        if(!temp || (timer->expire < temp->expire))
            return;
        // 
    }

private:
    util_timer* head;
    util_timer* tail;
}


#endif