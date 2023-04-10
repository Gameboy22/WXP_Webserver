#include<sys/socket.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<assert.h>
#include<stdio.h>
#include<unistd.h>
#include<string.h>
#include<stdlib.h>
#include<errno.h>
#include<sys/stat.h>
#include<sys/types.h>
#include<fcntl.h>
#include<sys/uio.h>
#include<pthread.h>
#include<sys/epoll.h>
#include<signal.h>
#include<string.h>
#include"lst_timer.h"

#define MAX_EVENT_NUMBER 1024
#define FD_LIMIT 65535
#define TIMESLOT 5
static int pipefd[2];
static sort_timer_lst timer_lst;
static int epollfd=0;



int setnonblocking(int fd){
	int old_option=fcntl(fd,F_GETFL);
	int new_option=old_option | O_NONBLOCK;
	fcntl(fd,F_SETFL,new_option);
	return old_option;
}

//将文件描述符fd上的EPOLLIN注册到epollfd指示的epoll内核事件表上，参数enable_et指定是否对fd启用ET模式
void addfd(int epollfd,int fd){
	epoll_event event;
	event.data.fd=fd;
	event.events=EPOLLIN | EPOLLET;
	epoll_ctl(epollfd,EPOLL_CTL_ADD,fd,&event);
	setnonblocking(fd);
	
}

void sig_handler(int sig){
	int save_errno=errno;
	int msg=sig;
	send(pipefd[1],(char* )&msg,1,0);
	errno=save_errno;
}


void addsig(int sig){
	struct sigaction sa;
	memset(&sa,'\0',sizeof(sa));
	sa.sa_handler=sig_handler;
	sa.sa_flags |=SA_RESTART;
	sigfillset(&sa.sa_mask);
	assert(sigaction(sig,&sa,NULL)!=-1);
}

void timer_handler(){
	timer_lst.tick();
	alarm(TIMESLOT);
}

void cb_func(client_data* user_data){
	epoll_ctl(epollfd,EPOLL_CTL_DEL,user_data->sockfd,0);
	assert(user_data);
	close(user_data->sockfd);
	printf("close fd %d\n",user_data->sockfd);
}

int main(int argc,char* argv[]){
	if(argc<=2){
	printf("no input ip_address port_number");return 0;}
	
	const char* ip=argv[1];
	int port=atoi(argv[2]);
	
	struct sockaddr_in address;
	bzero( &address,sizeof(address));
	address.sin_family=AF_INET;
	
	inet_pton(AF_INET,ip,&address.sin_addr);
	address.sin_port=htons(port);
	
	int listenfd=socket(PF_INET,SOCK_STREAM,0);
	assert(listenfd>=0);
	
	int ret=bind(listenfd,(struct sockaddr*)&address,sizeof(address));
	assert(ret!=-1);
	
	ret=listen(listenfd,5);
	assert(ret!=-1);
	
	epoll_event events[MAX_EVENT_NUMBER];
	int epollfd=epoll_create(5);
	assert(epollfd!=-1);
	addfd(epollfd,listenfd);
	ret=socketpair(PF_UNIX,SOCK_STREAM,0,pipefd);
	assert(ret!=-1);
	setnonblocking(pipefd[1]);
	addfd(epollfd,pipefd[0]);
	addsig(SIGALRM);
	addsig(SIGTERM);
	bool stop_server=false;
	client_data* users=new client_data[FD_LIMIT];
	bool timeout=false;


#define TIMEOUT 5000
int timeout=TIMEOUT;
time_t start=time(NULL);
time_t end=time(NULL);


	while(1){
        printf("the timeout is now %d mil-seconds\n",timeout);
        start=time(NULL);
		int number=epoll_wait(epollfd,events,MAX_EVENT_NUMBER,timeout);
		if((number<0)&&(errno!=EINTR)){
		printf("epoll failure\n");
		break;
		}

        if(number==0){
            timeout=TIMEOUT;
            continue;
        }
        end=time(NULL);
        timeout-=(end-start)*1000;
        if(timeout<=0){
            timeout=TIMEOUT;
        }
		 //handle connections
		
	}
	close(listenfd);
	close(pipefd[1]);
	close(pipefd[0]);
	delete []users;
	return 0;
}
