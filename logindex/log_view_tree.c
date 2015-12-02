#include <stdlib.h>
#include "dig_rb_tree.h"
#include "log_view_tree.h"
#include "log_utils.h"
static inline
int name_compare(const void *value1, const void *value2)
{
    assert(value1 && value2);
    const char *name1 = ((view_tree_node_t *)value1)->name;
    const char *name2 = ((view_tree_node_t *)value2)->name;

    return strcmp(name1, name2);
}
view_tree_t *view_tree_create(int (cmp)(const void *, const void *))
{
	view_tree_t *vt = malloc(sizeof(view_tree_t));
	if (vt == NULL)
		return NULL;
	vt->rbtree = rbtree_create(cmp);
    vt->count = 0;
	ASSERT(vt->rbtree, "rbtree or lru list init failed may be no enough memory");
    return vt;
}

uint64_t view_tree_get_size(view_tree_t *tree) {
	uint64_t total_size = 0;
    rbnode_t *node;
    RBTREE_FOR(node, rbnode_t *, tree->rbtree)
    {
        view_tree_node_t *tnode = (view_tree_node_t *)(node->value);
        view_stats_t *vs = tnode->vs;
	}
	return total_size;
}

view_tree_node_t *view_tree_the_only_one(view_tree_t *tree){
    rbnode_t *node;
	int i = 0;
	view_tree_node_t *vtnode = NULL;
    RBTREE_FOR(node, rbnode_t *, tree->rbtree)
    {
        vtnode = (view_tree_node_t *)(node->value);
		printf(">>>>>>>>>>>>>>>>>>>>>>only one %d \n", i);
		i++;
	}
	return vtnode;
}

void view_tree_set_memsize(view_tree_t *tree, uint64_t expect_size) {
}

int view_tree_insert(view_tree_t *tree, const char *name, view_stats_t *vs)
{
    view_tree_node_t *vtnode = malloc(sizeof(view_tree_node_t));
    strcpy(vtnode->name, name);
    vtnode->vs = vs;
	vtnode->ptr = NULL;
    rbnode_t *node = (rbnode_t *)malloc(sizeof(rbnode_t));
    node->parent = NULL;
    node->left = NULL;
    node->right = NULL;
    node->color =  1;
    node->value = vtnode;

    rbnode_t *insert_node = rbtree_insert(tree->rbtree, node);
    if (!insert_node)
    {
        free(vtnode);
        free(node);
        return 1;
    }

    tree->count++;
    return 0;
}

void *view_tree_find(view_tree_t *tree, const char *name)
{
    view_tree_node_t vtnode;
    strcpy(vtnode.name, name);
    vtnode.vs = NULL;
    rbnode_t *node = rbtree_search(tree->rbtree, (void *)&vtnode);
    if (!node)
        return NULL;
    else
        return (view_tree_node_t* )(node->value);
}

void view_tree_delete(view_tree_t *tree, const char *name)
{
    view_tree_node_t vtnode;
    strcpy(vtnode.name, name);
    vtnode.vs = NULL;
    rbnode_t *del_node = rbtree_delete(tree->rbtree, (void *)&vtnode);
    if (!del_node)
        return ;
	view_tree_node_t *vtnodeptr = del_node->value;
	view_stats_destory(vtnodeptr->vs);
	free(del_node);
	free(vtnodeptr);
	tree->count--;
}

void destory_func(void *node)
{
    rbnode_t *rbnode = (rbnode_t *)node;
    view_tree_node_t *vtnode = (view_tree_node_t *)(rbnode->value);
    view_stats_destory(vtnode->vs);
    free(vtnode);
    free(rbnode);
}

void view_tree_destory(view_tree_t *tree)
{
    rbtree_destory(tree->rbtree, destory_func);
	free(tree);
}
