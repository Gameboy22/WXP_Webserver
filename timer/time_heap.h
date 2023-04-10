//时间堆定时器
#ifndef MIN_HEAP
#define MIN_HEAP
#include<iostream>
#include<netinet/in.h>
#include<time.h>

#define BUFFER_SIZE 64

class heap_timer;

//绑定socket和定时器
struct client_data
{
    /* data */
    sockaddr_in address;
    int sockfd;
    char buf[BUFFER_SIZE];
    heap_timer* timer;
};

class heap_timer
{
private:
    
public:
    heap_timer(int delay){
        expire=time(NULL)+delay;
    }
public:
    time_t expire;//定时器生效的绝对时间
    void (*cb_func)(client_data* );
    client_data* user_data;
};
//时间堆类
class time_heap
{
private:
    /* data */
    heap_timer** array;
    int capacity;
    int cur_size;
public:
    time_heap(int cap) :capacity(cap),cur_size(0) {
        array=new heap_timer* [capacity];
        if(!array){
            //throw std::exception();
            return ;
        }
        for (int i = 0; i < capacity; i++)
        {
            array[i]=NULL;
        }
        
    }
    time_heap(heap_timer** init_array,int size ,int cap):capacity(cap),cur_size(size){
        if(capacity<size){
            //throw std::exception();
            return;
        }
        array=new heap_timer* [capacity];
        if(!array){
            //throw std::exception();
            return;
        }
        for (int i = 0; i < capacity; i++)
        {
            array[i]=NULL;
        }
        if(size!=0){
            for (int i = 0; i < size; i++)
            {
                array[i]=init_array[i];
            }
            for(int i=(cur_size-1)/2;i>=0;--i){
                percolate_down(i);
            }
            
        }
     } 
    ~time_heap(){
        for (int i = 0; i < capacity; i++)
        {
            /* code */
            delete array[i];
        }
        delete[] array;
        
    }
public:
    void add_timer(heap_timer* timer) /*throw (std::exception)*/ 
    {
        if(!timer){
            return;
        }
        if(cur_size>=capacity){
            resize();
        }
        int hole=cur_size++;
        int parent;
        for(;hole>0;hole=parent){
            parent=(hole-1)/2;
            if(array[parent]->expire<=timer->expire){
                break;
            }
            array[hole]=array[parent];
        }
        array[hole]=timer;
    }
    void del_timer(heap_timer* timer){
        if(!timer){
            return;
        }
        timer->cb_func=NULL;
    }
    heap_timer* top() const{
        if(empty()){
            return NULL;
        }
        return array[0];
    }
     void pop_timer(){
        if(empty()){
            return;
        }
        delete array[0];
        array[0]=array[--cur_size];
        percolate_down(0);
     }

     void tick(){
        heap_timer* temp=array[0];
        time_t cur=time(NULL);
        while(!array){
            if(!temp){
                break;
            }
            if(temp->expire>cur){
                array[0]->cb_func(array[0]->user_data);
            }
            pop_timer();
            temp=array[0];
        }
     }
     bool empty() const { return cur_size==0;}
private:
	//最小堆的下滤操作，确保堆数组中的第hole个节点作为根的子树拥有最小堆性质
	void percolate_down(int hole){
        heap_timer* temp=array[hole];
        int child=0;
        for(;((hole*2+1)<=(cur_size-1));hole=child){
            child=hole*2+1;
            if((child<(cur_size-1))&&(array[child+1]->expire<array[child]->expire)){
                ++child;
            }
            if(array[child]->expire<temp->expire){
                array[hole]==array[child];
            }
            else{
                break;
            }
        }
        array[hole]=temp;
    }
	//将堆数组容量扩大一倍
    void resize() /*throw (std::exception)*/ {
        heap_timer** temp=new heap_timer* [2*capacity];
        for(int i=0;i<2*capacity;++i){
            temp[i]=NULL;
        }
        if(!temp){
            throw std::exception();
        }
        capacity=2*capacity;
        for(int i=0;i<cur_size;++i){
            temp[i]=array[i];
        }
        delete[] array;
        array=temp;
    }
};








#endif
