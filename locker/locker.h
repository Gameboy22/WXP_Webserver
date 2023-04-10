//封装三种线程同步机制  
#ifndef LOCKER_H
#define LOCKER_H
#include<exception>
#include<pthread.h>
#include<semaphore.h>

//封装信号量的类
class sem
{
private:
    /* data */
    sem_t m_sem;
public:
    sem(/* args */){
        //构造函数没有返回值，可以抛出异常来报告错误
        if(sem_init(&m_sem,0,0)!=0)
        {
            throw std::exception();
        }
        
    }
    //有参构造函数
    sem(int num){
        if(sem_init(&m_sem,0,num)!=0){
            throw std::exception();
        }
    }

    ~sem(){
        sem_destroy(&m_sem);
    }
    //等待信号量
    bool wait(){
        return sem_wait(&m_sem)==0;
    }
    //增加信号量
    bool post(){
        return sem_post(&m_sem)==0;
    }
};


//封装互斥锁的类
class locker
{
private:
    /* data */
    pthread_mutex_t m_mutex;
public:
    locker(/* args */){
        if(pthread_mutex_init(&m_mutex,NULL)!=0)
        {
            throw std::exception();
        }
    }
    ~locker(){
        pthread_mutex_destroy(&m_mutex);
    }
    //获取互斥锁
    bool lock(){
        return pthread_mutex_lock(&m_mutex)==0;
    }
    bool unlock(){
        return pthread_mutex_unlock(&m_mutex)==0;
    }
};
//封装条件变量的类

class cond
{
private:
    /* data */
    pthread_mutex_t m_mutex;
    pthread_cond_t m_cond;
public:
    cond(/* args */){
        if(pthread_mutex_init(&m_mutex,NULL)!=0){
            throw std::exception();
        }
        if(pthread_cond_init(&m_cond,NULL)!=0){
            //一旦构造函数有问题应该立即释放以份配资源
            pthread_mutex_destroy(&m_mutex);
            throw std::exception();
        }

    }
    ~cond(){
        pthread_mutex_destroy(&m_mutex);
        pthread_cond_destroy(&m_cond);
    }
    //等待变量
    bool wait(){
        int ret=0;
        pthread_mutex_lock(&m_mutex);
        ret=pthread_cond_wait(&m_cond,&m_mutex);
        pthread_mutex_unlock(&m_mutex);
        return ret==0;
    }
    bool signal(){
        return pthread_cond_signal(&m_cond)==0;
    }
};

#endif
