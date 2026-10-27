#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
typedef struct ntask ntask;
typedef struct nworker nworker;
typedef struct workmanager workmanager;

int threadpool_create(workmanager* pool,int workernum);
void* threadpool_doing(void* arg);
int threadpool_addtask(workmanager* pool,void(*func)(void*),void* arg);
int threadpool_distory(workmanager *pool);







struct ntask
{
struct ntask* prev;
struct ntask* next;

void (*func)(void*);
void* arg;
};//任务队列

struct nworker
{
pthread_t workid;
workmanager* pool;

struct nworker* prev;
struct nworker* next;
};//线程

struct workmanager
{
struct ntask *taskhead;
struct nworker* workerhead;
int isterminated;

pthread_mutex_t mutex; //任务队列的锁
pthread_cond_t condition;//没任务时候让线程休眠
};

void* threadpool_doing(void* arg)//arg是worker
{
    nworker* worker=(nworker*) arg;
    workmanager *pool=worker->pool;
    while(1)
    {
        pthread_mutex_lock(&pool->mutex);


        //如果没有任务且没有终止   挂起 阻塞等待
        while(pool->taskhead==NULL&&pool->isterminated!=1)
        {
            pthread_cond_wait(&pool->condition,&pool->mutex);
        }
        //如果没有任务且收到了终止信号
        if(pool->taskhead==NULL&&pool->isterminated==1)
        {
            pthread_mutex_unlock(&pool->mutex);
            break;
        }
        //以上两都不是  可以去拿任务了
        ntask* task=pool->taskhead;
        pool->taskhead=task->next;
        if(pool->taskhead!=NULL)
        {
            pool->taskhead->prev=NULL;
        }
        task->next=NULL;task->prev=NULL;

        pthread_mutex_unlock(&pool->mutex);
        task->func(task->arg);
        free(task);
    }
    return NULL;    //回收全部交给主线程的distory函数，统一处理malloc和悬空指针
}

int threadpool_create(workmanager* pool,int workernum)
{
  if(pool==NULL)
  {
    return -1;
  }
  if(workernum<1)
  {
    workernum=1;
  }
  memset(pool, 0, sizeof(workmanager));
  pthread_mutex_init(&pool->mutex,NULL);
  pthread_cond_init(&pool->condition,NULL);
  pool->isterminated=0;
  pool->taskhead=NULL;

  for(int i=0;i<workernum;++i)
  {
    nworker* worker=malloc(sizeof(nworker));
    if(worker==NULL)
    {
        fprintf(stderr,"malloc error happened in pool create\n");
        return -2;
    }
    memset(worker,0,sizeof(nworker));
    worker->pool=pool;
    int ret=pthread_create(&worker->workid,NULL,threadpool_doing,worker);
    if (ret != 0)
    {
            perror("pthread_create");
            free(worker);
            return -3;
    }
   
    // 插入队列 
    if(pool->workerhead==NULL)
    {
        pool->workerhead=worker;
    }
    else
    {
        nworker* lastworker=pool->workerhead;
        while(lastworker->next!=NULL)
        {
            lastworker=lastworker->next;
        }
        lastworker->next=worker;
        worker->prev=lastworker;
        worker->next=NULL;
    }
  }
  return 0;
}

int threadpool_addtask(workmanager* pool,void(*funct)(void*),void* argt)
{
    if(pool==NULL||funct==NULL)
    {
        return -1;
    }
    ntask* newtask=(ntask*) malloc(sizeof(ntask));
    if(newtask==NULL)
    {
        fprintf(stderr,"malloc error happened in add task\n");
        return -2;
    }
    memset(newtask,0,sizeof(ntask));
    newtask->arg=argt;
    newtask->func=funct;

    pthread_mutex_lock(&pool->mutex);

    ntask *tailfinder=pool->taskhead;
    if(tailfinder==NULL)
    {
        pool->taskhead=newtask;
        newtask->prev=NULL;
        newtask->next=NULL;
    }
    else
    {
        while(tailfinder->next!=NULL)
        {
            tailfinder=tailfinder->next;
        }
        tailfinder->next=newtask;
        newtask->prev=tailfinder;
        newtask->next=NULL;
    }


    pthread_cond_signal(&pool->condition);
    pthread_mutex_unlock(&pool->mutex);

    return 0;
}

int threadpool_distory(workmanager *pool)
{
    if(pool==NULL)
    {
        return -1;
    }

    pthread_mutex_lock(&pool->mutex);
    
    pool->isterminated=1;
    pthread_cond_broadcast(&pool->condition);

    pthread_mutex_unlock(&pool->mutex);

    nworker* myworker=pool->workerhead;
    while(myworker!=NULL)
    {
        nworker *nexworker=myworker->next;
        pthread_join(myworker->workid,NULL);//阻塞等待线程执行完毕再回收
        free(myworker);
        myworker=nexworker;
    }
    pool->workerhead=NULL;

    ntask* mytask=pool->taskhead;
    while(mytask!=NULL)
    {
        ntask* nextask=mytask->next;
        free(mytask);
        mytask=nextask;
    }
    pool->taskhead=NULL;

    pthread_mutex_destroy(&pool->mutex);
    pthread_cond_destroy(&pool->condition);

    return 0;
}



// 测试用的任务函数
void test_task(void* arg) {
    int* num = (int*)arg;
    
    // 获取当前线程 ID，验证是不是 3 个不同的线程在干活
    pthread_t tid = pthread_self();
    
    printf("线程 [%lu] 开始执行任务: %d\n", (unsigned long)tid, *num);
    
    // 模拟耗时操作（100毫秒），让其他线程有机会抢到锁去执行任务
    usleep(100000); 
    
    printf("线程 [%lu] 完成执行任务: %d\n", (unsigned long)tid, *num);
    
    // ⭐ 谁申请谁释放：这里释放我们在 main 里 malloc 的整数参数
    free(num); 
}



int main()
{
    // 1. 在栈上声明线程池管理者（注意：不是指针）
    workmanager pool;
    
    // 2. 初始化线程池，开启 3 个工作线程
    int ret = threadpool_create(&pool, 3);
    if (ret != 0) {
        fprintf(stderr, "线程池创建失败，错误码: %d\n", ret);
        return -1;
    }
    printf("=== 线程池初始化成功，共 3 个工作线程 ===\n\n");

    // 3. 模拟生产者：投递 10 个任务
    for (int i = 1; i <= 10; i++) {
        // 为参数分配堆内存
        int* arg = (int*)malloc(sizeof(int));
        if (arg == NULL) {
            perror("malloc arg failed");
            continue;
        }
        *arg = i; // 任务编号 1~10

        // 将任务加入线程池
        ret = threadpool_addtask(&pool, test_task, arg);
        if (ret != 0) {
            fprintf(stderr, "任务 %d 添加失败\n", i);
            free(arg); // 添加失败要自己释放参数，否则内存泄漏
        }
    }
    printf("\n=== 主线程已投递 10 个任务，等待工作线程处理 ===\n\n");

    // 4. 主线程休眠 2 秒，等待所有任务被执行完毕
    // （真实生产环境通常用其他同步机制，这里为了演示简单直接 sleep）
    sleep(2);

    // 5. 销毁线程池（唤醒所有工作线程退出，回收内存，销毁锁和条件变量）
    threadpool_distory(&pool);
    printf("\n=== 线程池已销毁，程序正常退出 ===\n");

    return 0;
}