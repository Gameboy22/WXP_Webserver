#ifndef HTTPCONNECTION_H
#define HTTPCONNECTION_H

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
#include<stdarg.h>
#include<sys/wait.h>
#include<sys/uio.h>
#include<map>

#include"../conn_mysql/sql_connection_pool.h"
#include"../time.h"
#include"../log/log.h"
#include"../locker/locker.h"

class http_conn
{
public:
    //文件名最大长度
    static const int FILENAME_LEN=200;
    //该缓冲区的大小
    static const int READ_BUFFER_SIZE=2048;
    //写缓冲区大小
    static const int WRITE_BUFFER_SIZE=1024;
    //http请求方式，目前仅支持GET方式
    enum METHOD {GET=0,POST,HEAD,PUT,DELETE,TRACE,OPTIONS,CONNECT,PATCH};
    //解析客户请求
    enum CHECK_STATE{CHECK_REQUESTINE=0,CHECK_STATE_HEADER,CHECK_STATE_CONTENT};
    //服务器处理HTTP请求的可能状态
    enum HTTP_CODE {NO_REQUEST,GET_REQUEST,BAD_REQUEST,NO__RESOURCE,FORBIDDEN_REQUEST,FILE__REQUEST,
    INTERNAL_ERROR,CLOSED_CONNECTION};
    //行的读取状态
    enum LINE_STATUS{LINE_OK=0,LINE_BAD,LINE_OPEN};   

public:
    http_conn(){}
    ~http_conn(){}
public:
    //初始化新接受的连接
    void init(int sockfd, const sockaddr_in &addr, char *root, int TRIGMode,
                     int close_log, string user, string passwd, string sqlname);
    //关闭连接
    void close_conn(bool real_close=true);
    //处理客户请求
    void process();
    //非阻塞读操作
    bool read();
    //非阻塞写操作
    bool write();
    sockaddr_in *get_address()
    {
        return &m_address;
    }
    void initmysql_result(connection_pool * connPool);
    void inittresultFile(connection_pool * connPool);
    int timer_flag;
    int improv;
private:
    //初始连接
    void init();
    //解析HTTP请求
    HTTP_CODE process_read();
    //填充HTTP应答
    bool process_write(HTTP_CODE ret);
    //用于process_read调用以分析HTTP请求
    HTTP_CODE parse_request_line(char* text);
    HTTP_CODE parse_headers(char* text);
    HTTP_CODE pares_content(char* text);
    HTTP_CODE do_request();
    char* get_line(){return m_read_buf+m_start_line;}
    LINE_STATUS parse_line();

    //用于process_write调用以填充HTTP应答
    void unmap();
    bool add_response(const char* format,...);
    bool add_content(const char* content);
    bool add_status_line(int status,const char* title);
    bool add_headers(int content_length);
    bool add_content_type();
    bool add_content_length(int content_lenght);
    bool add_linger();
    bool add_blank_line();
    

public:
    
    static int m_epollfd;//所有socket上的事件都被注册到一个epoll内核事件表中，所以将epoll文件描述符设置为静态的
    //统计用户数量
    static int m_user_count;
    MYSQL *mysql;
    int m_state;
private:
    //该HTTP连接的sockte和对方的socket地质
    int  m_socketfd;
    sockaddr_in m_address;

    //读缓冲区域
    char m_read_buf[READ_BUFFER_SIZE];
    //标识该缓冲区中已经读入的客户端数据的最后一个字节位置
    int m_read_idx;
    //当前正在分析的字符在读缓冲区中的位置
    int m_checked_idx;
    //当前正在解析的行起始位置
    int m_start_line;
    //写缓冲区
    char m_write_buf[WRITE_BUFFER_SIZE];
    //写缓冲区待发字节数
    int m_write_idx;
    //主状态机当前所处的状态
    CHECK_STATE m_check_state;
    //请求方法
    METHOD m_method;
    //客户端请求的，目标文件的完整路径，其内容等于doc_root+m_url,doc_root是网站根目录
    char m_real_file[FILENAME_LEN];
    //客户请求的目标文件的文件名
    char* m_url;
    //HTTP协议版本号，目前仅支持HTTP/1.1
    char* m_version;
    //主机名
    char* m_host;
    //HTTP请求的消息长度
    int m_content_length;
    //HTTP请求是否要求保持连接
    bool m_linger;

    //客户请求的目标文件被mmap到内存中的起始位置
    char* m_file_address;
    //目标文件的状态，通过它们可以判断文件是否存在、是否为目标、是否可读、并获取文件大小等信息
    struct stat m_file_stat;
    //采用writev来执行写操作，m_iv_count表示被写内存块的数量
    struct iovec m_iv[2];
    int m_iv_count;

    int cgi;//is open POST
    char *m_string;
    int btyes_to_send;
    int btyes_have_send;
    char *doc_root;

    map<string,string> m_users;
    int m_TRIGMode;
    int m_close_log;

    char sql_user[100];
    char sql_passwd[100];
    char sql_name[100];

};




#endif