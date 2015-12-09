#include <stdio.h>
#include "csapp.h"

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

typedef struct request_info {
    char *hostname;
    char *method;
    char *port;
    char *uri;
} request_info;

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";

//volatile file?
void *doit(void* connfd_ptr);
int parse_uri(char *uri, char *filename, char *cgiargs);
void clienterror(int fd, char *cause, char *errnum,
char *shortmsg, char *longmsg);
void read_requesthdrs(rio_t *rp, char *header_buf);
int is_valid(int fd, char *method, char *url, char *version, struct request_info *r_info);
void handle_connection(int fd, struct request_info *r_info, char *header_buf);
#define debugprintf  printf
//void debugprintf();
#define GOTCHA printf("GOTCHA!\n");

int main(int argc, char **argv)
{
    /* variables */
    int listenfd, connfd;
    int *connfd_ptr;
    pthread_t tid;
    char hostname[MAXLINE], port[MAXLINE];
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;

    /* Check command line args */
    if (argc != 2) {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
    	exit(1);
    }
    Signal(SIGPIPE, SIG_IGN);

    /* client length?*/
    listenfd = Open_listenfd(argv[1]);
    while(1) {
        clientlen = sizeof(clientaddr)  ;
        connfd = Accept(listenfd, (SA *) &clientaddr, &clientlen);

        /* parse arguments */
        connfd_ptr = malloc(sizeof(void *)); 
        *connfd_ptr = connfd;
        Pthread_create(&tid, NULL, doit, connfd_ptr);
        //Getnameinfo((SA *) &clientaddr, clientlen, hostname,MAXLINE, port, MAXLINE, 0);
        //printf("Accepted connection: (%s, %s)\n", hostname, port);
        //doit(connfd);
        //Close(connfd);
    }

/*    printf("%s", user_agent_hdr);*/
}

void *doit(void *connfd_ptr) {

    int fd = *(int *) connfd_ptr;
    Pthread_detach(Pthread_self());
    debugprintf("\n---------New thread run\n");
    Free(connfd_ptr);

    //pthread_rwlock_init

    
    size_t n;
    char buf[MAXLINE], method[MAXLINE], url[MAXLINE], version[MAXLINE];
    char header_buf[MAXLINE];
    rio_t rio;
    request_info* r_info;

    r_info = malloc(sizeof(request_info));

    /* Accept each request */
    Rio_readinitb(&rio, fd);
    /*Read one line*/
    if (!Rio_readlineb(&rio, buf, MAXLINE))
        return NULL;

    printf("%s", buf);

    /* Read remained request header */
    read_requesthdrs(&rio, header_buf);

    if (sscanf(buf, "%s %s %s", method, url, version) != 3) {
        printf("%s\n", "Not 3 request");
        return NULL;
    }

    if (!is_valid(fd, method, url, version, r_info)) {
        printf("%s\n", "invalid request");
        clienterror(fd, version, "404", "Not Supported or invalid request",
        "Proxy only support HTTP/1.0 or 1.1 version HTTP");
        printf("invalid request: \n");
        Free(r_info);
        Close(fd);
        return NULL;
    }
    else {
        printf("%s\n", "valid reqeust!");
    }

    /* suppose valid here */
    /* make request */
    handle_connection(fd, r_info, header_buf);
    Free(r_info);
    Close(fd);
    //Pthread_exit(NULL);

    return NULL;
    /* make response */
}

void handle_connection(int connfd, struct request_info *r_info, char *header_buf) {

    /* make request */
    int clientfd;
    int byte_count;
    char *host, *uri, *port, buf[MAXLINE], out_buf[MAXLINE];
    rio_t rio;

    debugprintf("start connection\n");
    host = r_info->hostname;
    debugprintf("host:%s\n", host);
    uri = r_info->uri;
    debugprintf("port: %s\n", r_info->port);
    port = ( (r_info->port)[0] == '\0' ? "80": r_info->port);

    /* Init client fd towards server */
    debugprintf("start clientfd\n");

    clientfd = Open_clientfd(host, port);
    Rio_readinitb(&rio, clientfd);

    /* Initiate header info */
    sprintf(buf, "%s %s %s\r\n", "GET", uri, "HTTP/1.0");

    if ((r_info->port)[0] == '\0') {
        sprintf(buf, "%sHost: %s\r\n", buf, host);
    }
    else {
        sprintf(buf, "%sHost: %s:%s\r\n", buf, host, port);
    }
    //sprintf(buf, "%s%s\r\n", buf, user_agent_hdr);
    sprintf(buf, "%s%s\r\n", buf, header_buf);
    printf("-----------------print headers:\n%s", buf);

    if (rio_writen(clientfd, buf, strlen(buf)) < 0) {
        if (errno == EPIPE) {
            Close(clientfd);
            return;
        }
    }

    /* Start read and forward */
    debugprintf("--------------start read--------------\n");

    Rio_readlineb(&rio, buf, MAXLINE);
    while (strcmp(buf, "\r\n")) {
        //sprintf(out_buf, "%s%s", out_buf, buf);
        if (rio_writen(connfd, buf, strlen(buf)) < 0) {
            if (errno == EPIPE) {
                Close(clientfd);
                return;
            }
        }
        printf("%s", buf);
        Rio_readlineb(&rio, buf,MAXLINE);
    }

    sprintf(buf, "\r\n");
   // Rio_writen(connfd, buf, strlen(buf));
    if (rio_writen(connfd, buf, strlen(buf)) < 0) {
        if (errno == EPIPE) {
            Close(clientfd);
            return;
        }
    }
    
    while (1) { 
        byte_count = Rio_readnb(&rio, buf, MAXLINE);
        if (byte_count == 0) {
            break;
        }

        if (rio_writen(connfd, buf, byte_count) < 0) {
            if (errno == EPIPE) {
                Close(clientfd);
            }
            return;
        }
    }
    Close(clientfd);
    return;
}


int is_valid(int fd, char *method, char *url, char *version, struct request_info *r_info) {
    char hostname[100];
    char port[20] = "";
    char uri[200];
    char uri_tmp[200] = "";
    int valid_flag = 0;
    int robust_case;

    debugprintf("%s\n", url);

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


        printf("before:%s\n", port);

    if (sscanf(url, "http://%99[^:]:%20[^/]/%199s", hostname, port, uri_tmp) == 3)
    {
        debugprintf("case 1");
        valid_flag = 1;
    }
    /* http://fonts.googleapis.com/css?family=Open+Sans:400italic,700italic,800italic,700,800,400*/
    else if (sscanf(url, "http://%99[^/]/%199s", hostname, uri_tmp) == 2) {
        debugprintf("case 2");
        printf("%s\n", port);
        port[0] = '\0'; 
        valid_flag = 1;
    }
    else if ((sscanf(url, "http://%99[^:]:%20[^/]/", hostname, port ) == 2)) {
        valid_flag = 1;
        debugprintf("case 3");
    }
    else if (sscanf(url, "http://%99[^/]/", hostname) == 1) {
        debugprintf("case 4");
        valid_flag = 1;
    }
    /* extreme case */
    else if ( (robust_case = sscanf(url, "%99[^/]/%199s", hostname, uri_tmp)) > 0) {
            if (robust_case == 1) {
                /* set it as empty */
                uri_tmp[0] = '\0';                
            }
        valid_flag = 1;
    }


    if (valid_flag) {
        debugprintf("method: %s \n verision: %s\n", method, version);
        r_info->port = strdup(port);
        debugprintf("port: %s\n", port);
        r_info->hostname = strdup(hostname);
        debugprintf("hostname: %s\n", (r_info->hostname));
        snprintf(uri, sizeof(uri), "%s%s", "/", uri_tmp);
        r_info->uri = strdup(uri);
        debugprintf("uri: %s\n", (r_info->uri));
        return 1;
    }
    else {
        printf("%s\n", hostname);
        printf("%s\n", uri);
        printf("%s\n", port);
        clienterror(fd, hostname, "400","Bad request", "Please check if the url is valid");
        printf("%s\n", "invalid request: URL");
        return 0;
    }


}


void clienterror(int fd, char *cause, char *errnum,
		 char *shortmsg, char *longmsg)
{
    char buf[MAXLINE], body[MAXBUF];
    Rio_writen(fd, buf, strlen(buf));    Rio_writen(fd, buf, strlen(buf));
    /* Build the HTTP response body */
    sprintf(body, "<html><title>T-Proxy Error</title>");
    sprintf(body, "%s<body bgcolor=""ffffff"">\r\n", body);
    sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
    sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
    sprintf(body, "%s<hr><em>The Tiny Proxy</em>\r\n", body);

    /* Print the HTTP response */
    //sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
    sprintf(buf, "HTTP/1.0 %s %s\r\n", "200", "OK");
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
void read_requesthdrs(rio_t *rp, char *header_buf)
{
    char buf[MAXLINE];
    int read_count;

    read_count = Rio_readlineb(rp, buf, MAXLINE);
    while (strcmp(buf, "\r\n") && read_count > 0) {
            GOTCHA;
        if (strstr(buf, "Connection:") || strstr(buf, "Proxy-Connection") 
                || strstr(buf, "User-Agent") || strstr(buf, "Host:")) 
        {
            //Do nothing
        }
        else {
            sprintf(header_buf, "%s%s", header_buf, buf); 
        }
        read_count = Rio_readlineb(rp, buf, MAXLINE);
    }

    sprintf(header_buf, "%s%s\r\n", header_buf, "Connection: close");
    sprintf(header_buf, "%s%s\r\n", header_buf, "Proxy-Connection: close");
    sprintf(header_buf, "%s%s\r\n", header_buf, user_agent_hdr);
    debugprintf("finish reading heaeder %s", header_buf);
    return;
}
/* $end read_requesthdrs */

//void debugprintf(){}
