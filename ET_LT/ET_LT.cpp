//epoll对文件描述符的两种操作模式

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

#define MAX_EVENT_NUMBER 1024
#define BUFFER_SIZE 10

int setnonblocking(int fd){
	int old_option=fcntl(fd,F_GETFL);
	int new_option=old_option | O_NONBLOCK;
	fcntl(fd,F_SETFL,new_option);
	return old_option;
}

//将文件描述符fd上的EPOLLIN注册到epollfd指示的epoll内核事件表上，参数enable_et指定是否对fd启用ET模式
void addfd(int epollfd,int fd,bool enable_et){
	epoll_event event;
	event.data.fd=fd;
	event.events=EPOLLIN;
	if(enable_et)
	{
	event.events |=EPOLLET;
	}
	epoll_ctl(epollfd,EPOLL_CTL_ADD,fd,&event);
	setnonblocking(fd);
	
}
//LT工作模式
void lt(epoll_event* events,int number,int epollfd,int listenfd)
{
	char buf[BUFFER_SIZE];
	for(int i=0;i<number;i++){
	int sockfd = events[i].data.fd;
	if(sockfd==listenfd)
	{
	struct sockaddr_in client_address;
	socklen_t client_addrlenth = sizeof(client_address);
	int connfd=accept(listenfd,(struct sockaddr*)&client_address,&client_addrlenth);
	addfd(epollfd,connfd,false);
	}
	else if(events[i].events&EPOLLIN)
	{
	//只要sokcet读缓存中还有未读出的数据，这段代码就被触发
	printf("event trigger once\n");
	memset(buf,'\0',BUFFER_SIZE);
	int ret=recv(sockfd,buf,BUFFER_SIZE-1,0);
	if(ret<0){
	close(sockfd);
	continue;
	}
	printf("get %d bytes of content: %s\n",ret,buf);
	}
	else {
	printf("something else happened\n");
	}
 	} 
}


//ET工作模式
void et(epoll_event* events,int number,int epollfd,int listenfd)
{
	char buf[BUFFER_SIZE];
	for(int i=0;i<number;i++){
	int sockfd = events[i].data.fd;
	if(sockfd==listenfd)
	{
	struct sockaddr_in client_address;
	socklen_t client_addrlenth = sizeof(client_address);
	int connfd=accept(listenfd,(struct sockaddr*)&client_address,&client_addrlenth);
	addfd(epollfd,connfd,true);
	}
	else if(events[i].events&EPOLLIN)
	{
	//这段代码不会被重复触发，所以要循环取保把socket该缓存中的所有数据读出
	printf("event trigger once\n");
	while(1){
	memset(buf,'\0',BUFFER_SIZE);
	int ret=recv(sockfd,buf,BUFFER_SIZE-1,0);
	if(ret<0)
	{
		if((errno==EAGAIN)||(errno==EWOULDBLOCK))
		{
		printf("read later\n");
		break;}
		close(sockfd);
		break;
	}
	else if(ret==0){
	close(sockfd);
	}
	else{
	printf("get %d bytes of content: %s\n",ret,buf);
	}
	}
	}
	else {
	printf("something else happened\n");
	}
 	} 
	
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
	
	int sock=socket(PF_INET,SOCK_STREAM,0);
	assert(sock>=0);
	
	int ret=bind(sock,(struct sockaddr*)&address,sizeof(address));
	assert(ret!=-1);
	
	ret=listen(sock,5);
	assert(ret!=-1);
	
	epoll_event events[MAX_EVENT_NUMBER];
	int epollfd=epoll_create(5);
	assert(epollfd!=-1);
	addfd(epollfd,sock,true);
	
	while(1){
		ret=epoll_wait(epollfd,events,MAX_EVENT_NUMBER,-1);
		if(ret<0){
		printf("epoll failure\n");
		break;
		}
		//lt(events,ret,epollfd,sock);
		et(events,ret,epollfd,sock);
	}
	close(sock);
	return 0;
}

