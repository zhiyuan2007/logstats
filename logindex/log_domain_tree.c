#include <pthread.h>
#include <assert.h>
#include <stdint.h>
#include "log_domain_tree.h"


static inline 
int name_compare(const void *value1, const void *value2)
{
    assert(value1 && value2);
    const char *name1 = ((node_data_t *)value1)->fqdn;
    const char *name2 = ((node_data_t *)value2)->fqdn;

    return strcmp(name1, name2);
}

domain_tree_t *
domain_tree_create()
{
    domain_tree_t *tree = calloc(1, sizeof(domain_tree_t));
    if (!tree)
        return NULL;

    tree->_tree = rbtree_create(name_compare);
    if (!tree->_tree)
        goto MEM_ALLOC_FAILED;

    tree->node_pool = mem_pool_create(sizeof(rbnode_t), 1024, true);
    if (!tree->node_pool)
        goto MEM_ALLOC_FAILED;

    tree->name_pool = mem_pool_create(sizeof(node_data_t), 1024, true);
    if (!tree->name_pool)
        goto MEM_ALLOC_FAILED;

    tree->count = 0;

    return tree;

MEM_ALLOC_FAILED :
    if (tree->name_pool) mem_pool_delete(tree->name_pool);
    if (tree->node_pool) mem_pool_delete(tree->node_pool);
    if (tree->_tree) free(tree->_tree);
    free(tree);
}

void
domain_tree_destroy(domain_tree_t *tree)
{
    mem_pool_delete(tree->node_pool);
    mem_pool_delete(tree->name_pool);
    free(tree->_tree);
    free(tree);
}

static inline bool
name_end_with_dot(const char *name)
{
    assert(name && strlen(name));
    return name[strlen(name) - 1] == '.';
}

int
domain_tree_insert(domain_tree_t *tree, const char *name)
{
    if (!name || (strlen(name) >= MAX_NAME_LEN))
        return -1;

    node_data_t *nv = domain_tree_exactly_find(tree, name);
    if (nv == NULL)
    {
       nv = (node_data_t *)mem_pool_alloc(tree->name_pool);
       if (!nv)
       {
           return -1;
       }
       rbnode_t *node = (rbnode_t *)mem_pool_alloc(tree->node_pool);
       if (!node) {
           return -1;
       }

       strcpy(nv->fqdn, name);
       nv->type = 0;
       if (!name_end_with_dot(name))
           strcat(nv->fqdn, ".");
       node->value = (void *)nv;

       rbnode_t *insert_node = rbtree_insert(tree->_tree, node);
       if (!insert_node) {
           mem_pool_free(tree->node_pool, (void *)node);
           return -1;
       }
    }
    return 0;
}

int
domain_tree_delete(domain_tree_t *tree,
                    const char *name, del_node1 del_func)
{
    if (!name || (strlen(name) >= MAX_NAME_LEN))
        return -1;

    node_data_t nv;
    strcpy(nv.fqdn, name);
    if (!name_end_with_dot(name))
        strcat(nv.fqdn, ".");

    rbnode_t *del_node = rbtree_delete(tree->_tree, (void *)&nv);
    if (!del_node)
        return -1;

    mem_pool_free(tree->name_pool, del_node->value);
    mem_pool_free(tree->node_pool, (void *)del_node);

    tree->count--;
    return 0;
}

node_data_t *
domain_tree_exactly_find(domain_tree_t *tree,
                       const char *name)
{
    if (!name || (strlen(name) >= MAX_NAME_LEN))
        return NULL;

    node_data_t nv;
    strcpy(nv.fqdn, name);
    if (!name_end_with_dot(name))
        strcat(nv.fqdn, ".");

    rbnode_t *node = rbtree_search(tree->_tree, (void *)&nv);
    if (!node)
    {
        return NULL;
    }
    return node->value;
}

static inline bool
name_is_root(const char *name)
{
    assert(name && strlen(name));
    return (strlen(name) == 1) && (name[0] == '.');
}

static inline const char *
generate_parent_name(const char *name)
{
    assert(name && strlen(name));
    if (name_is_root(name))
        return NULL;

    const char *dot_pos = strchr(name, '.');
    if ((dot_pos - name) == (strlen(name) - 1))
        return dot_pos;
    else
        return dot_pos + 1;
}

node_data_t *
domain_tree_closely_find(domain_tree_t *tree,
                       const char *name)
{
    if (!name || (strlen(name) >= MAX_NAME_LEN))
        return NULL;

    node_data_t nv;
    strcpy(nv.fqdn, name);
    if (!name_end_with_dot(name))
        strcat(nv.fqdn, ".");

    const char *name_pos = nv.fqdn;
    void *find_name = NULL;

    do {
        if (find_name = domain_tree_exactly_find(tree, name_pos))
            return find_name;
    } while (name_pos = generate_parent_name(name_pos));

    return NULL;
}
