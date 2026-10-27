#include <stdio.h>
#include <stdbool.h>
#include <stdatomic.h>
#include <pthread.h>
typedef enum{
 INIT=1,
 ACTIVE,
 CLOSED
}conditions;

atomic_int condition=INIT;
atomic_int wholetimes=0;

int judge()
{
    int expected=INIT;
    if(atomic_compare_exchange_strong_explicit(&condition,&expected,ACTIVE,memory_order_seq_cst,memory_order_seq_cst))
    {
        return 1;
    }
    return 0;
}
void* worker(void* arg)
{
  if(judge())
  {
    atomic_fetch_add(&wholetimes,1);
  }
  return NULL;
}

int main(void)
{
  pthread_t tid[10];
  for(int i=0;i<10;++i)
  {
    pthread_create(&tid[i],NULL,worker,NULL);
  }
  for(int i=0;i<10;++i)
  {
    pthread_join(tid[i],NULL);
  }
  printf("the whole time is %d\n",atomic_load(&wholetimes));
  return 0;
}