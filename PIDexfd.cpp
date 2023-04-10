//进程之间传递文件描述符
#define _GUN_SOURCE 1
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
#include<time.h>
#include<sys/ioctl.h>
#include<poll.h>
#include<sys/epoll.h>
#include<signal.h>
#include<sys/wait.h>
#include<sys/mman.h>
#include<string>

static const int CONTROL_LEN=CMSG_LEN(sizeof(int));
void send_fd(int fd,int fd_to_send){
    struct iovec iov[1];
    struct msghdr msg;
    char buf[0];
    iov[0].iov_base=buf;
    iov[0].iov_len=1;
    msg.msg_name=NULL;
    msg.msg_namelen=0;
    msg.msg_iov=iov;
    msg.msg_iovlen=1;

    cmsghdr cm;
    cm.cmsg_len=CONTROL_LEN;
    cm.cmsg_level=SOL_SOCKET;
    cm.cmsg_type=SCM_RIGHTS;
    *(int *)CMSG_DATA(&cm)=fd_to_send;
    msg.msg_control=&cm;
    msg.msg_controllen=CONTROL_LEN;
    sendmsg(fd,&msg,0);
}


int recv_fd(int fd){
    struct iovec iov[1];
    struct msghdr msg;
    char buf[0];
    iov[0].iov_base=buf;
    iov[0].iov_len=1;
    msg.msg_name=NULL;
    msg.msg_namelen=0;
    msg.msg_iov=iov;
    msg.msg_iovlen=1;

    cmsghdr cm;
    msg.msg_control=&cm;
    msg.msg_controllen=CONTROL_LEN;
    recvmsg(fd,&msg,0);
    int fd_to_read= *(int *) CMSG_DATA(&cm);
    return fd_to_read;
}

int main(){
    int pipefd[2];
    int fd_to_pass=0;
    int ret=socketpair(PF_UNIX,SOCK_DGRAM,0,pipefd);
    assert(ret!=-1);
    pid_t pid=fork();
    assert(pid>=0);
    if(pid==0){
        close(pipefd[0]);
        fd_to_pass=open("test.txt",O_RDWR,0666);
        send_fd(pipefd[1],fd_to_pass>0?fd_to_pass:0);
        close(fd_to_pass);
        exit(0);
    }
    close(pipefd[1]);
    fd_to_pass=recv_fd(pipefd[0]);
    char buf[1024];
    memset(buf,'\0',1024);
    read(fd_to_pass,buf,1024);
    printf("%d     %s\n",fd_to_pass,buf);
    close(fd_to_pass);
}