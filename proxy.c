#include <stdio.h>
#include "cache.h"
#include "csapp.h"

/*
 * Guangyao Xie <guangyax>
 *
 * A proxy implementation
 * It accept HTTP request, forwarding to servers and receive/cache replies
 * Use linked list as data structure, LRU eviction policy
 *
 * Also plaese notice that cache routines are mainly in cache.c
 *
 */

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400
#define MAX_THREAD_NUM 12

typedef struct request_info {
    char *hostname;
    char *method;
    char *port;
    char *uri;
} request_info;

struct cache_entry *entry;
struct cache_node *cache_head;
struct cache_node *cache_tail;
/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";

/* read write lock */
pthread_rwlock_t cache_rwlock;

//volatile file?
void *doit(void* connfd_ptr);
int parse_uri(char *uri, char *filename, char *cgiargs);
void clienterror(int fd, char *cause, char *errnum,
char *shortmsg, char *longmsg);
void read_requesthdrs(rio_t *rp, char *header_buf);
int is_valid(int fd, char *method, char *url,
        char *version, struct request_info *r_info);
void handle_connection(int fd, struct request_info *r_info, char *header_buf);
int forward_cache(int fd, struct request_info *r_info, char *header_buf) ;
void handle_hostname(char* host, char* port, char* result);
void free_r_info(struct request_info *r_info);
#ifndef debugprintf
#define debugprintf  printf
//void debugprintf();
#define GOTCHA printf("GOTCHA!\n");
#endif

int main(int argc, char **argv)
{
    /* Variables */
    int listenfd, connfd;
    int *connfd_ptr;
    int rc;
    pthread_t tid;
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;

    signal(SIGPIPE, SIG_IGN);
    /* Check command line args */
    if (argc != 2) {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(1);
    }
    /* Block SIGPIPE signal*/
    init_cache();
    /* init lock */
    rc = pthread_rwlock_init(&cache_rwlock, NULL);
    debugprintf("init lock return %d\n", rc);

    /* Client length */
    listenfd = Open_listenfd(argv[1]);
    while(1) {
        clientlen = sizeof(clientaddr)  ;
        connfd = Accept(listenfd, (SA *) &clientaddr, &clientlen);

        /* Parse connection's fd to threads */
        connfd_ptr = Malloc(sizeof(void *));
        *connfd_ptr = connfd;
        Pthread_create(&tid, NULL, doit, connfd_ptr);
    }

}


/*
 * doit - thread routine to handle each request
 * It validate the request, check the cache and save cache.
 *
 */
void *doit(void *connfd_ptr) {
    int fd = *(int *) connfd_ptr;
    Pthread_detach(Pthread_self());
    Free(connfd_ptr);
    int rc;
    char buf[MAXLINE], method[MAXLINE], url[MAXLINE], version[MAXLINE];
    char header_buf[MAXLINE];
    rio_t rio;
    request_info* r_info;

    r_info = Malloc(sizeof(request_info));
    debugprintf("\n---------New thread run\n");

    /* Accept each request */
    Rio_readinitb(&rio, fd);

    /*Read one line*/
    if (!Rio_readlineb(&rio, buf, MAXLINE)) {
        Free(r_info);    /* Free and close and quit */
        Close(fd);
        return NULL;
    }

    debugprintf("%s", buf);

    /* Read remained request header */
    read_requesthdrs(&rio, header_buf);

    /* Check request first line */
    if (sscanf(buf, "%s %s %s", method, url, version) != 3) {
        Free(r_info);    /* Free and close and quit */
        Close(fd);
        return NULL;
    }

    /* Validate request and parse infos */
    if (!is_valid(fd, method, url, version, r_info)) {
        debugprintf("%s\n", "invalid request");
        Free(r_info);
        Close(fd);
        return NULL;
    }
    else {
        debugprintf("%s\n", "Valid reqeust!");
    }

    /* Cache routine, find and forward */
    rc = forward_cache(fd, r_info, header_buf);

    /* If not found start connection */
    if (rc == NO_FOUND)
        handle_connection(fd, r_info, header_buf);
    if (rc == RIO_ERROR)
        printf("RIO_ERROR happened\n");

    free_r_info(r_info);
    Free(r_info);

    Close(fd);

    return NULL;
}


/*
 * forward_cache - try to find and forward cache to client.
 * When theres a cache hit, move the cache node to the head of list
 * return NO_FOUND if no cache available
 *
 */
int forward_cache(int fd, struct request_info *r_info, char *header_buf) {

    struct cache_node *result_cache;
    int rc;
    char search_hostname[100];

    /* Read infos: concatenate hostname and port */
    handle_hostname(r_info->hostname, r_info->port, search_hostname);

    /* Find cache according to cache info */
    rc = pthread_rwlock_rdlock(&cache_rwlock);
    if (rc) {
        Close(fd);
        unix_error("thread lock issues happend\n");
    }

    rc = search_cache(search_hostname, r_info->uri, &result_cache);

    /* if cache is not found, return NO_FOUND */
    if (rc == NO_FOUND || rc == EMPTY_CACHE) {
        debugprintf("---NO_CACHE_FOUND----\n");
        pthread_rwlock_unlock(&cache_rwlock);
        return NO_FOUND;
    }

    /* Start read and forward cache header and content*/
    if (rc == FOUND) {
        debugprintf("--------------------FORWARD CACHE HDR BEGIN--------\n");
        /* Forward headers */
        if (rio_writen(fd, result_cache->header,
                    strlen(result_cache->header)) < 0)
        {
            if (errno == EPIPE) {
                pthread_rwlock_unlock(&cache_rwlock);
                return RIO_ERROR;
            }
            unix_error("something wrong with EPIPE error\n");
        }

        debugprintf("--------------------FORWARD CACHE OBJ BEGIN--------\n");
        /* Forward cache object */
        debugprintf("%s: %lu\n","block_size", result_cache->block_size );
        if (rio_writen(fd, result_cache->content,
                    result_cache->block_size) < 0)
	{
            if (errno == EPIPE) {
                pthread_rwlock_unlock(&cache_rwlock);
                return RIO_ERROR;
            }
        }
    }
    /* move node to head */
    //sleep(3);
    pthread_rwlock_unlock(&cache_rwlock);
    /* try to acquire a lock */
    rc = pthread_rwlock_trywrlock(&cache_rwlock);
    if (!rc) {
        if (detach_node(result_cache)) {
            insert_node(result_cache);
        }
        /* unlock */
        pthread_rwlock_unlock(&cache_rwlock);
    }
    return 0;
}


/*
 * handle_connection - Start fetching from server and forward to clients.
 * Attmpt to save objects to cache, if no space, evicted farest cache_nodes
 *
 */
void handle_connection(int connfd, struct request_info *r_info
        , char *header_buf)
{
    /* make request */
    int clientfd, size_exceed_flag, byte_count, object_total_byte_count;
    char *host,*uri,*port, hdr_hostname[MAXLINE], buf[MAXLINE], buf2[MAXLINE],
         cache_header_buf[MAXLINE], cache_content_buf[MAX_OBJECT_SIZE];
    rio_t rio;

    /* Start sending request to server */
    host = r_info->hostname;
    uri = r_info->uri;
    debugprintf("start connection\n");
    debugprintf("host:%s\n", host);

    /* if port is empty set it 80 (for connection) */
    port = ( (r_info->port)[0] == '\0' ? "80": r_info->port);
    debugprintf("port: %s\n", port);

    /* Concatenate hostname and port for http header and cache */
    handle_hostname(r_info->hostname, r_info->port, hdr_hostname);

    debugprintf("start clientfd\n");
    clientfd = Open_clientfd(host, port);
    Rio_readinitb(&rio, clientfd);

    /* Initiate header info */
    /* Compose headers according to cases */
    sprintf(buf, "%s %s %s\r\n", "GET", uri, "HTTP/1.0");
    sprintf(buf, "%sHost: %s\r\n", buf, hdr_hostname);

    /* Append client's request header to buf */
    sprintf(buf, "%s%s\r\n", buf, header_buf);

    debugprintf("-----------------print headers:\n%s", buf);

    if (rio_writen(clientfd, buf, strlen(buf)) < 0) {
        if (errno == EPIPE) {
            Close(clientfd);
            return;
        }
    }
    /* End sending request */

    /* Start forwarding response header */
    debugprintf("-----start read-------\n");

    // save cache header
    Rio_readlineb(&rio, buf, MAXLINE);
    while (strcmp(buf, "\r\n")) {
        /* write buf into another buf */
        sprintf(cache_header_buf, "%s%s", cache_header_buf, buf);
        if (rio_readlineb(&rio, buf,MAXLINE)){
             if(errno == ECONNRESET){
                Close(clientfd);
                printf("\nRIO error ECONNRESET n");
                return;
            }
        }
        printf("%s\n", buf);
    }
    /* Write \r\n to end header text */
    sprintf(cache_header_buf, "%s\r\n", cache_header_buf);
    debugprintf("Finish appending header\n");

    /* Write to client */
    if (rio_writen(connfd, cache_header_buf, strlen(cache_header_buf)) < 0) {
        if (errno == EPIPE) {
            Close(clientfd);
            return;
        }
    }
    /* End forwarding response header*/
    debugprintf("Finish forwarding header\n");

    /* Write response body to client */
    object_total_byte_count = 0;
    size_exceed_flag = 0;

    while (1) {
        buf2[0] = '\0';
        /* Read body */
        byte_count = rio_readnb(&rio, buf2, MAXLINE);
        if (byte_count < 0) {
            if(errno == ECONNRESET){
                Close(clientfd);
                printf("\nRIO error ECONNRESET n");
                return;
            }
        }
        /* Write body to client */
        if (rio_writen(connfd, buf2, byte_count) < 0) {
            if (errno == EPIPE) {
                printf("\nRio error: EPIPE n");
                Close(clientfd);
            }
            return;
        }

        object_total_byte_count += byte_count;
        if (object_total_byte_count > MAX_OBJECT_SIZE) {
            /* discard cache*/
            size_exceed_flag = 1;
        }
        else {
            void *ptr = (void*)((char*)(cache_content_buf)
            + object_total_byte_count - byte_count);
            memcpy(ptr, buf2, byte_count);
        }
        if (byte_count == 0) { //finish reading
            break;
        }
    }
    /* End write response body to client */

    /* Following routine don't need this*/
    Close(clientfd);
    /* server parse parse nothing */
    if (object_total_byte_count <= 0) {
        return;
    }

    /* If not exceed */
    if (!size_exceed_flag) {
        debugprintf("--------------------SAVE CACHE BEGIN--------\n");
        /* Call this function to get a cache node pointer*/
        struct cache_node *new_cache =
            create_node(hdr_hostname, uri, object_total_byte_count,
            cache_content_buf, cache_header_buf);

        pthread_rwlock_wrlock(&cache_rwlock);
        if (check_available(object_total_byte_count)){
            /* create cache node */

            /* write lock*/
            insert_node(new_cache);
            pthread_rwlock_unlock(&cache_rwlock);
        }
        else {
        /* if exceed LRU */
            evict_LRU(object_total_byte_count);
            insert_node(new_cache);
            pthread_rwlock_unlock(&cache_rwlock);
        }
        debugprintf("--------------------SAVE CACHE END--------\n");
        debugprintf("entry next: %p\n", entry->next);
    }
    return;
}


/*
 * is_valid - A validation and parser for requests.
 * It checks if request are valid using sscanf and parse infos
 * to a request_info struct
 *
 */
int is_valid(int fd, char *method, char *url,
        char *version, struct request_info *r_info)
{
    char hostname[100];
    char port[20] = "";
    char uri[MAXLINE] = "";
    char uri_tmp[MAXLINE] = "";
    int valid_flag = 0;
    //int robust_case;

    debugprintf("%s\n", url);

    /* Check request */
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

        /* Match and parse request URL */
    if (sscanf(url, "http://%99[^:]:%20[^/]/%8191s",
                hostname, port, uri_tmp) == 3)
    {
        debugprintf("case 1");
        valid_flag = 1;
    }
    else if (sscanf(url, "http://%99[^/]/%8191s",
                hostname, uri_tmp) == 2)
    {
        debugprintf("case 2");
        port[0] = '\0';
        valid_flag = 1;
    }
    else if ((sscanf(url, "http://%99[^:]:%20[^/]/",
                hostname, port ) == 2))
    {
        valid_flag = 1;
        debugprintf("case 3");
    }
    else if (sscanf(url, "http://%99[^/]/",
                hostname) == 1)
    {
        debugprintf("case 4");
        valid_flag = 1;
    }
    // /* extreme case */
    // else if ((robust_case = sscanf(url, "%99[^/]/%499s",
    //                 hostname, uri_tmp)) > 0)
    // {
    //     if (robust_case == 1) {
    //         /* set it as empty */
    //         uri_tmp[0] = '\0';
    //     }
    //     valid_flag = 1;
    // }

    /* Save infos into a request_info struct*/
    if (valid_flag) {
        debugprintf("method: %s \n verision: %s\n", method, version);
        r_info->port = strdup(port);
        debugprintf("port: %s\n", port);
        r_info->hostname = strdup(hostname);
        debugprintf("hostname: %s\n", (r_info->hostname));
        sprintf(uri, "%s%s", "/", uri_tmp);
        r_info->uri = strdup(uri);
        debugprintf("uri: %s\n", (r_info->uri));
        return 1;
    }
    else {
        clienterror(fd, hostname, "400","Bad request",
        "Please check if the url is valid");
        debugprintf("%s\n", "invalid request: URL");
        return 0;
    }


}


/*
 * Reply a error messgage to client.
 * Ususally it won't work
 *
 */
void clienterror(int fd, char *cause, char *errnum,
        char *shortmsg, char *longmsg)
{
    char buf[MAXLINE], body[MAXBUF];
    int rc;
    rc = rio_writen(fd, buf, strlen(buf));
    if (rc < 0) {
        if (errno == EPIPE) {
            return;
        }
    }
    /* Build the HTTP response body */
    sprintf(body, "<html><title>T-Proxy Error</title>");
    sprintf(body, "%s<body bgcolor=""ffffff"">\r\n", body);
    sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
    sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
    sprintf(body, "%s<hr><em>The Tiny Proxy</em>\r\n", body);

    /* Print the HTTP response */
    sprintf(buf, "HTTP/1.0 %s %s\r\n", "200", "OK");
    sprintf(buf, "%sContent-type: text/html\r\n", buf);
    sprintf(buf, "%sContent-length: %d\r\n\r\n", buf, (int)strlen(body));
    if (rio_writen(fd, buf, strlen(buf)) < 0) {
        if (errno == EPIPE) {
            return;
        }
    }
    if (rio_writen(fd, body, strlen(body)) < 0) {
        if (errno == EPIPE) {
            return;
        }
    }
}
/* $end clienterror */

/*
 * read_requesthdrs - read HTTP request headers
 * and save it into a buffer to be forwarded to client.
 */
/* $begin read_requesthdrs */
void read_requesthdrs(rio_t *rp, char *header_buf)
{
    char buf[MAXLINE] = "";
    int read_count;

    read_count = rio_readlineb(rp, buf, MAXLINE);
    if (read_count < 0) {
        if(errno == ECONNRESET){
                printf("\nRIO error ECONNRESET n");
                return;
         }
    }

    /* read request headers */
    while (strcmp(buf, "\r\n") && read_count > 0) {
        /* We will overwrite these header infos */
        if (strstr(buf, "Connection:") || strstr(buf, "Proxy-Connection")
                || strstr(buf, "User-Agent") || strstr(buf, "Host:"))
        { // do nothing
        }
        else {
            /* readlineb reads \r\n too */
            sprintf(header_buf, "%s%s", header_buf, buf);
        }
        read_count = rio_readlineb(rp, buf, MAXLINE);
        if (read_count < 0) {
                if(errno == ECONNRESET){
                        printf("\nRIO error ECONNRESET n");
                        return;
                 }
            }
    }

    sprintf(header_buf, "%s%s\r\n", header_buf, "Connection: close");
    sprintf(header_buf, "%s%s\r\n", header_buf, "Proxy-Connection: close");
    sprintf(header_buf, "%s%s\r\n", header_buf, user_agent_hdr);
    debugprintf("finish reading heaeder %s", header_buf);
    return;
}
/* $end read_requesthdrs */

/*
 * handle_hostname - A helper function to concatenate hostname
 * Check if port is empty. If empty, save hostname only.
 * If not empty, join the hostname and port with ':'
 */
void handle_hostname(char* host, char* port, char* result) {
    size_t needed;
    /* If port is empty, save hostname without port */
    if ((port)[0] == '\0')
	strcpy(result, host);
    else  {
	/* Calculate needed size */
        needed = snprintf(NULL, 0, "%s:%s", host, port);
        char buf[needed];
        snprintf(buf, needed + 1, "%s:%s", host, port);
        strcpy(result, buf);
    }
}

void free_r_info(struct request_info *r_info) {
    if (r_info->hostname[0] != '\0')
        Free(r_info->hostname);
    if (r_info->port[0] != '\0')
        Free(r_info->port);
    if (r_info->uri[0] != '\0')
        Free(r_info->uri);
}

//void debugprintf(){}
