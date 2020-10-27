#include <stdio.h>
#include "csapp.h"
/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";
static const char *connect_hdr = "Connection: close\r\n";
static const char *proxy_hdr = "Proxy-Connection: close\r\n";

void parse_uri(char * uri, char* hostname, char* path, char* port);
void build_requesthdr(char* sever_header, char* path, char* hostname, char* port, rio_t* client_r);
void doit(int connfd){
    int serverfd;
    char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
    char server_header[MAXLINE];
    char hostname[MAXLINE], path[MAXLINE];
    char port[MAXLINE];

    rio_t client_r, server_r;
    Rio_readinitb(&client_r, connfd);
    Rio_readlineb(&client_r, buf, MAXLINE);
    sscanf(buf, "%s %s %s", method, uri, version);

    if (strcasecmp(method, "GET")) {
        printf("Proxy does not implement this method.");   
        return;
    }
    
    parse_uri(uri, hostname, path, port);
    build_requesthdr(server_header, path, hostname, port, &client_r);
    
    if ((serverfd = Open_clientfd(hostname, port)) < 0 ) {
        printf("connection failed\n");
        return;
    }
    Rio_readinitb(&server_r, serverfd);
    Rio_writen(serverfd, server_header, strlen(server_header));
    size_t n;
    while ((n = Rio_readlineb(&server_r, buf, MAXLINE)) != 0) {
        printf("proxy received %ld bytes then send\n", n);
        Rio_writen(connfd, buf, n);
    }
    Close(serverfd);
    return;
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

int main(int argc, char** argv)
{
    Signal(SIGPIPE, SIG_IGN);
    int listenfd, connfd;
    char hostname[MAXLINE], port[MAXLINE];
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;

    /* Check command-line args */
    if (argc != 2) {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(0);
    }

    listenfd = Open_listenfd(argv[1]);
    while(1) {
        clientlen = sizeof clientaddr;
        connfd = Accept(listenfd, (SA* )&clientaddr, &clientlen);
        Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);
        printf("Accept connection from (%s, %s)\n", hostname, port);
        doit(connfd);
        Close(connfd);
    }
    printf("%s", user_agent_hdr);
    return 0;
}
