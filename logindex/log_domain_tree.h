#ifndef _H_DOMAIN_NAME_TREE_
#define _H_DOMAIN_NAME_TREE_

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include "dig_mem_pool.h"
#include "dig_rb_tree.h"

#define MAX_NAME_LEN    32

typedef struct domain_tree domain_tree_t;
typedef struct node_data node_data_t;
struct node_data
{
    char fqdn[MAX_NAME_LEN];
    short type;
};

struct domain_tree
{
    rbtree_t    *_tree;
    mem_pool_t  *node_pool;
    mem_pool_t  *name_pool;
    uint32_t    count;
    pthread_rwlock_t rwlock;
};

typedef (*del_node1)(void *manager, void *data);

domain_tree_t *   domain_tree_create();
uint64_t domain_tree_get_size(domain_tree_t *tree); 
void            domain_tree_destroy(domain_tree_t *tree);
int domain_tree_insert(domain_tree_t *tree,
                        const char *name);
int             domain_tree_delete(domain_tree_t *tree,
                        const char *name, del_node1 del_func);
node_data_t *domain_tree_exactly_find(domain_tree_t *tree,
                        const char *name);
node_data_t *domain_tree_closely_find(domain_tree_t *tree,
                        const char *name);
int domain_tree_topn(domain_tree_t *tree, int topn, char *buff);

#endif  /* _:H_DOMAIN_NAME_TREE_ */
