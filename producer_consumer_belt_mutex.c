#include <pthread.h>
#include<stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <stdint.h>
#include <signal.h>

#define B 0
#define M 1
#define N_PROD 3
#define N_CONS 2
#define BUFFER_LEN 3

typedef struct{
    short int id;
    char quality;
    short int type;
    struct timespec time_create;
    struct timespec time_out;
}Product;

Product buffer[BUFFER_LEN];
int in = 0, out = 0, count = 0, global_id=1;

int total_produced = 0, total_consumed = 0, total_discarded = 0;
void* p_func(void *args);
void* c_func(void *args);
Product createProduct();
int insertProduct(Product p);
Product removeProduct();
void handle_sigint(int sig);

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t not_full = PTHREAD_COND_INITIALIZER;
pthread_cond_t not_empty = PTHREAD_COND_INITIALIZER;

int main(int argc, char* argv[]){
    signal(SIGINT, handle_sigint);

    pthread_t producer[N_PROD];
    pthread_t consumer[N_CONS];
    int res;
    void *thread_result;

    for(int i=0; i < N_PROD;i++) {
        res = pthread_create(&producer[i],NULL,p_func,(void*)(intptr_t)i);
        if(res!=0){
            perror("Error initializing a producer thread");
            exit(EXIT_FAILURE);
        }
    }

    for(int i=0; i < N_CONS;i++) {
        res = pthread_create(&consumer[i],NULL,c_func,(void*)(intptr_t)i);
        if(res!=0){
            perror("Error initializing a consumer thread");
            exit(EXIT_FAILURE);
        }
    }

    for(int i = 0; i < N_PROD;i++){
        res = pthread_join(producer[i],&thread_result);
        if(res!=0){
            perror("Error joining producer threads");
            exit(EXIT_FAILURE);
        }
    } 

    for(int i = 0; i < N_CONS;i++) {
        res = pthread_join(consumer[i],&thread_result);
        if(res!=0){
            perror("Error joining consumer threads");
            exit(EXIT_FAILURE);
        }
    }

    return 0;
}

Product createProduct(){
    Product p;

    pthread_mutex_lock(&mutex);
    p.id = global_id;
    global_id++;
    pthread_mutex_unlock(&mutex);
    if(rand() % 2 == 0)p.quality = B;
    else p.quality = M;
    return p;
}

int insertProduct(Product p){
    pthread_mutex_lock(&mutex);
    for(int i = 0; i < BUFFER_LEN;i++){
        if(buffer[i].id == 0){
            buffer[i] = p;
            count++;
            pthread_mutex_unlock(&mutex);
            return 1;
        }
    }
    total_discarded++;
    pthread_mutex_unlock(&mutex);
    return 0;
}

Product removeProduct(){
    pthread_mutex_lock(&mutex);
    for(int i =0;i< BUFFER_LEN;i++){
        if(buffer[i].id != 0){
            Product p = buffer[i];
            buffer[i].id = 0;
            count --;
            total_consumed++;
            pthread_mutex_unlock(&mutex);
            return p;
        }
    }
    pthread_mutex_unlock(&mutex);
    return (Product){0, 0, 0, {0, 0}, {0, 0}};
}

void* p_func(void *args){
    int id = (int)(intptr_t)args;
    while(1){
        Product p = createProduct();
        total_produced++; 
        int res = insertProduct(p);
        if(res == 1)
        printf("Producer[%d] produced product[%d] (%s), buffer count =%d\n",id, p.id, p.quality == B ? "B" : "M", count);
        else printf("Product %d discarded (buffer full)\n", p.id);
        sleep(rand() % 3 + 1);
    }
}

void* c_func(void *args){
    int id = (int)(intptr_t)args;
    while(1) {
        Product p = removeProduct();
        
        if(p.id != 0) {
            printf("Consumer[%d] consumed product[%d] (%s), buffer count=%d\n",id, p.id, p.quality == B ? "B" : "M", count);
            sleep(rand() % 2 + 1);
        }
    }

    return NULL;
}

void handle_sigint(int sig){
    printf("\n\n=== STATISTICS ===\n");
    printf("Total produced: %d\n", total_produced);
    printf("Total consumed: %d\n", total_consumed);
    printf("Total in-process: %d\n",total_produced - (total_consumed + total_discarded));
    printf("Total discarded: %d\n", total_discarded);
    exit(0);
}