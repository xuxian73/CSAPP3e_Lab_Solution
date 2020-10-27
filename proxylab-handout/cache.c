#include "cache.h"

void cache_init(cache_t* cp) {
    cp->empty = 1;
    cp->readcnt = 0;
    cp->time = 0;
    sem_init(&cp->writer, 0, 1);
    sem_init(&cp->reader, 0, 1);
}