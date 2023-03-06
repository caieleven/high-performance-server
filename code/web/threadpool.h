#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <list>
#include <cstdio>
#include <exception>
#include <pthread.h>
#include "locker.h"

/**
 * @brief 线程池类
 * @tparam T 任务类
*/
template< typename T >
class threadpool{
public:
    /**
     * @param thread_number 线程池中线程数量
     * @param max_requests 请求队列中最多允许、等待处理的请求的数量
    */
    threadpool( int thread_number = 8, int max_requests = 10000 );
    ~threadpool();
    bool append( T* request );

private:
    /**
     * @brief 工作线程运行的函数，不断从工作队列中取出任务并执行
    */
    static void* worker( void* arg );
    void run();

private:
    int m_thread_number;                // 线程数
    int m_max_requests;                 // 请求队列允许的最大请求数
    pthread_t* m_threads;               // 描述线程池的数组，大小为m_thread_number
    std::list< T* > m_workqueue;        // 请求队列
    locker m_queuelocker;               // 保护请求队列的互斥锁
    sem m_queuestat;                    // 是否有任务需要处理
    bool m_stop;                        // 是否结束线程
};

// 创建线程
template< typename T >
threadpool< T >::threadpool( int thread_number, int max_requests ) : 
        m_thread_number( thread_number ), m_max_requests( max_requests ), m_stop( false ), m_threads( NULL ){
    if( ( thread_number <= 0 ) || ( max_requests <= 0 ) ){
        throw std::exception();
    }

    m_threads = new pthread_t[ m_thread_number ];
    if( ! m_threads ){
        throw std::exception();
    }
    // 创建线程，并将他们设置为脱离线程
    for ( int i = 0; i < thread_number; ++i ){
        printf( "create the %dth thread\n", i );
        if( pthread_create( m_threads + i, NULL, worker, this ) != 0 ){
            delete [] m_threads;
            throw std::exception();
        }
        if( pthread_detach( m_threads[i] ) ){
            delete [] m_threads;
            throw std::exception();
        }
    }
}



template< typename T >
threadpool< T >::~threadpool()
{
    delete [] m_threads;
    m_stop = true;
}


// 添加任务
template< typename T >
bool threadpool< T >::append( T* request ){
    m_queuelocker.lock();
    if ( m_workqueue.size() > m_max_requests ){
        m_queuelocker.unlock();
        return false;
    }
    m_workqueue.push_back( request );
    m_queuelocker.unlock();
    m_queuestat.post();
    return true;
}


// 由于pthread_create，第三个参数只能是静态函数，要在静态函数中使用类的动态成员，如成员函数和成员变量，所以，需要将类的对象作为参数传递给静态函数
template< typename T >
void* threadpool< T >::worker( void* arg ){
    threadpool* pool = ( threadpool* )arg;
    pool->run();
    return pool;
}

template< typename T >
void threadpool< T >::run(){
    while ( ! m_stop ){
        // 与append对应
        m_queuestat.wait();
        m_queuelocker.lock();
        if ( m_workqueue.empty() ){
            m_queuelocker.unlock();
            continue;
        }
        T* request = m_workqueue.front();
        m_workqueue.pop_front();
        m_queuelocker.unlock();
        if ( ! request ){
            continue;
        }
        request->process();
    }
}

#endif
