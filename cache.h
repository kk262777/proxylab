#include <stdio.h>
#include "csapp.h"

#ifndef MAX_CACHE_SIZE
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400
#endif

#ifndef NO_ENOUGH_SPACE  
#define FOUND 1
#define NO_ENOUGH_SPACE 2
#define NO_FOUND 3
#define EMPTY_CACHE 4
#endif

typedef struct cache_node {
    char *hostname;
    char *uri;
    size_t block_size;
    char *content;
    struct cache_node *prev;
    struct cache_node *next;
} cache_node;

typedef struct cache_entry {
    struct cache_node *next;
    size_t available_size; 
} cache_entry;


