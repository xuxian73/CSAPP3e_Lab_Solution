#include <stdio.h>
#include "csapp.h"
#include "sbuf.h"
#include "cache.h"
/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400
#define NTHREADS 4
#define SBUFSIZE 16

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";
static const char *connect_hdr = "Connection: close\r\n";
static const char *proxy_hdr = "Proxy-Connection: close\r\n";

/* sbuf for threads */
sbuf_t sbuf;

/* cache */
cache_t cache[CACHENUM];

/* time for LRU */
int turn = 0;
sem_t timemutex;

void parse_uri(char * uri, char* hostname, char* path, char* port);
void build_requesthdr(char* sever_header, char* path, char* hostname, char* port, rio_t* client_r);
void* thread(void* arg);
void doit(int connfd);
void read_cache(int i, int connfd, rio_t* rp);
void write_cache(int i, char* uri, char* cachebuf);
int cache_evic();
inline void pre_read(int i);
inline void pro_read(int i);

void pre_read(int i) {
    P(&cache[i].reader);
    ++cache[i].readcnt;
    if(cache[i].readcnt == 1) 
        P(&cache[i].writer);
    V(&cache[i].reader);
}

void pro_read(int i) {
    P(&cache[i].reader);
    --cache[i].readcnt;
    if(cache[i].readcnt == 0)
        V(&cache[i].writer);
    V(&cache[i].reader);
}

void update_time(int i) {
    P(&cache[i].writer);
    cache[i].time = turn;
    V(&cache[i].writer);
}

int main(int argc, char** argv)
{
    Signal(SIGPIPE, SIG_IGN);
    sem_init(&timemutex, 0, 1);
    int listenfd, connfd;
    char hostname[MAXLINE], port[MAXLINE];
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;
    pthread_t tid;
    /* Check command-line args */
    if (argc != 2) {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(0);
    }

    listenfd = Open_listenfd(argv[1]);

    sbuf_init(&sbuf, SBUFSIZE);
    for (int i = 0 ; i < CACHENUM; ++i) {
        cache_init(&cache[i]);
    }
    for (int i = 0; i < NTHREADS; ++i) {
        Pthread_create(&tid, NULL, thread, NULL);
    }

    while(1) {
        clientlen = sizeof clientaddr;
        connfd = Accept(listenfd, (SA* )&clientaddr, &clientlen);
        Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);
        printf("Accept connection from (%s, %s)\n", hostname, port);
        sbuf_insert(&sbuf, connfd);
    }
    printf("%s", user_agent_hdr);
    return 0;
}

void *thread(void* arg) {
    Pthread_detach(Pthread_self());
    while(1) {
        int connfd = sbuf_remove(&sbuf);
        doit(connfd);
        Close(connfd);
    }
}

void doit(int connfd){
    int serverfd;
    char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
    char server_header[MAXLINE];
    char hostname[MAXLINE], path[MAXLINE];
    char port[MAXLINE];

    rio_t client_r, server_r;
    P(&timemutex);
    ++turn;
    V(&timemutex);
    Rio_readinitb(&client_r, connfd);
    Rio_readlineb(&client_r, buf, MAXLINE);
    sscanf(buf, "%s %s %s", method, uri, version);

    if (strcasecmp(method, "GET")) {
        printf("Proxy does not implement this method.");   
        return;
    }

    char tmp_uri[MAXLINE];
    strcpy (tmp_uri, uri);
    parse_uri(tmp_uri, hostname, path, port);
    build_requesthdr(server_header, path, hostname, port, &client_r);

    int cachenum = -1;
    for (int i = 0; i < CACHENUM; ++i) {
        pre_read(i);
        if (!cache[i].empty && strcmp(uri, cache[i].url) == 0) {
            cachenum = i;
            pro_read(i);
            break;
        }
        pro_read(i);
    }
    if (cachenum >= 0) {
        read_cache(cachenum, connfd, &client_r);
        update_time(cachenum);
        return;
    }
    
    if ((serverfd = Open_clientfd(hostname, port)) < 0 ) {
        printf("connection failed\n");
        return;
    }
    Rio_readinitb(&server_r, serverfd);
    Rio_writen(serverfd, server_header, strlen(server_header));
    size_t n;
    size_t total = 0;
    char cachebuf[MAX_OBJECT_SIZE];
    while ((n = Rio_readlineb(&server_r, buf, MAXLINE)) != 0) {
        printf("proxy received %ld bytes\n", n);
        total += n;
        if (total < MAX_OBJECT_SIZE) strcat(cachebuf, buf);
        Rio_writen(connfd, buf, n);
    }
    if (total < MAX_OBJECT_SIZE) {
        cachenum = cache_evic();
        write_cache(cachenum, uri, cachebuf);
    }
    Close(serverfd);
    return;
}

void read_cache(int cachenum, int connfd, rio_t* client_r){
    pre_read(cachenum);
    size_t n = strlen(cache[cachenum].buf);
    Rio_writen(connfd, cache[cachenum].buf, strlen(cache[cachenum].buf));
    printf("proxy received %ld bytes from cache %d\n", n, cachenum);
    pro_read(cachenum);
}

void write_cache(int cachenum, char* uri, char * cachebuf) {
    P(&cache[cachenum].writer);
    cache[cachenum].time = turn;
    cache[cachenum].empty = 0;
    printf("proxy write %ld bytes to cache[%d]\n", strlen(cachebuf), cachenum);
    strcpy(cache[cachenum].buf, cachebuf);
    strcpy(cache[cachenum].url, uri);
    V(&cache[cachenum].writer);
}

int cache_evic(){
    int min = 0xefffffff;
    int cachenum;
    for (int i = 0; i < CACHENUM; ++i) {
        pre_read(i);
        if (cache[i].empty) {
            cachenum = i;
            pro_read(i);
            break;
        } else {
            if(cache[i].time < min) {
                min = cache[i].time;
                cachenum = i;
            }
        }
        pro_read(i);
    }
    return cachenum;
}
void build_requesthdr(char* server_header, char* path, char* hostname, char* port, rio_t* rp) {
    char buf[MAXLINE], request_hdr[MAXLINE], host_hdr[MAXLINE];
    host_hdr[0] = '\0';
    sprintf(request_hdr, "GET %s HTTP/1.0\r\n", path);
    while (Rio_readlineb(rp, buf, MAXLINE) > 0) {
        if (strcmp(buf, "\r\n") == 0) break;
        if (strstr(buf, "Host: ")) {
            strcpy(host_hdr, buf);
            continue;
        }
    }
    if (strlen(host_hdr) == 0)
        sprintf(host_hdr, "Host: %s\r\n", hostname);
    sprintf(server_header, "%s%s%s%s%s%s", request_hdr, host_hdr, connect_hdr, proxy_hdr, user_agent_hdr, "\r\n");
    return;
}


void read_reqesthdrs(rio_t *rp) {
    char buf[MAXLINE];

    Rio_readlineb(rp, buf, MAXLINE);
    while(strcmp(buf, "\r\n")) {
        Rio_readlineb(rp, buf, MAXLINE);
        printf("%s", buf);
    }
    return;
}

void parse_uri(char *uri, char *hostname, char* path, char* port) {
    port[0] = '8'; port[1] = '0'; port[2] = '\0';
    char *pos = strstr(uri, "//");
    pos = pos ? pos + 2 : uri;
    char *p = strstr(pos, ":");
    if (p) {
        char *pp = strstr(p, "/");
        *p = '\0';
        sscanf(pos, "%s", hostname);
        if (pp) {
            *pp = '\0';
            sscanf(p+1, "%s", port);
            *pp = '/';
            sscanf(pp, "%s", path);
        } else {
            sscanf(p + 1, "%s", port);
        }
    } else {
        p = strstr(pos, "/");
        if(p) {
            *p = '\0';
            sscanf(pos, "%s", hostname);
            *p = '/';
            sscanf(p, "%s", path);
        }
        else {
            sscanf(pos, "%s", hostname);
        }
    }
    return;
}

