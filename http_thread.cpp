#include<sys/socket.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<stdio.h>
#include<unistd.h>
#include<string.h>
#include<stdlib.h>
#include<errno.h>
#include<fcntl.h>
#include<sys/epoll.h>
#include<cassert>

#include"locker.h"
#include"./process_thread_pool/threadpool.h"
#include"http_conn.h"

#define MAX_FD 65536
#define MAx_EVENT_MUMBER 10000

extern int addfd(int epollfd,int fd,bool one_shot);
extern int removefd(int epollfd,int fd);

void addsig(int sig,void(handler)(int),bool restart=true)
{
    struct sigaction sa;
    memset(&sa,'\0',sizeof(sa));
    sa.sa_handler=handler;
    if(restart)
    {
        sa.sa_flags|=SA_RESTART;
    }

    sigfillset(&sa.sa_mask);
    assert(sigaction(sig,&sa,NULL)!=-1);
}

void show_error(int connfd,const char* info)
{
    printf("%s",info);
    send(connfd,info,strlen(info),0);
    close(connfd);
}


int main(int argc,char* argv[]){
    
	if(argc<=2){
	printf("no input ip_address port_number");return 1;}
	const char* ip=argv[1];
	int port=atoi(argv[2]);
	//忽略SIGPIPE信号
    addsig(SIGPIPE,SIG_IGN);

    //创建线程池
    threadpool<http_conn>* pool=NULL;
    try
    {
        //rintf("BBBB\n");
        /*这里报错，处理了一个小时，人麻了，可以删除对于代码看看报错，特别sb。都没找到问题，
        最后发现是locker.h中对信号量初始化时直接抛出了异常，没加以判断：如
        sem(){
        if(sem_init(&m_sem,0,0)!=0)
        {
            throw std::exception();  }
        正确为：
        sem(){
        if(sem_init(&m_sem,0,0)!=0)
        {
            throw std::exception();
        }}
        的逻辑原因是在创建线程池时，会创建locker类，而locker类会创建一个信号量，导致报错
        */
        pool=new threadpool<http_conn>(10,20);
    }
    catch(...)
    {
        throw std::exception();
        return 1;
    }
//为每一个线程创建分配一个http_conn
    http_conn* users=new http_conn[MAX_FD];
    assert(users);
    int user_count=0;

	int listenfd=socket(PF_INET,SOCK_STREAM,0);
	assert(listenfd>=0);
	struct linger tmp={1,0};
    setsockopt(listenfd,SOL_SOCKET,SO_LINGER,&tmp,sizeof(tmp));

    int ret=0;
	struct sockaddr_in address;
	bzero( &address,sizeof(address));
	address.sin_family=AF_INET;
	
	inet_pton(AF_INET,ip,&address.sin_addr);
	address.sin_port=htons(port);
	
	
	ret=bind(listenfd,(struct sockaddr*)&address,sizeof(address));
	assert(ret>=0);
	
	ret=listen(listenfd,5);
	assert(ret>=0);

    epoll_event events[MAx_EVENT_MUMBER];
    int epollfd=epoll_create(5);
    assert(epollfd!=-1);
    addfd(epollfd,listenfd,false);
    http_conn::m_epollfd=epollfd;

    while(true)
    {
        int number=epoll_wait(epollfd,events,MAx_EVENT_MUMBER,-1);
        if((number<=0)&&(errno!=EINTR))
        {
            printf("epoll failure\n");
            break;
        }

        for(int i=0;i<number;i++)
        {
            int sockfd=events[i].data.fd;
            if(sockfd==listenfd)
            {
                struct sockaddr_in client_address;
                socklen_t client_addrlength=sizeof(client_address);
                int connfd=accept(listenfd,(struct sockaddr*)&client_address,&client_addrlength);
                if(connfd<0)
                {
                    printf("errno is: %d\n",errno);
                    continue;
                }
                if(http_conn::m_user_count>=MAX_FD)
                {
                    printf("Max_FD\n");
                    show_error(connfd,"Internal server busy");
                    continue;
                }
                //初始化连接
                printf("successful\n");
                users[connfd].init(connfd,client_address);
                printf("init end\n");
            }
            else if(events[i].events&(EPOLLRDHUP|EPOLLHUP|EPOLLERR))
            {
                //如果有异常直接关闭
                printf(" have errno\n");
                users[sockfd].close_conn();
            }
            else if(events[i].events&EPOLLIN)
            {
                //根据读的结果，决定将任务添加到线程池还是关闭连接
                if(users[sockfd].read())
                {
                    pool->append(users+sockfd);
                }
                else
                {
                	printf(" EPOLLIN close\n");
                    users[sockfd].close_conn();
                }
            }
            else if(events[i].events&EPOLLOUT)
            {
                //根据写的结果，决定是否关闭连接
                if(!users[sockfd].write())
                {
                    printf("answer close\n");
                    users[sockfd].close_conn();
                }
            }
            else
            {
            printf("else\n");
            }
        }
    }
close(epollfd);
close(listenfd);
delete pool;
return 0;
}
