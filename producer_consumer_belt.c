//Arquivo principal, a definição do trabalho

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

typedef enum {
    CONSOLE_RETRO = 0,
    CONSOLE_MODERN = 1,
    CONSOLE_PORTABLE = 2
} ConsoleType;
const char* console_names[] = {"Atari", "PS5", "Nintendo Switch"};

Product buffer[BUFFER_LEN];
int in = 0, out = 0, count = 0, global_id=1;
int total_produced = 0, total_consumed = 0, total_discarded = 0,total_B =0,total_M=0;
double time_on_belt = 0.0;

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
    p.type = rand() % 3;
    clock_gettime(CLOCK_REALTIME, &p.time_create);
    return p;
}

int insertProduct(Product p){
    for(int i = 0; i < BUFFER_LEN;i++){
        if(buffer[i].id == 0){
            buffer[i] = p;
            count++;
            return 1;
        }
    }
    total_discarded++;
    return 0;
}

Product removeProduct(){
    for(int i =0;i< BUFFER_LEN;i++){
        if(buffer[i].id != 0){
            Product p = buffer[i];
            buffer[i].id = 0;
            count --;
            total_consumed++;
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

        pthread_mutex_lock(&mutex);
        while(count == BUFFER_LEN){
            pthread_cond_wait(&not_full, &mutex);
        }
        total_produced++; 
        int res = insertProduct(p);
        if(res == 1)
        printf("Producer[%d] produced product[%d] (%s) (%s), buffer count=%d, created at %ld.%09ld\n",id, p.id, p.quality == B ? "B" : "M", console_names[p.type], count,p.time_create.tv_sec, p.time_create.tv_nsec);
        else printf("Product %d discarded (buffer full)\n", p.id);
        pthread_cond_signal(&not_empty);
        pthread_mutex_unlock(&mutex);

        sleep(rand() % 4 + 2);
    }
}

void* c_func(void *args){
    int id = (int)(intptr_t)args;
    while(1) {
        pthread_mutex_lock(&mutex);
        while(count == 0){
            pthread_cond_wait(&not_empty, &mutex);
        }
        Product p = removeProduct();
        clock_gettime(CLOCK_REALTIME, &p.time_out);
        double diff_sec = (p.time_out.tv_sec - p.time_create.tv_sec) 
                        + (p.time_out.tv_nsec - p.time_create.tv_nsec) / 1e9;
        time_on_belt += diff_sec;
        if(p.quality == B) total_B++;
        else if(p.quality == M) total_M++;

        pthread_cond_signal(&not_full);
        pthread_mutex_unlock(&mutex);

        if(p.id != 0) {
            printf("Consumer[%d] consumed product[%d] (%s) (%s), buffer count=%d, "
                   "created at %ld.%09ld, consumed at %ld.%09ld\n",id, p.id, p.quality == B ? "B" : "M", console_names[p.type], count,p.time_create.tv_sec, p.time_create.tv_nsec,p.time_out.tv_sec, p.time_out.tv_nsec);
                }
        sleep(rand() % 2 + 1);
    }

    return NULL;
}

void handle_sigint(int sig){
    printf("\n\n=== STATISTICS ===\n");
    printf("Total produced: %d\n", total_produced);
    printf("Total consumed: %d\n", total_consumed);
    printf("Total in-process: %d\n",total_produced - (total_consumed + total_discarded));
    double avg_time = total_consumed ? time_on_belt / total_consumed : 0;
    printf("Average time on belt: %.6f s\n", avg_time);
    printf("Total good quality: %d\n",total_B);
    printf("Total bad quality: %d\n",total_M);
    printf("Total discarded: %d\n", total_discarded);
    exit(0);
}