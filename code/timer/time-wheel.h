#ifndef TIME_WHEEL_TIMER
#define TIME_WHEEL_TIMER

#include <time.h>
#include <netinet/in.h>
#include <stdio.h>

#define BUFFER_SIZE 64
class tw_timer;
struct client_data{
    sockaddr_in address;
    int sockfd;
    char buf[BUFFER_SIZE];
    tw_timer* timer;
};

class tw_timer{
public:
    tw_timer( int rot, int ts ) 
    : next( NULL ), prev( NULL ), rotation( rot ), time_slot( ts ){}

public:
    int rotation;   //记录定时器在时间轮旋转多少圈后生效
    int time_slot;  //记录定时器属于时间轮上的哪个槽
    void (*cb_func)( client_data* );    //定时器回调函数
    client_data* user_data; //客户数据
    tw_timer* next;
    tw_timer* prev;
};

class time_wheel{
public:
    time_wheel() : cur_slot( 0 ){
        for( int i = 0; i < N; ++i ){
            slots[i] = NULL;    //初始化每个槽的头节点
        }
    }

    ~time_wheel(){
        for( int i = 0; i < N; ++i ){
            tw_timer* tmp = slots[i];
            while( tmp ){
                slots[i] = tmp->next;
                delete tmp;
                tmp = slots[i];
            }
        }
    }

    // 根据定时值创建一个定时器，并将他插入合适的槽中
    tw_timer* add_timer( int timeout ){
        if( timeout < 0 ){
            return NULL;
        }
        int ticks = 0;
        // 根据超时值计算在时间轮旋转多少个滴答之后被触发
        if( timeout < TI ){
            ticks = 1;
        }
        else{
            ticks = timeout / TI;
        }
        // 计算时间轮旋转多少圈后触发
        int rotation = ticks / N;
        // 计算待插入的定时器应该被插入到哪个槽中
        int ts = ( cur_slot + ( ticks % N ) ) % N;
        tw_timer* timer = new tw_timer( rotation, ts );
        if( !slots[ts] ){
            printf( "add timer, rotation is %d, ts is %d, cur_slot is %d\n", rotation, ts, cur_slot );
            slots[ts] = timer;
        }
        else{
            timer->next = slots[ts];
            slots[ts]->prev = timer;
            slots[ts] = timer;
        }
        return timer;
    }

    void del_timer( tw_timer* timer ){   
        if( !timer ){
            return;
        }
        int ts = timer->time_slot;
        if( timer == slots[ts] ){
            slots[ts] = slots[ts]->next;
            if( slots[ts] ){
                slots[ts]->prev = NULL;
            }
            delete timer;
        }
        else{
            timer->prev->next = timer->next;
            if( timer->next ){
                timer->next->prev = timer->prev;
            }
            delete timer;
        }
    }

    //SI时间到后，调用该函数，时间轮向前滚动一个槽的间隔
    void tick(){
        tw_timer* tmp = slots[cur_slot];
        printf( "current slot is %d\n", cur_slot );
        while( tmp ){
            printf( "tick the timer once\n" );
            // 定时器在这一轮不起作用
            if( tmp->rotation > 0 ){
                tmp->rotation--;
                tmp = tmp->next;
            }
            //定时器到期，执行任务，并删除该定时器
            else{
                tmp->cb_func( tmp->user_data );
                if( tmp == slots[cur_slot] ){
                    printf( "delete header in cur_slot\n" );
                    slots[cur_slot] = tmp->next;
                    delete tmp;
                    if( slots[cur_slot] ){
                        slots[cur_slot]->prev = NULL;
                    }
                    tmp = slots[cur_slot];
                }
                else{
                    tmp->prev->next = tmp->next;
                    if( tmp->next ){
                        tmp->next->prev = tmp->prev;
                    }
                    tw_timer* tmp2 = tmp->next;
                    delete tmp;
                    tmp = tmp2;
                }
            }
        }
        // 更新时间轮的当前槽，以反映时间轮的转动
        cur_slot = ++cur_slot % N;
    }

private:
    //槽的数目
    static const int N = 60;
    //没1s时间轮转动一次，即槽间隔为1s
    static const int TI = 1; 
    //时间轮的槽，指向定时器链表
    tw_timer* slots[N];
    //时间轮的当前槽
    int cur_slot;
};

#endif