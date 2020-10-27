#include "cachelab.h"
#include <getopt.h>
#include <stdio.h>
#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>
int s = 0, E = 0, b = 0;
bool v = false;
char fname[100] = "";
int miss_count = 0, evi_count = 0, hit_count = 0;
int time = 0;
void usage(char* argv[]){
    printf("./csim-ref: Missing required command line argument\n");
    printf("Usage: %s [-hv] -s <num> -E <num> -b <num> -t <file>\n", argv[0]);
    printf("Options:\n");
    printf("-h          Print this help message.\n");
    printf("-v          Optional verbose flag\n.");
    printf("-s <num>    Number of set index bits.\n");
    printf("-E <num>    Number of lines per set.\n");
    printf("-b <num>    Number of block offset bits.\n");
    printf("-t <file>   Trace file\n");
    printf("Examples:\n");
    printf("linux>  %s -s 4 -E 1 -b 4 -t traces/yi.trace\n", argv[0]);
    printf("linux>  %s -v -s 8 -E 2 -b 4 -t traces/yi.trace\n", argv[0]);
}

typedef struct{
    int timestamp;
    long unsigned int tag;
    int valid;
} Line;

typedef Line* Set;

typedef Set* Cache;

Cache cache;

int main(int argc, char* argv[])
{
    // get opt
    int opt;
    while ((opt = getopt(argc,argv,"hvs:E:b:t:")) != -1) {
        switch(opt) {
        case 's':
            s = atoi(optarg);
            break;
        case 'E':
            E = atoi(optarg);
            break;
        case 'b':
            b = atoi(optarg);
            break;
        case 'v':
            v = true;
            break;
        case 't':
            strcpy(fname, optarg);
            break;
        case 'h':
            usage(argv);
            exit(0);
        default:
            usage(argv);
            exit(1);
        }
    }
    //open the file
    FILE* fptr;
    fptr = fopen(fname, "r");
    
    // alloc the cache
    cache = (Set*)malloc((1<<s) * sizeof(Set));
    for (int i = 0; i < (1<<s); ++i) {
        cache[i] = (Line*)malloc(E * sizeof(Line));
        memset((void*)cache[i], 0, E * sizeof(Line));
    }

    typedef long unsigned int lu;
    char type;
    long unsigned int address;
    int size;
    
    while(fscanf(fptr, "%c %lx,%d\n", &type, &address, &size) != EOF) {
        ++time;
        if (type == 'I' || type == ' ') continue;
        lu set = (address >> b) % (1 << s);
        lu tag = address >> b;
        bool hit = false, miss = false;
        Line* curSet = cache[set];
        //solve hit
        for (int i = 0; i < E; ++i) {
            if (curSet[i].valid && curSet[i].tag == tag){
                hit = true;
                curSet[i].timestamp = time;
            }
        }
        if (hit) {
            if (type == 'M') ++hit_count;
            if (v){
                if (type == 'M') {
                    printf("%c %lx,%d hit hit\n", type, address, size);
                    ++hit_count;
                    continue;
                }
                printf("%c %lx,%d hit\n", type, address, size);
            }
            ++hit_count;
            continue;
        }
        //solve miss
        ++miss_count;
        for (int i = 0; i < E; ++i) {
            if(!curSet[i].valid) {
                curSet[i].valid = true;
                curSet[i].timestamp = time;
                curSet[i].tag = tag;
                miss = true;
                break;
            }
        }
        if (miss) {
            if (type == 'M') ++hit_count;
            if (v) {
                if(type == 'M') {
                    printf("%c %lx,%d miss hit\n", type, address, size);
                    continue;
                }
                printf("%c %lx,%d miss\n", type, address, size);
            }
            continue;
        }

        //solve eviction
        ++evi_count;
        int min_time = INT_MAX;
        int evict_line;
        for (int i = 0; i < E; ++i){
            if (min_time > curSet[i].timestamp) {
                evict_line = i;
                min_time = curSet[i].timestamp;
            }
        }
        curSet[evict_line].tag = tag;
        curSet[evict_line].timestamp = time;
        if (type == 'M') ++hit_count;
        if (v){
            if(type == 'M') {
                printf("%c %lx,%d miss eviction hit\n", type, address, size);
                continue;
            }
            printf("%c %lx,%d miss eviction\n", type, address, size);
        }
    }

    //free cache
    for (int i = 0; i < (1<<s); ++i) {
        free(cache[i]);
    }
    free(cache);

    //close file
    fclose(fptr);
    printSummary(hit_count, miss_count, evi_count);
    
    return 0;
}
