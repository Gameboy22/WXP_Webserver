#include"http_conn.h"

#include<mysql/mysql.h>
#include<fstream>
//定义HTTP响应状态信息
const char* ok_200_title="OK";

const char* error_400_title="Bad Request";
const char* error_400_form="Your request has bad syntax or is inherently impossible to satisfy.\n";
const char* error_403_title="Forbidden";
const char* error_403_from="You not have permission to get file from this server.\n";
const char* error_404_title="Not Found";
const char* error_404_from="The requested file was not found on this server.\n";
const char* error_500_title="Internal";
const char* error_500_from="There was an unusual problem serving the reqeusted file.\n";

//网站的根目录
const char* doc_root="/var/www/html";

locker m_lock;
map<string,string> users;


void http_conn::initmysql_result(connection_pool *connPool)
{
    MYSQL* mysql=NULL;
    connectionRAII mysqlcon(&mysql,connPool);

    if(mysql_query(mysql,"SELECT username,passwd FROM user"))
    {
        LOG_ERROR("SELECT error:%s\n",mysql_error(mysql));
    }
    //检索结果
    MYSQL_RES *result=mysql_store_result(mysql);

    int num_fields=mysql_num_fields(result);

    MYSQL_FIELD *fields=mysql_fetch_fields(result);

    while(MYSQL_ROW row=mysql_fetch_row(result))
    {
        string temp1(row[0]);
        string temp2(row[1]);
        users[temp1]=temp2;
    }
}

 int setnonblocking(int fd){
    int old_option=fcntl(fd,F_GETFL);
    int new_option=old_option | O_NONBLOCK;
    fcntl(fd,F_SETFL,new_option);
    return old_option;
}


void addfd(int epollfd,int fd,bool one_shot,int TRIGMode){
    epoll_event event;
    event.data.fd=fd;
    if(TRIGMode==1){
        event.events=EPOLLIN|EPOLLET|EPOLLRDHUP;
    }
    else{
        event.events=EPOLLIN|EPOLLRDHUP;
    }
    
    if(one_shot)
    {
        event.events|=EPOLLONESHOT;
    }
    epoll_ctl(epollfd,EPOLL_CTL_ADD,fd,&event);
    setnonblocking(fd);
    }

void removefd(int epollfd,int fd)
{
    epoll_ctl(epollfd,EPOLL_CTL_DEL,fd,0);
    close(fd);
}
void modfd(int epollfd,int fd,int ev, int TRIGMode)
{
    epoll_event event;
    event.data.fd=fd;
    if(1==TRIGMode)
    event.events=ev|EPOLLET|EPOLLONESHOT|EPOLLRDHUP;
    else
    event.events=ev|EPOLLONESHOT|EPOLLRDHUP;
    epoll_ctl(epollfd,EPOLL_CTL_MOD,fd,&event);

}

int http_conn::m_user_count=0;
int http_conn::m_epollfd=-1;


void http_conn::close_conn(bool real_close)
{
    if(real_close && m_socketfd!=-1)
    {
        removefd(m_epollfd,m_socketfd);
        m_user_count--;
        m_socketfd=-1;
    }
}

void http_conn::init(int sockfd, const sockaddr_in &addr, char *root, int TRIGMode,
                     int close_log, string user, string passwd, string sqlname)
{
    m_socketfd=sockfd;
    m_address=addr;
    //下两行为调试代码，避免TIME_WAIT
    // int reuser=1;
    // setsockopt(m_socketfd,SOL_SOCKET,SO_REUSEADDR,&reuser,sizeof(reuser));

    addfd(m_epollfd,sockfd,true,TRIGMode);
    m_user_count++;

    doc_root=root;
    m_TRIGMode=TRIGMode;
    m_close_log=close_log;
    
    strcpy(sql_name,user.c_str());
    strcpy(sql_passwd, passwd.c_str());
    strcpy(sql_name, sqlname.c_str());

    init();
}
void http_conn::init()
{
    mysql=NULL;
    btyes_have_send=0;
    btyes_to_send=0;
    m_check_state=CHECK_REQUESTINE;
    m_linger=false;
    m_method=GET;
    m_url=0;
    m_version=0;
    m_content_length=0;
    m_host=0;
    m_start_line=0;
    m_checked_idx=0;
    m_read_idx=0;
    m_write_idx=0;
    memset(m_read_buf,'\0',READ_BUFFER_SIZE);
    memset(m_write_buf,'\0',WRITE_BUFFER_SIZE);
    memset(m_real_file,'\0',FILENAME_LEN);
}

http_conn::LINE_STATUS http_conn::parse_line()
{
    char temp;
    for(;m_checked_idx<m_read_idx;++m_checked_idx)
    {
        temp=m_read_buf[m_checked_idx];
        if(temp=='\r')
        {
            if((m_checked_idx+1)==m_read_idx)
            {
                return LINE_OPEN;
            }
            else if(m_read_buf[m_checked_idx+1]=='\n')
            {
                m_read_buf[m_checked_idx++]='\0';
                m_read_buf[m_checked_idx++]='\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }
        else if(temp=='\n')
    {
        if((m_checked_idx>1)&&(m_read_buf[m_checked_idx-1]=='\r'))
        {
            m_read_buf[m_checked_idx++]='\0';
            m_read_buf[m_checked_idx++]='\0';
            return LINE_OK;
        }
        return LINE_BAD;
    }
    }
    return LINE_OPEN;
}


//循环读取客户数据，指导无数据可读才关闭
bool http_conn::read()
{
    if(m_read_idx>=READ_BUFFER_SIZE)
    {
        return false;
    }

    int bytes_read=0;

    //LT读数据
    if(0==m_TRIGMode)
    {
        bytes_read=recv(m_socketfd,m_read_buf+m_read_idx,READ_BUFFER_SIZE-m_read_idx,0);
        m_read_idx+=bytes_read;
        if(bytes_read<=0)
        {
            return false;
        }
        return true;
    }
    else{
    while (true)
    {
        bytes_read=recv(m_socketfd,m_read_buf+m_read_idx,READ_BUFFER_SIZE-m_read_idx,0);
        if(bytes_read==-1)
        {
            if(errno==EAGAIN||errno==EWOULDBLOCK)
            {
                break;
            }
            return false;
        }
        else if(bytes_read==0)
        {
            return false;
        }
        m_read_idx+=bytes_read;
    }
    return true;
    }
}

//解析HTTP祈求行
http_conn::HTTP_CODE http_conn::parse_request_line(char* text)
{
    m_url=strpbrk(text,"\t");
    if(!m_url)
    {
        return BAD_REQUEST;
    }
    *m_url++='\0';

    char* method=text;
    if(strcasecmp(method,"GET")==0)
    {
        m_method=GET;

    }
    else if(strcasecmp(method,"POST")==0)
    {
        m_method=POST;
        cgi=1;
    }
    else
    {
        return BAD_REQUEST;
    }
    m_url+=strspn(m_url," \t");
    m_version=strpbrk(m_url," \t");
    if(!m_version)
    {
        return BAD_REQUEST;
    }
    *m_version++='\0';
    m_version+=strspn(m_version," \t");
    if(strcasecmp(m_version,"HTTP/1.1")!=0)
    {
        return BAD_REQUEST;
    }
    if(strncasecmp(m_url,"http://",7)==0)
    {
        m_url+=7;
        m_url=strchr(m_url,'/');
    }

    if(strncasecmp(m_url,"https://",8)==0){
        m_url+=8;
        m_url=strchr(m_url,'/');
    }

    if(!m_url||m_url[0]!='/')
    {
        return BAD_REQUEST;
    }
    if(strlen(m_url)==1)
    strcat(m_url,"judge.html");
    m_check_state=CHECK_STATE_HEADER;
    return NO_REQUEST;

}

//解析HTTP请求的头部信息
http_conn::HTTP_CODE http_conn::parse_headers(char* text)
{
    //空行表示头部以解析完毕
    if(text[0]=='\0')
    {
        if(m_content_length!=0)
        {
            m_check_state=CHECK_STATE_CONTENT;
            return NO_REQUEST;
        }

        return GET_REQUEST;
    }
    else if(strncasecmp(text, "Connection:", 11) == 0){
         text += 11;
        text += strspn(text, " \t");
        if (strcasecmp(text, "keep-alive") == 0)
        {
            m_linger = true;
        }
    }
    //处理头部
    else if(strncasecmp(text,"Content-Length:",15)==0)
    {
        text+=15;
        text+=strspn(text," \t");
        m_host+=strspn(text," \t");
        m_content_length=atol(text);

    }
    //处理Host头部字节
    else if(strncasecmp(text,"Host:",5)==0)
    {
        text+=15;
        text+=strspn(text," \t");
        m_host=text;
        m_content_length=atol(text);
    }
    else
    {
        LOG_INFO("opp! unknow header %s",text);
    }
    return NO_REQUEST;
}

//并没有真正解析HTTP请求体，只是判断他是否被完整读如
http_conn::HTTP_CODE http_conn::pares_content(char* text)
{
    if(m_read_idx>=(m_content_length+m_checked_idx))
    {
        text[m_content_length]='\0';
        m_string=text;
        return GET_REQUEST;
    }
    return NO_REQUEST;
}


//主状态机
http_conn::HTTP_CODE http_conn::process_read()
{
    LINE_STATUS line_status=LINE_OK;
    HTTP_CODE ret=NO_REQUEST;
    char* text=0;
    
    while (((m_check_state==CHECK_STATE_CONTENT)&&(line_status==LINE_OK))||((line_status=parse_line())==LINE_OK))
    {
        //printf("%d\n",m_check_state);
        text=get_line();
        m_start_line=m_checked_idx;
        LOG_INFO("got 1 http line:%s",text);
        switch (m_check_state)
        {
        case CHECK_REQUESTINE:
        {
            //printf("CHECK_REQUESTINE\n");
            ret=parse_request_line(text);
            if(ret==BAD_REQUEST)
            {
                return BAD_REQUEST;
            }
            //printf("CHECK_REQUESTINE end\n");
            break;
        }
        case CHECK_STATE_HEADER:
        {
            ret=parse_headers(text);
            if(ret==BAD_REQUEST)
            {
                return BAD_REQUEST;
            }
            else if(ret==GET_REQUEST)
            {
                //printf("do_request\n");
                return do_request();
            }
            break;
        }
        case CHECK_STATE_CONTENT:
        {
            ret=pares_content(text);
            if(ret==GET_REQUEST)
            {
                return do_request();
            }
            line_status=LINE_OPEN;
            break;
        } 
        
        default:
        {
            return INTERNAL_ERROR;
            break;
        }
            
        }
        //printf("read end\n");
    }
    return NO_REQUEST;
    
}

//分析请求，分析文件的属性，如果目标文件存在，对用户可读，且不是目录，则使用mmap将其映射到内存m_file_address中，并告诉调用着获取文件成功

http_conn::HTTP_CODE http_conn::do_request()
{
    strcpy(m_real_file,doc_root);
    int len=strlen(doc_root);
    const char *p=strrchr(m_url,'/');

    //处理cgi
    if (cgi == 1 && (*(p + 1) == '2' || *(p + 1) == '3')){
        char flag = m_url[1];
        char *m_url_real=(char*)malloc(sizeof(char)* 200);
        strcpy(m_url_real, "/");
        strcat(m_url_real, m_url + 2);
        strncpy(m_real_file + len, m_url_real, FILENAME_LEN - len - 1);
        free(m_url_real);
        //将用户名和密码提取出来
        //user=123&passwd=123
        char name[100], password[100];
        int i;
        for (i = 5; m_string[i] != '&'; ++i)
            name[i - 5] = m_string[i];
            name[i - 5] = '\0';

        int j = 0;
        for (i = i + 10; m_string[i] != '\0'; ++i, ++j)
            password[j] = m_string[i];
        password[j] = '\0';

        if (*(p + 1) == '3')
        {
            //如果是注册，先检测数据库中是否有重名的
            //没有重名的，进行增加数据
            char *sql_insert = (char *)malloc(sizeof(char) * 200);
            strcpy(sql_insert, "INSERT INTO user(username, passwd) VALUES(");
            strcat(sql_insert, "'");
            strcat(sql_insert, name);
            strcat(sql_insert, "', '");
            strcat(sql_insert, password);
            strcat(sql_insert, "')");

            if (users.find(name) == users.end())
            {
                m_lock.lock();
                int res = mysql_query(mysql, sql_insert);
                users.insert(pair<string, string>(name, password));
                m_lock.unlock();

                if (!res)
                    strcpy(m_url, "/log.html");
                else
                    strcpy(m_url, "/registerError.html");
            }
            else
                strcpy(m_url, "/registerError.html");
        }
        //如果是登录，直接判断
        //若浏览器端输入的用户名和密码在表中可以查找到，返回1，否则返回0
        else if (*(p + 1) == '2')
        {
            if (users.find(name) != users.end() && users[name] == password)
                strcpy(m_url, "/welcome.html");
            else
                strcpy(m_url, "/logError.html");
        }
    }

    if (*(p + 1) == '0')
    {
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/register.html");
        strncpy(m_real_file + len, m_url_real, strlen(m_url_real));

        free(m_url_real);
    }
    else if (*(p + 1) == '1')
    {
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/log.html");
        strncpy(m_real_file + len, m_url_real, strlen(m_url_real));

        free(m_url_real);
    }
    else if (*(p + 1) == '5')
    {
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/picture.html");
        strncpy(m_real_file + len, m_url_real, strlen(m_url_real));

        free(m_url_real);
    }
    else if (*(p + 1) == '6')
    {
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/video.html");
        strncpy(m_real_file + len, m_url_real, strlen(m_url_real));

        free(m_url_real);
    }
    else if (*(p + 1) == '7')
    {
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/fans.html");
        strncpy(m_real_file + len, m_url_real, strlen(m_url_real));

        free(m_url_real);
    }
    else
        strncpy(m_real_file + len, m_url, FILENAME_LEN - len - 1);

    if (stat(m_real_file, &m_file_stat) < 0)
        return NO_REQUEST;

    if (!(m_file_stat.st_mode & S_IROTH))
        return FORBIDDEN_REQUEST;

    if (S_ISDIR(m_file_stat.st_mode))
        return BAD_REQUEST;

    int fd = open(m_real_file, O_RDONLY);
    m_file_address = (char *)mmap(0, m_file_stat.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);
    return FILE__REQUEST;

    //////////////////////////////////////////////////////////////////
    // strncpy(m_real_file+len,m_url,FILENAME_LEN-len-1);
    // if(stat(m_real_file,&m_file_stat)<0){
    //     return NO__RESOURCE;
    // }
    // if(!(m_file_stat.st_mode&S_IROTH))
    // {
    //     return FORBIDDEN_REQUEST;
    // }
    // if(S_ISDIR(m_file_stat.st_mode))
    // {
    //     return BAD_REQUEST;
    // }

    // int fd=open(m_real_file,O_RDONLY);
    // m_file_address=(char*)mmap(0,m_file_stat.st_size,PROT_READ,MAP_PRIVATE,fd,0);
    // close(fd);
    // return FILE__REQUEST;
}


//对内存映射区执行munmap操作
void http_conn::unmap()
{
    if(m_file_address)
    {
        munmap(m_file_address,m_file_stat.st_size);
        m_file_address=0;
    }
}

//写HTTP响应
bool http_conn::write()
{
    int temp=0;
    // int bytes_have_send=0;
    // int bytes_to_send=m_write_idx;
    if(btyes_to_send==0)
    {
        modfd(m_epollfd,m_socketfd,EPOLLIN,m_TRIGMode);
        init();
        return true;
    }

    while (1)
    {
        temp=writev(m_socketfd,m_iv,m_iv_count);
        if(temp<0)
        {
            if(errno==EAGAIN)
            {
                modfd(m_epollfd,m_socketfd,EPOLLOUT,m_TRIGMode);
                return true;
            }
            unmap();
            return false;
        }
        btyes_have_send+=temp;
        btyes_to_send-=temp;
        if (btyes_have_send >= m_iv[0].iov_len)
        {
            m_iv[0].iov_len = 0;
            m_iv[1].iov_base = m_file_address + (btyes_have_send - m_write_idx);
            m_iv[1].iov_len = btyes_to_send;
        }
        else
        {
            m_iv[0].iov_base = m_write_buf + btyes_have_send;
            m_iv[0].iov_len = m_iv[0].iov_len - btyes_have_send;
        }

        if (btyes_to_send <= 0)
        {
            unmap();
            modfd(m_epollfd, m_socketfd, EPOLLIN, m_TRIGMode);

            if (m_linger)
            {
                init();
                return true;
            }
            else
            {
                return false;
            }
        }
        }
    }
    



//往写缓冲中写入待发数据
//下次继续的位置
bool http_conn::add_response(const char* format,...)
{
    if(m_write_idx>=WRITE_BUFFER_SIZE)
    {
        return false;
    }
    va_list arg_list;
    va_start(arg_list,format);
    //printf("mbuf:%s ,midx:%d    ,WS:%d  ,format:%s\n",m_write_buf,m_write_idx,WRITE_BUFFER_SIZE,format);
    int len=vsnprintf(m_write_buf+m_write_idx,WRITE_BUFFER_SIZE-1-m_write_idx,format,arg_list);
    if(len>=(WRITE_BUFFER_SIZE-1-m_write_idx))
    {
        va_end(arg_list);
        return false;
    }
    m_write_idx+=len;
    va_end(arg_list);
    
    LOG_INFO("request:%s",m_write_buf);
    return true;
}

bool http_conn::add_status_line(int status,const char* title)
{
    return add_response("%s %d %s\r\n","HTTP/1.1",status,title);
}

bool http_conn::add_headers(int content_length)
{
    add_content_length(content_length);
    add_linger();
    return add_blank_line();
}

bool http_conn::add_content_length(int content_lenght){
    return add_response("Content-Length:%d\r\n",content_lenght);
}

bool http_conn::add_content_type()
{
    return add_response("Content-Type:%s\r\n", "text/html");
}
bool http_conn::add_linger()
{
    return add_response("Connection: %s\r\n",(m_linger==true)?"keep-alive":"close");

}

bool http_conn::add_blank_line()
{
    return add_response("%s","\r\n");
}

bool http_conn::add_content(const char* content)
{
    return add_response("%s" ,content);
}

//根据请求处理结果，返回客户端的内容
bool http_conn::process_write(HTTP_CODE ret)
{
    switch (ret)
    {
    case  INTERNAL_ERROR:
        {
            add_status_line(500,error_500_title);
            //printf("errno: %s\n",error_500_from);
            add_headers(strlen(error_500_from));
            if(!add_content(error_500_from))
            {
                return false;
            }
            break;
        }
    case BAD_REQUEST:
    {
        add_status_line(400,error_400_title);
        //printf("errno: %s\n",error_400_form);
        add_headers(strlen(error_400_form));
        if(!add_content(error_400_form))
        {
            return false;
        }
        break;
    }
    case NO__RESOURCE:
    {
        add_status_line(404,error_404_title);
        //printf("errno: %s\n",error_404_from);
        add_headers(strlen(error_404_from));
        if(!add_content(error_404_from))
        {
            return false;
        }
        break;
    }
    case FORBIDDEN_REQUEST:
    {
        add_status_line(403,error_403_title);
        //printf("errno: %s\n",error_403_from);
        add_headers(strlen(error_403_from));
        if(!add_content(error_403_from))
        {
            return false;
        }
        break;
    }
    case FILE__REQUEST:
    {
        add_status_line(200,ok_200_title);
        //printf("errno: %s\n",ok_200_title);
        if(m_file_stat.st_size!=0)
        {
            add_headers(m_file_stat.st_size);
            m_iv[0].iov_base=m_write_buf;
            m_iv[0].iov_len=m_write_idx;
            m_iv[1].iov_base=m_file_address;
            m_iv[1].iov_len=m_file_stat.st_size;
            m_iv_count=2;
            return true;
        }
        else{
            const char* ok_string="<html><body></body></html>";
            add_headers(strlen(ok_string));
            if(!add_content(ok_string))
            {
                return false;
            }
        }
        break;
    }
    
    default:
    {
        break;
    }
    }
    m_iv[0].iov_base=m_write_buf;
    m_iv[0].iov_len=m_write_idx;
    m_iv_count=1;
    return true;
}

//由线程池中的工作线程调用，这是处理HTTP请求的入口
void http_conn::process()
{
    HTTP_CODE read_ret=process_read();
    //printf("process()\n");
    if(read_ret==NO_REQUEST)
    {
        modfd(m_epollfd,m_socketfd,EPOLLIN,m_TRIGMode);
        return;
    }
    //printf("process_write\n");
    bool write_ret=process_write(read_ret);
    
    if(!write_ret)
    {
        close_conn();
    }

    modfd(m_epollfd,m_socketfd,EPOLLOUT,m_TRIGMode);
}
