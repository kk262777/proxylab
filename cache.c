#include "cache.h"

/*
 * Guangyao Xie <guangyax>
 * cache functions for proxy lab
 * use linked list as data structure
 */
extern struct cache_node *cache_head;
extern struct cache_node *cache_tail;
extern struct cache_entry *entry;

/* read global variables and initialize them */
void init_cache() {
    /* init cache */
    entry = (struct cache_entry*) malloc(sizeof(struct cache_entry));
    
    entry->available_size = MAX_CACHE_SIZE;
    entry->next = NULL;
    cache_tail = NULL;
    debugprintf("entry inited, %p\n", entry);
    debugprintf("entry next, %p\n", entry->next);
}

/*
 * check_available - check size needed to insert a new cache
 * if not return 0
 */
int check_available(size_t size_needed) {
    debugprintf("--------------check size available! -------------\n");
    if (entry->available_size >= size_needed) {
        debugprintf(" good with %lu\n", entry->available_size);
        return 1;
    }
    else {
        debugprintf(" no %lu\n", entry->available_size);
        return 0;
    }
}

/*
 * check_cachelist - check cache by iterating the whole list
 */
void check_cachelist () {
    struct cache_node *ptr = entry->next;
    debugprintf("--------------------CHECK  CACHE--\n");
    while (ptr!=NULL) {
        if (ptr->block_size == 35) {
            debugprintf("ptr: %p\n", ptr);
            debugprintf("size: %lu", ptr->block_size);
            debugprintf("size: %s", ptr->header);
        }
        ptr = ptr->next;
    }
}

/*
 * search_cache - check if there are available cache in the list
 * If found, return FOUND to caller func
 *
 */
int search_cache(char *search_hostname, char *search_uri,
         struct cache_node **cache_ptr)
{
    debugprintf("--------------search node! -------------\n");
    debugprintf("search:%s %s\n", search_hostname, search_uri);
    if ((entry->next) == NULL) {
        debugprintf("EMPTY_CACHE\n");
        return EMPTY_CACHE;
    }

    struct cache_node *search_cache = entry->next;
    while (search_cache != NULL) {
        /* Check host name and uri */
        if (!strcmp(search_cache->hostname, search_hostname)) {
            if (!strcmp(search_cache->uri, search_uri)){
                /* If found, modify the pointer*/
                //debugprintf("cache-found\n");
                *cache_ptr = search_cache;
                return FOUND;
            }
        }
        search_cache = search_cache->next;
    }
    debugprintf("failed find cache\n");
    return NO_FOUND;
}

/*
 * create_node - create a cache node according to request info
 * it accept arguments from caller function and return to caller a pointer
 *
 */
struct cache_node* create_node(char *hostname, char *uri,
        size_t object_size, char *object_buf, char* cache_header)
{
    /* Malloc a char array big enough*/
    struct cache_node *new_cache_node = malloc(sizeof(struct cache_node));
    new_cache_node->hostname = strdup(hostname);
    new_cache_node->uri = strdup(uri);
    new_cache_node->header = strdup(cache_header);
    new_cache_node->block_size = object_size;
    new_cache_node->content = malloc(object_size);
    memcpy(new_cache_node->content, object_buf, object_size);
    return new_cache_node;
}


/*
 * insert_node - helper function to insert node to a cache_list
 * Always make it to the head of list
 */
void insert_node(struct cache_node *new_node) {
    /* Insert to head, neck used to be the first node*/
    debugprintf("--------------insert node ! -------------\n");
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
        new_node->prev = NULL;
        new_node->next = NULL;
    }

    debugprintf("finish insert\n");
    /* Update cache available size */
    entry->available_size -= new_node->block_size;
}


/*
 * detach_node - unlink node from list, but not delete it.
 * return 1 if actually detached, if not return 0 and caller
 * will not need to add again.
 */
int detach_node(struct cache_node *detach_node) {
    debugprintf("-------------detach node ! -------------\n");
    struct cache_node *previous_node = detach_node->prev;
    struct cache_node *next_node = detach_node->next;

    if (previous_node != NULL) {

        if (next_node != NULL) {
            previous_node->next = next_node;
            next_node->prev = previous_node;
        }
        else {
            previous_node->next = NULL;
            cache_tail = previous_node;
        }
        return 1;
    }
    else {
        return 0;
    }

}

/*
 * delete_node - A helper function, given a cache_node, detach it and free it.
 * first call detach function and then free it
 */
void delete_node(struct cache_node *delete_node) {
    size_t size = delete_node->block_size;
    detach_node(delete_node);
    free_node_var(delete_node);
    Free(delete_node);

    /* Update cache available size */
    entry->available_size += size;
}

void free_node_var(struct cache_node *delete_node) {
    Free(delete_node->hostname);
    Free(delete_node->uri);
    Free(delete_node->content);
    Free(delete_node->header);

}

/*
 * Evict LRU
 * Evict the farest node, recalculate the size available
 */
void evict_LRU(size_t size_needed) {
    /* Calculate gap */
    debugprintf("--------------------EVICT!!!!----------\n");
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
}
