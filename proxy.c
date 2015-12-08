#include <stdio.h>
#include "csapp.h"

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

typedef struct request_info {
    char *hostname;
    char *method;
    int port;
    char *uri;
} request_info;

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";

//volatile file?
void doit(int fd);
int parse_uri(char *uri, char *filename, char *cgiargs);
void clienterror(int fd, char *cause, char *errnum,
char *shortmsg, char *longmsg);
void read_requesthdrs(rio_t *rp);
int is_valid(int fd, char *method, char *url, char *version, struct request_info *r_info);
void handle_connection(int fd, struct request_info *r_info);
#define debugprintf  printf
//void debugprintf();
#define GOTCHA printf("GOTCHA!\n");

int main(int argc, char **argv)
{
    /* variables */
    int listenfd, connfd;
    char hostname[MAXLINE], port[MAXLINE];
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;

    /* Check command line args */
    if (argc != 2) {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
    	exit(1);
    }

// client length?
    listenfd = Open_listenfd(argv[1]);
    while(1) {
        clientlen = sizeof(clientaddr)  ;
        connfd = Accept(listenfd, (SA *) &clientaddr, &clientlen);
        Getnameinfo((SA *) &clientaddr, clientlen, hostname,
        MAXLINE, port, MAXLINE, 0);
        printf("Accepted connection: (%s, %s)\n", hostname, port);
        doit(connfd);
        Close(connfd);
    }

//    printf("%s", user_agent_hdr);
}

void doit(int fd) {
    size_t n;
    char buf[MAXLINE], method[MAXLINE], url[MAXLINE], version[MAXLINE];
    char read_buf;
    rio_t rio;
    request_info r_info;

/* Accept each request */
    Rio_readinitb(&rio, fd);
    /*Read one line*/
    if (!Rio_readlineb(&rio, buf, MAXLINE))
        return;

    printf("%s", buf);
    read_requesthdrs(&rio);
    if (sscanf(buf, "%s %s %s", method, url, version) != 3) {
        printf("%s\n", "Not 3 request");
        return;
    }

    if (!is_valid(fd, method, url, version, &r_info)) {
        printf("%s\n", "invalid request");
        return;
    }
    else {
        printf("%s\n", "valid reqeust!");
    }

    /* suppose valid here */
    /* make request */
    handle_connection(fd, &r_info);

    /* make response */
}

void handle_connection(int connfd, struct request_info *r_info) {

    /* make request */
    int clientfd;
    char *host, *uri, *port, buf[MAXLINE];
    rio_t rio;
    port = "";

    debugprintf("start connection\n");
    host = r_info->hostname;
    printf("%s\n", (char *)host);
    uri = r_info->uri;
    sprintf(port, "%d", r_info->port);
    GOTCHA;

    /* Init client fd towards server */
    debugprintf("start clientfd\n");

    clientfd = Open_clientfd(host, port);
    Rio_readinitb(&rio, clientfd);

    /* Initiate header info */
    sprintf(buf, "%s %s %s\r\n", "GET", uri, "HTTP/1.0");
    sprintf(buf, "host: %s\r\n\r\n", host);
    Rio_writen(clientfd, buf, strlen(buf));
    //Rio_readlineb(&rio, buf, MAXLINE);

    printf("start read\n");
    while (strcmp(buf, "\r\n")) {
        Rio_readlineb(&rio, buf, MAXLINE);
        printf("%s", buf);
    }
    return;




}


int is_valid(int fd, char *method, char *url, char *version, struct request_info *r_info) {
    char hostname[100];
    int port = 80;
    char uri[200];
    int valid_flag = 0;
    printf("%s\n", url);

    if (strcasecmp(method, "GET")) {
        clienterror(fd, method, "501", "Not Supported method"
        , "Proxy does not support this method");
        printf("invalid request: Method\n");
        return 0;
    }
    if (strcasecmp(version, "HTTP/1.0") && strcasecmp(version, "HTTP/1.1")) {
        clienterror(fd, version, "501", "Not Supported version",
        "Proxy only support HTTP/1.0 or 1.1 version");
        printf("invalid request: Version\n");
        return 0;
    }


    if (sscanf(url, "http://%99[^:]:%99d/%199[^/n]", hostname, &port, uri) == 3)
    {
        valid_flag = 1;
    }
    else if (sscanf(url, "http://%99[^/]/%199[^/n]", hostname, uri) == 2) {
        valid_flag = 1;
    }

    if (valid_flag) {
        printf("hostname: %s\n method: %s \n verision: %s\n", hostname, method, version);
        r_info->port = port;
        printf("%d\n", port);
        r_info->hostname = hostname;
        printf("%s\n", (char *)(r_info->hostname));
        r_info->uri = uri;
        return 1;
    }
    else {
        printf("%s\n", hostname);
        printf("%s\n", uri);
        printf("%d\n", port);
        clienterror(fd, hostname, "400","Bad request", "Please check if the url is valid");
        printf("%s\n", "invalid request: URL");
        return 0;
    }


}
void make_request(int fd, char *read_buf)
{

}

void make_response(int fd, char *content_buf) {

}

void clienterror(int fd, char *cause, char *errnum,
		 char *shortmsg, char *longmsg)
{
    char buf[MAXLINE], body[MAXBUF];
    Rio_writen(fd, buf, strlen(buf));    Rio_writen(fd, buf, strlen(buf));
    /* Build the HTTP response body */
    sprintf(body, "<html><title>Tiny Error</title>");
    sprintf(body, "%s<body bgcolor=""ffffff"">\r\n", body);
    sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
    sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
    sprintf(body, "%s<hr><em>The Tiny Web server</em>\r\n", body);

    /* Print the HTTP response */
    sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-type: text/html\r\n");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
    Rio_writen(fd, buf, strlen(buf));
    Rio_writen(fd, body, strlen(body));
}
/* $end clienterror */

/*
 * read_requesthdrs - read HTTP request headers
 */
/* $begin read_requesthdrs */
void read_requesthdrs(rio_t *rp)
{
    char buf[MAXLINE];

    Rio_readlineb(rp, buf, MAXLINE);
    printf("%s", buf);
    while(strcmp(buf, "\r\n")) {          //line:netp:readhdrs:checkterm
    	Rio_readlineb(rp, buf, MAXLINE);
    	printf("%s", buf);
    }
    return;
}
/* $end read_requesthdrs */

//void debugprintf(){}
