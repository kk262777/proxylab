#include "csapp.h"
#include "cache.h"

extern struct cache_node *cache_head;
extern struct cache_node *cache_tail;
extern struct cache_entry *entry;

void init_cache() {
    //init cache   
    entry = (struct cache_entry*)Calloc(1, sizeof(struct cache_entry));
    entry->available_size = MAX_CACHE_SIZE;
    cache_tail = NULL;
}

/*
 * check_available - check size needed to insert a new cache
 *
 */
int check_available(size_t size_needed) {
    if (entry->available_size >= size_needed) {
        return 1;
    }
    else {
        return 0;
    }
}


int search_cache(char *search_hostname, char *search_uri, 
        size_t search_size, struct cache_node **cache_ptr) 
{
    if (cache_head == NULL) 
        return EMPTY_CACHE;

    struct cache_node *search_cache = cache_head->next;
    while (search_cache != NULL) {
        if (!strcmp(search_cache->hostname, search_hostname)) {
            if (!strcmp(search_cache->uri, search_uri)){
                /* If found modify the pointer*/
                *cache_ptr = search_cache;
                return FOUND;
            }
        }
        search_cache = search_cache->next;            
    }
    
    printf("failed find cache\n");
    return NO_FOUND;
}

/*
 * create_node - create a cache node according to request info
 *
 *
 */
struct cache_node* create_node(char *hostname, char *uri,
        size_t object_size, char *object_buf) 
{
    /* Malloc a char array big enough*/
    char *cache_content = (char *)malloc(object_size);
    struct cache_node *new_cache_node = malloc(sizeof(struct cache_node));
    new_cache_node->hostname = hostname; 
    new_cache_node->uri = uri;
    new_cache_node->block_size = object_size;
    new_cache_node->content = cache_content;
    return new_cache_node;
} 


/*
 * insert_node - helper function to insert node to a cache_list
 *
 *
 */
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
        cache_tail = new_node;
    }

    /* Update cache available size */
    entry->available_size -= new_node->block_size;
    if ((entry->available_size) < 0) {
        unix_error("Available size become negative, should not happen");
    }
}

/*
 * detach_node - unlink node from list, but not delete it.
 *
 */
void detach_node(struct cache_node *detach_node) {
    if (detach_node->prev == NULL) {
        if (detach_node->next == NULL) {
            entry->next = NULL;
            cache_tail = NULL;
        }
        else {
            (detach_node->next)->prev = NULL;
            entry->next = detach_node->next;
        }
      //  if (detach_node->next) {
      //      unix_error("If no prev node, should not have next node");
      //  }
    }
    else if (detach_node->next == NULL) {
        if (detach_node != cache_tail) {
            unix_error("detach_node != cache_tail\n");
        }
        /* above case covered case that prev is null */
        cache_tail = detach_node->prev;
        (detach_node->prev)->next = NULL;
    }

    else {
        (detach_node->next)->prev = detach_node->prev;
        (detach_node->prev)->next = detach_node->next;
    }
}

/*
 * delete_node - A helper function, given a cache_node, detach it and free it.
 *
 */
void delete_node(struct cache_node *delete_node) {
    size_t size = delete_node->block_size;
    detach_node(delete_node);
    Free(delete_node);

    /* Update cache available size */
    entry->available_size += size;

}

void evict_LRU(size_t size_needed) {
    /* Calculate gap */
    if (size_needed < entry->available_size) 
        unix_error("size_needed < available_size\n");
    size_t gap = size_needed - entry->available_size;

    /* Evict cache_nodes from tail*/
    size_t evicted_size = 0;
    size_t total_evicted_size = 0;
    while (total_evicted_size < gap) {
        evicted_size = cache_tail->block_size;
        /* delete already add size */
        delete_node(cache_tail);
        total_evicted_size += evicted_size;
    }
       
    if (size_needed > entry->available_size) 
        unix_error("evict fail, please check\n");


}

