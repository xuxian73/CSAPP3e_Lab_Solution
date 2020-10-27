#include "csapp.h"
#define CACHESIZE 102400
#define CACHENUM 8

typedef struct {
    int readcnt;
    char buf[CACHESIZE];
    char url[MAXLINE];
    int time;
    int empty;
    sem_t writer;
    sem_t reader;
} cache_t;

void cache_init(cache_t* cp);

void cache_read(cache_t* cp);

void cache_write(cache_t* cp);