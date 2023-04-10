#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <stdarg.h>
#include "log.h"
#include <pthread.h>
using namespace std;

 Log::Log(){
    m_count=0;
    m_is_async=false;
}

Log::~Log(){
    if(m_fp !=NULL){
        fclose(m_fp);
    }

}

bool Log::init(const char *file_name, int log_buf_size, int split_lines, int max_queue_size)
{
    if(max_queue_size>=1)
    {
        m_is_async=true;
        m_log_queue=new log_queue<string>(max_queue_size);
        pthread_t tid;
        pthread_create(&tid,NULL,flush_log_thread,NULL);
    }

    m_log_buf_size=log_buf_size;
    m_buf=new char[m_log_buf_size];
    memset(m_buf,'\0',m_log_buf_size);
    m_split_lines=split_lines;

    time_t t=time(NULL);
    struct tm *sys_tm=localtime(&t);
    struct tm my_tm=*sys_tm;

    const char *p=strrchr(file_name,'/');
    char log_full_name[256]={0};

    if(p==NULL)
    {
        snprintf(log_full_name,255,"%d_%02d_%02d_%s",my_tm.tm_year+1900,my_tm.tm_mon+1,my_tm.tm_mday,file_name);
    }
    else{
        ///下次继续
    }
}