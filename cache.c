#include "cache.h"
#include "csapp.h"
typedef struct cache_node {
    char *hostname;
    char *uri;
    size_t block_size;
    struct cache_node *prev;
    struct cache_node *next;
} cache_node;

typedef struct cache_entry {
    struct cache_node *next;
    size_t available_size; 
} cache_entry;


struct cache_node *cache_head;
struct cache_node *cache_tail;
struct cache_entry *entry;

void init_cache(struct cache_node *cache_head, struct cache_node *cache_tail) {
    //init cache   
    entry = NULL;
    cache_tail = NULL;
}

void search_cache(char *search_hostname, char *search_uri, size_t search_size) {
        if (cache_head == NULL) {
            return;
        }

        struct cache_node *search_cache = cache_head->next;
        while (search_cache != NULL) {
            if (strcmp(search_cache->hostname, search_hostname)) {
                if (strcmp(search_cache->uri, search_uri)){

                    return ; //xxxxxxxx
                }
            }

            search_cache = search_cache->next;            
        }
}

struct cache_node* create_node(char *hostname, char *uri, size_t object_size) {
} 



void insert_node(struct cache_node *new_node) {
    /* Insert to head, neck used to be the first node*/
    struct cache_node *neck = entry->next; 
    if (neck != NULL ) {
        neck->prev = new_node;
        new_node->next = neck;
        new_node->prev = NULL;
        entry->next = new_node;
    }
    else {
        entry->next = new_node;
    }
}

/*
 * detach_node - unlink node from list, but not delete it.
 *
 *
 */
void detach_node(struct cache_node *detach_node) {
    if (detach_node->prev == NULL) {
        entry->next = NULL;
        //Free(delete_node);
        
        if (detach_node->next) {
            unix_error("if no prev node, should not have next node");
        }
    }
    else {
        (detach_node->next)->prev = detach_node->prev;
        (detach_node->prev)->next = detach_node->next;
    }
}

/*
 * delete_node - detach it and free it.
 *
 */
void delete_node(struct cache_node *delete_node) {
    detach_node(delete_node);
    Free(delete_node);
}

void evict_LRU() {
    /* Evict cache_nodes from tail*/
    
    /* Update + available size */


    /* call insert node */
}

