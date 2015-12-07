#include <stdio.h>
#include <csapp.h>

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";

//volatile file?

int main(int argc, char **argv)
{
    /* variables */
    int listenfd, connfd;
    char hostname[MAXLINE], port[MAXLINE];
    socklen_t clientlen;
    struct sockaddr_storage clientaddr

    /* Check command line args */
    if (argc != 2) {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
    	exit(1);
    }

// client length?
    listenfd = Open_listenfd(argv[1]);
    while(1) {
        clientlen = sizeof(clientaddr);
        connfd = Accept(listenfd, (SA *)clientaddr, &clientlen);
        Getnameinfo((SA *) &clientaddr, clientlen, hostname,
        MAXLINE, port, MAXLINE, 0)
        prinft("Accepted connection: (%s, %s)\n", hostname, port);
        doit(connfd);
        Close(connfd);
    }

//    printf("%s", user_agent_hdr);
}
