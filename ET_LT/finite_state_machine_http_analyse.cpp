//使用主从两个有限状态机器实现一个简单的HTTP请求的读取和分析
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

#define BUFFER_SIZE 4096 //读取缓冲区大小

//主状态机器可能的两种状态，正在分析请求行，正在分析头部字段
enum CHECK_STATE{CHECK_STATE_REQUESTLINE=0,CHECK_STATE_HEADER};

//从状态机器的三种状态，即行的读取状态:读取到一个完整行，出错行，行数据不完整
enum LINE_STATUS{ LINE_OK=0,LINE_BAD,LINE_OPEN};

//服务器处理HHTP请求的结果：请求不完整，需要继续读取客户端数据；完整的请求；客户请求语法有错误；客户对资源没有足够的访问权限；服务器内部错误；客户端以关闭连接；
enum HTTP_CODE{NO_REQUEST,GET_REQUEST,BAD_REQUEST,
FORBIDDEN_REQUEST,INTERNAL_ERROR,CLOSE_CONNECTION};

//简化问题，知识给客户端发送成功和失败的信息，不发送一个完整的HTTP应答报文
static const char* szret[]={"I get a corrext result\n","Something wrong\n"};

//从状态机，解析出一行的内容
LINE_STATUS parse_line(char* buffer,int& checked_index,int& read_index){
	char temp;
	for(;checked_index<read_index;++checked_index){
		temp=buffer[checked_index];
		if(temp=='\t'){
		if(checked_index+1==read_index){
		return LINE_OPEN;}
		else if(buffer[checked_index+1]=='\n'){
		buffer[checked_index++]='\0';
		buffer[checked_index++]='\0';
		return LINE_OK;
		}
		return LINE_BAD;
		}
		else if(temp=='\n'){
		if((checked_index>1)&&buffer[checked_index-1]=='\r'){
		buffer[checked_index++]='\0';
		buffer[checked_index++]='\0';
		return LINE_OK;
		}
		return LINE_BAD;
		}}
		return LINE_OPEN;}

//分析请求行
HTTP_CODE parse_requestion(char* temp,CHECK_STATE& checkstate)
{
	char* url=strpbrk(temp,"\t");
	if(!url){
		printf("parse_requestion error\t %s\n",temp);
		return BAD_REQUEST;
		}
	*url++='\0';
	char* method=temp;
	if(strcasecmp(method,"GET")==0)
	{
		printf("The request method is GET\n");
	}
	else
	{
		return BAD_REQUEST;
	}
	
	url+=strspn(url,"\t"); 
	char* version=strpbrk(url,"\t");
	if(!version){
	return BAD_REQUEST;}
	*version++='\0';
	version+=strspn(version,"\t");
	if(strcasecmp(version,"HTTP/1.2")!=0){
	return BAD_REQUEST;}
	if(strncasecmp(url,"http://",7)==0){
	url+=7;
	url=strchr(url,'/');}
	if(!url || url[0]!='/')
	{
	return BAD_REQUEST;
	}
	printf("The request URL is:%s\n",url);
	checkstate = CHECK_STATE_HEADER;
	return NO_REQUEST;
}
	
	
//分析头部
HTTP_CODE parse_headers(char* temp){
	if(temp[0]=='\0'){
		printf("parse_headers error\t %s\n",temp);
		return GET_REQUEST;
	}
	else if (strncasecmp(temp,"Host:",5)==0)
	{
		temp+=5;
		temp+=strspn(temp,"\t");
		printf("the request host is:%s\n",temp);
	}
	else
	{
		printf("I can not handle this header\n");
	}
	return NO_REQUEST;
}
//分析HTTP请求的入口函数
HTTP_CODE parse_content(char* buffer,int& checked_index,CHECK_STATE&
			checkstate,int& read_index,int& start_line)
{
	LINE_STATUS linestatus=LINE_OK;
	HTTP_CODE retcode=NO_REQUEST;
	
	while((linestatus=parse_line(buffer,checked_index,read_index)) == LINE_OK)
	{
		char* temp=buffer+start_line;
		start_line=checked_index;
	switch(checkstate)
	{
		case CHECK_STATE_REQUESTLINE:
		{
			retcode=parse_requestion(temp,checkstate);
			if(retcode==BAD_REQUEST)
			{
				return BAD_REQUEST;
			}
			break;
		}
		case CHECK_STATE_HEADER:
		{
			retcode=parse_headers(temp);
			if(retcode==BAD_REQUEST)
			{
				return BAD_REQUEST;
			}
			else if(retcode==GET_REQUEST)
			{
				return GET_REQUEST;
			}
			break;
		}
		default:
		{
		return INTERNAL_ERROR;
		}
	}
	}
	if(linestatus==LINE_OPEN)
	{
		return NO_REQUEST;
	}
	else
	{
		return BAD_REQUEST;
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
	
	struct sockaddr_in client;
	socklen_t client_addrlength=sizeof(client);
	int connfd=accept(sock,(struct sockaddr*)&client,&client_addrlength);
	if(connfd<0){
	printf("errno is %d\n",errno);
	}
	else{
	char buffer[BUFFER_SIZE];
	memset(buffer,'\0',BUFFER_SIZE);
	int data_read=0;
	int read_index=0;
	int checked_index=0;
	int start_line=0;
	CHECK_STATE checkstate=CHECK_STATE_REQUESTLINE;
	while(1){
	data_read=recv(connfd,buffer+read_index,BUFFER_SIZE-read_index,0);
	if(data_read==-1)
	{
		printf("reading failed\n");
		break;
	}
	else if(data_read==0)
	{
		printf("remote client has closed the connection\n");
		break;
	}
	read_index+=data_read;
	HTTP_CODE result=parse_content(buffer,checked_index,
	checkstate,read_index,start_line);
	
	if(result==NO_REQUEST){
	continue;
	}
	else if(result==GET_REQUEST)
	{
		send(connfd,szret[0],strlen(szret[0]),0);
		break;
	}
	else
	{
		send(connfd,szret[1],strlen(szret[1]),0);
		break;
	}
}
	
	close(connfd);
	}
	close(sock);
	return 0;
}
			
	





