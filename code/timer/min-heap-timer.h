#ifndef MIN_HEAP_TIMER_H
#define MIN_HEAP_TIMER_H

#include <iostream>
#include <netinet/in.h>
#include <time.h>

using std::exception;

#define BUFFER_SIZE 64

class heap_timer;

struct client_data
{
    sockaddr_in address;
    int sockfd;
    char buf[BUFFER_SIZE];
    heap_timer* timer;
};

class heap_timer{
public:
    heap_timer(int delay){
        expire = time(NULL) + delay;
    }

public:
    time_t expire;                      //定时器生效的绝对事件
    void (*cb_func) (client_data*);     //定时器的回调函数
    client_data* user_data;             //用户数据
};

class time_heap{
public:
    /**
     * @brief 构造函数
     * @param cap 堆容量
    */
    time_heap(int cap) throw(std::exception): capacity(cap), cur_size(0){
        array = new heap_timer* [capacity];
        if(!array){
            throw std::exception();
        }
        for(int i = 0; i < capacity; i++){
            array[i] == NULL;
        }
    }
    /**
     * 
    */
    time_heap(heap_timer** init_array, int size, int capacity) throw(std::exception):cur_size(size), capacity(capacity){
        if(capacity < size){
            throw std::exception();
        }
        if(!array){
            throw std::exception();
        }
        for(int i = 0; i < capacity; i++){
            array[i] = NULL;
        }
        if(size != 0){
            for(int i = 0; i < size; i++){
                array[i] = init_array[i];
            }
            // 下滤
            for(int i = (cur_size-1)/2; i >= 0; --i){
                percolate_down(i);
            }
        }
    }

    ~time_heap(){
        for(int i = 0; i < cur_size; i++){
            delete array[i];
        }
        delete [] array;
    }

    /**
     * 添加定时器，若容量不够，会自动扩容
    */
    void add_time(heap_timer* timer) throw(std::exception){
        if(!timer){
            return;
        }
        if(cur_size >= capacity){
            resize();
        }
        int hole = cur_size++;
        int parent = 0;
        for( ; hole > 0; hole=parent){
            parent = (hole-1)/2;
            if(array[parent]->expire <= timer->expire){
                break;
            }
            array[hole] = array[parent];
        }
        array[hole] = timer;
    }

    void del_timer(heap_timer* timer){
        if(!timer){
            return;
        }
        // 仅仅将目标定时器的回调函数设为空，即所谓的延迟销毁
        // 节省真正删除定时器造成的开销，但容易使堆数组膨胀
        timer->cb_func = NULL;
    }

    // 获取堆顶计时器
    heap_timer* top() const{
        if(empty()){
            return NULL;
        }
        return array[0];
    }

    // 删除堆顶定时器
    void pop_timer(){
        if(empty()){
            return;
        }
        if(array[0]){
            delete array[0];
            array[0] = array[--cur_size];
            // 从堆顶向下调整
            percolate_down(0);
        }
    }

    //判空
    bool empty() const{
        return cur_size == 0;
    }

    // 心搏函数
    void tick(){
        heap_timer* tmp = array[0];
        time_t cur = time(NULL);
        while (!empty())
        {
            if(!tmp){
                break;
            }
            // 未到期
            if(tmp->expire > cur){
                break;
            }
            // 到期，执行回调函数
            if(array[0]->cb_func){
                array[0]->cb_func(array[0]->user_data);
            }
            pop_timer();
            tmp = array[0];
        }
    }

private:
    // 最小堆下滤，确保以第hole个节点为根节点的子树为最小堆
    void percolate_down(int hole){
        heap_timer* temp = array[hole];
        int child = 0;
        for(; (hole*2+1) <= cur_size-1; hole = child){
            child = hole * 2 + 1;
            // 找到最小孩子
            if(child < cur_size-1 && array[child+1]->expire < array[child]->expire){
                ++child;
            }
            // 交换
            if(array[child]->expire < temp->expire){
                array[hole] = array[child];
            }
            else{
                break;
            }
        }
        array[hole] = temp;
    }

    // 将对数组容量翻倍
    void resize() throw(std::exception){
        heap_timer** temp = new heap_timer* [2*capacity];
        for(int i  = 0; i < 2*capacity; i++){
            temp[i] = NULL;
        }
        if(!temp){
            throw std::exception();
        }
        capacity *= 2;
        for(int i = 0; i < cur_size; i++){
            temp[i] = array[i];
        }
        delete [] array;
        array = temp;
    }



private:
    heap_timer** array;     //堆数组
    int capacity;           //堆数组容量
    int cur_size;           //堆数组当前包含元素个数
};


#endif