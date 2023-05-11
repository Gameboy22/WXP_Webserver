//升序定时器实现
#ifndef LST_TIMER
#define LST_TIMER
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <sys/stat.h>
#include <string.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/uio.h>


#include<time.h>
#include<cstdio>
#include<sys/socket.h>
#include<netinet/in.h>
#include"../log/log.h"
#define BUFFER_SIZE 64
class util_timer;

//用户数据结构：客户端socket地址、文件描述符、读缓存、定时器
struct client_data{
	sockaddr_in address;
	int sockfd;
	char buf[BUFFER_SIZE];
	util_timer* timer;
	
};

//定时器类
class util_timer
{
	public:
	util_timer():prev(NULL),next(NULL){}
	
	public:
	time_t expire;
	void (*cb_func)(client_data*);//任务回调函数
	client_data* user_data;
	util_timer* prev;
	util_timer* next;
	};
	
//定时器连表，他是一个升序、双向连表，带有头节点和尾节点

class sort_timer_lst{
	public :
	sort_timer_lst():head(NULL),tail(NULL){}
	~sort_timer_lst()
	{
	util_timer* tmp=head;
	while(tmp){
	head=tmp->next;
	delete tmp;
	tmp=head;
	}
	}
	//将目标定时器添加到链表中
	void add_timer(util_timer* timer){
	if(!timer){
	return;}
	if(!head){
	head=tail=timer;
	return;}
	if(timer->expire < head->expire){
        timer->next=head;
        head->prev=timer;
        head=timer;
        return;
    }
    add_timer(timer,head);
	}
    void adjust_timer(util_timer* timer){
        if(!timer){
            return;
        }
        util_timer* tmp=timer->next;
        if(!tmp||(timer->expire<tmp->expire)){
            return;
        }
        if(timer==head){
            head=head->next;
            head->prev=NULL;
            head->next=NULL;
            add_timer(timer,head);
        }
        else{
            timer->prev->next=timer->next;
            timer->next->prev=timer->prev;
            add_timer(timer,timer->next);
        }
    }
    void del_timer(util_timer* timer){
        if(!timer){
            return;
        }

        if((timer==head)&&(timer==tail)){
            delete timer;
            head=NULL;
            tail=NULL;
            return;
        }
        if(timer==head){
            head=head->next;
            head->prev=NULL;
            delete timer;
            return;
        }
        if(timer==tail){
            tail=tail->prev;
            tail->next=NULL;
            delete timer;
            return;
        }
        timer->prev->next=timer->next;
        timer->next->prev=timer->prev;
        delete timer;
        
    }
    void tick(){
        if(!head){
            return;
        }
        //printf("timer tick\n");
        time_t cur=time(NULL);
        util_timer* tmp=head;
        while(tmp){
            if(cur<tmp->expire){
                break;
            }
            tmp->cb_func(tmp->user_data);
            head=head->next;
            if(head){
                head->prev=NULL;
            }
            delete tmp;
            tmp=head;
        }
    }

    private:
    void add_timer(util_timer* timer,util_timer* lst_head){
        util_timer* prev=lst_head;
        util_timer* tmp=head->next;
        while (tmp)
        {
            if(timer->expire<tmp->expire){
                prev->next=timer;
                timer->next=tmp;
                tmp->prev=timer;
                timer->prev=prev;
                break;
            }
            prev=tmp;
            tmp=tmp->next;
                    }
            if(!tmp){
                prev->next=timer;
                timer->prev=prev;
                timer->next=NULL;
                tail=timer;
            }
        
    }
    util_timer* head;
	util_timer* tail;

};


class Utils
{
public:
    static int *u_pipefd;
    sort_timer_lst m_timer_lst;
    static int u_epollfd;
    int m_TIMESLOT;
public:
    Utils(){}
    ~Utils(){}
    void init(int timeslot){
        m_TIMESLOT=timeslot;
    }
    int setnonblocking(int fd){
        int old_option=fcntl(fd,F_GETFL);
        int new_option=old_option|O_NONBLOCK;
        fcntl(fd,F_SETFL,new_option);
        return old_option;
    }
    void addfd(int epollfd,int fd,bool one_shot,int TRIGMode){
        epoll_event event;
        event.data.fd=fd;
        if(1==TRIGMode){
            event.events=EPOLLIN|EPOLLET|EPOLLRDHUP;
        }
        else{
            event.events=EPOLLIN|EPOLLRDHUP;
        }

        if(one_shot)
            event.events|=EPOLLONESHOT;
        epoll_ctl(epollfd,EPOLL_CTL_ADD,fd,&event);
        setnonblocking(fd);
        
    }

    static void sig_handler(int sig){
         int save_errno = errno;
    int msg = sig;
    send(u_pipefd[1], (char *)&msg, 1, 0);
    errno = save_errno;
    }

    void addsig(int sig,void(handler)(int),bool restart=true){
        struct sigaction sa;
        memset(&sa, '\0', sizeof(sa));
        sa.sa_handler = handler;
        if (restart)
        sa.sa_flags |= SA_RESTART;
        sigfillset(&sa.sa_mask);
        assert(sigaction(sig, &sa, NULL) != -1);
    }

    void timer_handler(){
        m_timer_lst.tick();
        alarm(m_TIMESLOT);
    }

    void show_error(int connfd,const char *info){
        send(connfd,info,strlen(info),0);
        close(connfd);
    }
};



void cb_func(client_data *user_data);


#endif

