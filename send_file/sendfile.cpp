//在网络上传输文件，以下代码作用是将服务器的代码发送到客户端上
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
#include<sys/sendfile.h>

#define BUFFER_SIZE 1024

static const char* status_line[2]={"200 OK","500 Internal server error"};


int main(int argc,char* argv[]){
	if(argc<=3){
	printf("no input ip_address port_number");return 0;}
	
	const char* ip=argv[1];
	int port=atoi(argv[2]);
	const char* file_name=argv[3];
	
	int filefd = open(file_name,O_RDONLY);
	assert(filefd>0);
	struct stat stat_buf;
	fstat(filefd,&stat_buf);
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
	
	struct sockaddr_in client;
	socklen_t client_addrlength=sizeof(client);
	int connfd=accept(sock,(struct sockaddr*)&client,&client_addrlength);
	if(connfd<0){
	printf("errno is %d\n",errno);
	}
	else{
	sendfile(connfd,filefd,NULL,stat_buf.st_size);
	close(connfd);
	}
	close(sock);
	return 0;
	}
	
	
	
