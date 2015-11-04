#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <stdbool.h>
#include <signal.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <assert.h>
#include <pthread.h>
#include "log_utils.h"
#include "log_config.h"
#include "log_name_tree.h"
#include "log_view_tree.h"
#include "zip_addr.h"
#include "zip_socket.h"
#include "statsmessage.pb-c.h"
#include "dig_radix_tree.h"
#include "adlist.h"
#include "log_topn.h"
#include "dig_logger.h"
#define RECORD_LEN 1024
#define PART_LOG_LEN 768
#define SP_NUM 50

#define BUFF_SIZE 1024

void *command_handler(void *args);
void *qps_thread(void *args);

void house_keeper_destroy(house_keeper_t *keeper);
void add_init_view(house_keeper_t *keeper, const char *name);
house_keeper_t *keeper;
struct house_keeper {
    float qps;
    float success_rate;
    uint64_t count;
    uint64_t valid_count;
    uint64_t skip_count;
    view_tree_t *views;
    radix_tree_t *radix_tree;     
    config_t *conf;
    socket_t socket;
    pthread_t tid;
    pthread_t qps_tid;
	pthread_mutex_t 	mlock;
    char config_file[256];
    int max_pos;
};

static inline
int name_compare(const void *value1, const void *value2)
{
    assert(value1 && value2);
    const char *name1 = ((view_tree_node_t *)value1)->name;
    const char *name2 = ((view_tree_node_t *)value2)->name;

    return strcmp(name1, name2);
}


void get_monitor_logfile(house_keeper_t *keeper, char *filename)
{
    strcpy(filename, keeper->conf->monitor_log_file);
}

void delete_radix_node( void *data) 
{
    if (data)
    {
       //printf("view is %s\n",  (char *)data);
       free(data);
    }
}

void free_list_node(void *node)
{
    if (node)
        free(node);
}

int get_inode(struct stat *ptr)
{
    return (int)ptr->st_ino;
}

int get_hit_status(const char *status) 
{
    if (0 == strcmp(status, " HIT") )
    {
       return 0;
    }
    else if (0 == strcmp(status, "MISS") )
    {
       return 1;
    }
    else if (0 == strcmp(status, "EXPIRED"))
    {
       return 2;
    }
    return -1;
}
void log_handle(house_keeper_t *keeper, const char *key, const char *ip, const char *view_id, const char *status, uint32_t size, int hit_status, const char *logline, int logornot)
{
    ASSERT(keeper && key && ip ,"invalid pointer\n");
    //sample 
    if (keeper->conf->sample_rate > 0.00001 && keeper->conf->sample_rate < 0.99999) 
    {
        float sample_rate = 1.0 * (keeper->count - keeper->skip_count) / keeper->count;
        if (sample_rate > keeper->conf->sample_rate)
        {
             keeper->skip_count++;
             return;
        }
    }
	pthread_mutex_lock(&keeper->mlock);
    view_tree_node_t *vtnode = view_tree_find(keeper->views, key); 
	view_stats_t *vs = NULL;
    if (NULL == vtnode)
    {
        vs = view_stats_create(key);
        if (NULL == vs)
        {
            printf("not  enough memory\n");
	        pthread_mutex_unlock(&keeper->mlock);
            return;
        }
        view_tree_insert(keeper->views, key, vs);
		printf("key %s create stats vs %p\n", key, vs);
    }
    else 
    {
		vs = vtnode->vs;
		lru_list_move_to_first(keeper->views->lrulist, vtnode->ptr); 
	}

    if (logornot && vs->logger == NULL)
    {
        vs->logger = create_logger(keeper->conf->home_dir, strchr(key, ':')+1, keeper->conf->max_file_count, keeper->conf->file_size_limit); 
        if (!vs->logger)
        {
            printf("create logger failed\n");
        }
    }

    view_stats_bandwidth_increment(vs, size);
    if (hit_status == 0) {
        view_stats_hit_bandwidth_increment(vs, size); 
    }
    else if (hit_status == 1)
    {
        view_stats_lost_bandwidth_increment(vs, size); 
    }

    if (logornot)
    {
        log_msg(vs->logger, logline);
    }
	pthread_mutex_unlock(&keeper->mlock);
}
char *get_view_id_base_on_ip(radix_tree_t *radix_tree, char *clientip)
{
    void *id_node = radix_tree_find_str(radix_tree, clientip); 
    return (char *)id_node;
}

time_t get_timet(const char *timestr, const char *format)
{
    struct tm tmp;
    strptime(timestr, format , &tmp); 
    return mktime(&tmp);
}
void handle_string_log(house_keeper_t *keeper , char *line)
{
    char *str2, *saveptr2;
    int i;
    keeper->count++;
    char query_log[RECORD_LEN];
    strcpy(query_log, line);
    char *ptr[100] = {NULL};
    for (i = 0 ,str2 = query_log; i < keeper->max_pos ; i++, str2 = NULL)
    {
        ptr[i] = strtok_r(str2, SP, &saveptr2);
        if (ptr[i] == NULL)
            break;
    }
    if (i < keeper->max_pos) 
        return;
    char *hit_status = saveptr2 + strlen(saveptr2) - 5;
    hit_status[strlen(hit_status) - 1] = '\0'; // remote \n
    int hit_value = get_hit_status(hit_status);
    config_t *conf = keeper->conf;
    //printf("clientip: %s, domain: %s, status: %s, content: %s\n", ptr[conf->client_pos], ptr[conf->domain_pos], ptr[conf->status_pos], ptr[conf->content_pos]);

    int32_t size = atoi(ptr[conf->content_pos]);
    if (size <= 0 ) 
      return; 

    /*if (ptr[conf->time_pos][0] == '[')
    {
        time_t ts = get_timet(ptr[conf->time_pos] + 1, conf->timeformat);
        //printf("unix time %ld\n", ts);
    }
    */

    ptr[conf->status_pos][strlen(ptr[conf->status_pos]) - 1] = '\0';
    strcpy(ptr[conf->status_pos], ptr[conf->status_pos] + 1);
    if (hit_value != -1 && strcmp(ptr[conf->client_pos], "-") != 0 && atoi(ptr[conf->status_pos]) > 100 )
    {
       
       char *view_id = get_view_id_base_on_ip(keeper->radix_tree, ptr[conf->client_pos]);
       if ( view_id )
       {
       keeper->valid_count++;
       char temp_char[128];
       sprintf(temp_char, "view:%s:%s", view_id, ptr[conf->domain_pos]);
       log_handle(keeper, temp_char, ptr[conf->client_pos], view_id, ptr[conf->status_pos], size, hit_value, NULL, 0);
       } 
       else
       {
           printf("not find view of ip %s\n", ptr[conf->client_pos]);
       }
       char temp_char[128];
       sprintf(temp_char, "name:%s", ptr[conf->domain_pos]);
       log_handle(keeper, temp_char, ptr[conf->client_pos], view_id, ptr[conf->status_pos], size, hit_value, line, conf->enable_auto);

       sprintf(temp_char, "code:%s", ptr[conf->status_pos]);
       log_handle(keeper, temp_char, ptr[conf->client_pos], view_id, ptr[conf->status_pos], size, hit_value, NULL, 0);
    }
    else {
       printf("format error %s\n", line);
    }
}

void house_keeper_destroy(house_keeper_t *keeper)
{
    int ret = pthread_cancel(keeper->tid);
    ret = pthread_cancel(keeper->qps_tid);
    socket_set_addr_reusable(&keeper->socket);
    socket_close(&keeper->socket);
    view_tree_destory(keeper->views);
    radix_tree_delete(keeper->radix_tree);
    config_destroy(keeper->conf);

	pthread_mutex_destroy(&keeper->mlock);
    free(keeper);
}

void house_keeper_reload(house_keeper_t *keeper)
{
    radix_tree_delete(keeper->radix_tree);
    config_destroy(keeper->conf);
    config_t *conf = config_create(keeper->config_file);
    ASSERT(conf != NULL, "create config failed\n");
    keeper->conf = conf;
    keeper->radix_tree = radix_tree_create(delete_radix_node); 
    ASSERT(keeper->radix_tree != NULL, "create radix tree failed\n");

    int ret= read_iplib_from_file(keeper->radix_tree, keeper->conf->iplib_file);
    ASSERT( ret == 0, "load ip library from %s failed", keeper->conf->iplib_file);
}


void house_keeper_clear_stats(house_keeper_t *keeper)
{
    view_tree_destory(keeper->views);
    keeper->views = view_tree_create(name_compare);
    keeper->count = 0;
    keeper->skip_count = 0;
}

void add_init_view(house_keeper_t *keeper, const char *name)
{
    view_stats_t *vs = view_stats_create(name);
    ASSERT(vs, "create view %s stats failed\n", name);
    int ret = view_tree_insert(keeper->views, name, vs);
    ASSERT(ret == 0, "insert view %s node failed\n", name);
}

int read_iplib_from_file(radix_tree_t *radix_tree, const char *filename)
{   
    FILE *fp = fopen(filename, "r") ;
    if (fp == NULL)
    {
        printf("open file %s failed\n", filename);
        return 1;
    }
    char line[256] = {'\0'};
    while (fgets(line, 256, fp) != NULL) 
    {
        char *p = NULL;
        char *subnet = NULL;
        p = strtok(line, " "); 
        int i = 0;
        while (p)
        {
            if (i == 0) 
               subnet = p; 
            else if (i == 1)
            {
               break; 
            }
            i++;
            p = strtok(NULL, " ");
        }
        if (subnet != NULL && i == 1) 
        {
            p[strlen(p) - 1] = '\0';
            char *view_id = malloc(sizeof(char ) * 4); 
            strcpy(view_id, p) ;
            //printf("view_id %s, network %s\n", view_id, subnet);
            radix_tree_insert_subnet(radix_tree, subnet, view_id);
        }
        
    }

    fclose(fp);
    return 0;
}

house_keeper_t *house_keeper_create(const char *config_file)
{
    config_t *conf = config_create(config_file);
    ASSERT(conf != NULL, "create config failed\n");

    house_keeper_t *keeper = malloc(sizeof(house_keeper_t));
    ASSERT(keeper, "create house keeper failed\n");
    keeper->count = 0;
    keeper->skip_count = 0;
    keeper->qps = 0.0;
    keeper->success_rate = 1.0;
    keeper->conf = conf;
    keeper->max_pos = conf->content_pos + 1;
    keeper->views = view_tree_create(name_compare);

    socket_open(&keeper->socket, AF_INET, SOCK_STREAM, 0);
    addr_t addr;
    addr_init(&addr, conf->server_ip, conf->port);
    socket_set_addr_reusable(&keeper->socket);
    int ret = socket_bind(&keeper->socket, &addr);
    ASSERT(ret == 0, "socket bind port failed\n");

    keeper->radix_tree = radix_tree_create(delete_radix_node); 
    ASSERT(keeper->radix_tree != NULL, "create radix tree failed\n");

    ret= read_iplib_from_file(keeper->radix_tree, keeper->conf->iplib_file);
    ASSERT( ret == 0, "load ip library from %s failed", keeper->conf->iplib_file);

    ret = pthread_create(&keeper->tid, NULL, command_handler, keeper);
    ASSERT(ret == 0, "create command handler thread failed\n");

    ret = pthread_create(&keeper->qps_tid, NULL, qps_thread, keeper);
    ASSERT(ret == 0, "create command handler thread failed\n");
	pthread_mutex_init(&keeper->mlock,NULL);

    strcpy(keeper->config_file, config_file);

    return keeper;
}
unsigned int return_all_views_qps(house_keeper_t *keeper,  char **buff)
{
    int n = keeper->views->count;
    StatsReply reply = STATS_REPLY__INIT;
    reply.key = "qps_all";
    char **pdata = malloc(sizeof (char *) * n);

    int *pqps = malloc(sizeof (int32_t ) * n);
      
    rbnode_t *node;
    int i = 0;
    RBTREE_FOR(node, rbnode_t *, keeper->views->rbtree)
    {
       view_tree_node_t *tnode = (view_tree_node_t *)(node->value);
       
       if (strlen(tnode->name) == 0 )
           continue;
       pdata[i] = tnode->name;
       pqps[i] = tnode->vs->qps * 1000;

       i++;
    }
    reply.n_name = i;
    reply.name = pdata;
    reply.n_count = i;
    reply.count = pqps;
    unsigned int len = stats_reply__get_packed_size(&reply);
    char *result_ptr = malloc(sizeof(char) *len);
    stats_reply__pack(&reply, result_ptr);
    *buff = result_ptr;
    free(pdata);
    free(pqps);
    return len;
}

unsigned int return_all_views_bandwidth(house_keeper_t *keeper,  char **buff)
{
    int n = keeper->views->count;
    StatsReply reply = STATS_REPLY__INIT;
    reply.key = "bandwidth_all";
    char **pdata = malloc(sizeof (char *) * n);

    int *pbandwidth = malloc(sizeof (int32_t ) * n);
      
    rbnode_t *node;
    int i = 0;
    RBTREE_FOR(node, rbnode_t *, keeper->views->rbtree)
    {
       view_tree_node_t *tnode = (view_tree_node_t *)(node->value);
       
       if (strlen(tnode->name) == 0 )
           continue;
       unsigned int bw = view_stats_bandwidth(tnode->vs);
       if ( bw != 0) 
       {
           pdata[i] = tnode->name;
           pbandwidth[i] = bw;
           view_stats_set_bandwidth(tnode->vs, 0);
           i++;
       }
    }
    reply.n_name = i;
    reply.name = pdata;
    reply.n_count = i;
    reply.count = pbandwidth;
    unsigned int len = stats_reply__get_packed_size(&reply);
    char *result_ptr = malloc(sizeof(char) *len);
    stats_reply__pack(&reply, result_ptr);
    *buff = result_ptr;
    free(pdata);
    free(pbandwidth);
    return len;
}

unsigned int return_all_stats(house_keeper_t *keeper,  char **buff)
{
    int n = keeper->views->count;
    StatsReply reply = STATS_REPLY__INIT;
    reply.key = "stats_all";
    char **pdata = malloc(sizeof (char *) * n);

    int *prate= malloc(sizeof (int32_t ) * n);
    int *paccess_count = malloc(sizeof (int32_t ) * n);
    int *pbandwidth = malloc(sizeof (int32_t ) * n);
    int *phit_count = malloc(sizeof (int32_t ) * n);
    int *phit_bandwidth = malloc(sizeof (int32_t ) * n);
    int *plost_count = malloc(sizeof (int32_t ) * n);
    int *plost_bandwidth = malloc(sizeof (int32_t ) * n);
      
    rbnode_t *node;
    int i = 0;
    RBTREE_FOR(node, rbnode_t *, keeper->views->rbtree)
    {
       view_tree_node_t *tnode = (view_tree_node_t *)(node->value);
       
       if (strlen(tnode->name) == 0 )
           continue;
       unsigned int bw = view_stats_bandwidth(tnode->vs);
       if ( bw != 0) 
       {
           pdata[i] = tnode->name;
           pbandwidth[i] = bw;
           prate[i] = tnode->vs->rate;
           paccess_count[i] = tnode->vs->count;
           phit_count[i] = tnode->vs->hit_count;
           phit_bandwidth[i] = tnode->vs->hit_bandwidth;
           plost_count[i] = tnode->vs->lost_count;
           plost_bandwidth[i] = tnode->vs->lost_bandwidth;
           view_stats_init(tnode->vs);
           i++;
       }
    }
    reply.n_name = i;
    reply.name = pdata;
    reply.n_rate = i;
    reply.rate = prate;
    reply.n_access_count = i;
    reply.access_count = paccess_count;
    reply.n_bandwidth = i;
    reply.bandwidth = pbandwidth;
    reply.n_hit_count = i;
    reply.hit_count = phit_count;
    reply.n_hit_bandwidth= i;
    reply.hit_bandwidth = phit_bandwidth;
    reply.n_lost_count = i;
    reply.lost_count = plost_count;
    reply.n_lost_bandwidth = i;
    reply.lost_bandwidth = plost_bandwidth;

    unsigned int len = stats_reply__get_packed_size(&reply);
    char *result_ptr = malloc(sizeof(char) *len);
    stats_reply__pack(&reply, result_ptr);
    *buff = result_ptr;
    free(pdata);
    free(pbandwidth);
    free(paccess_count);
    free(prate);
    free(phit_count);
    free(phit_bandwidth);
    free(plost_count);
    free(plost_bandwidth);
    return len;
}
unsigned int return_all_views(house_keeper_t *keeper,  char **buff)
{
    int n = keeper->views->count;
    StatsReply reply = STATS_REPLY__INIT;
    reply.key = "views";
    char **pdata = malloc(sizeof (char *) * n);
      
    rbnode_t *node;
    int i = 0;
    RBTREE_FOR(node, rbnode_t *, keeper->views->rbtree)
    {
       view_tree_node_t *tnode = (view_tree_node_t *)(node->value);
       
       if (strlen(tnode->name) == 0 )
           continue;
       pdata[i] = tnode->name;
       i++;
    }
    reply.n_name = i;
    reply.name = pdata;
    unsigned int len = stats_reply__get_packed_size(&reply);
    char *result_ptr = malloc(sizeof(char) *len);
    stats_reply__pack(&reply, result_ptr);
    *buff = result_ptr;
    free(pdata);
    return len;
}

unsigned int view_not_found(house_keeper_t *keeper, char **buff)
{
    StatsReply reply = STATS_REPLY__INIT;
    reply.key = "error";
    reply.maybe = "view not find";
    unsigned int len = stats_reply__get_packed_size(&reply);
    char *result_ptr = malloc(sizeof(char) *len);
    stats_reply__pack(&reply, result_ptr);
    *buff = result_ptr;
    return len;
}


unsigned int flush_stats(house_keeper_t *keeper, char **buff)
{
    house_keeper_clear_stats(keeper);
    StatsReply reply = STATS_REPLY__INIT;
    reply.key = "flush";
    reply.maybe = "success";
    unsigned int len = stats_reply__get_packed_size(&reply);
    char *result_ptr = malloc(sizeof(char) *len);
    stats_reply__pack(&reply, result_ptr);
    *buff = result_ptr;
    return len;
}

unsigned int reload(house_keeper_t *keeper, char **buff)
{
    house_keeper_reload(keeper);
    StatsReply reply = STATS_REPLY__INIT;
    reply.key = "reload";
    reply.maybe = "success";
    unsigned int len = stats_reply__get_packed_size(&reply);
    char *result_ptr = malloc(sizeof(char) *len);
    stats_reply__pack(&reply, result_ptr);
    *buff = result_ptr;
    return len;
}


uint32_t absolute_diff(uint32_t first, uint32_t second)
{
    if (second >= first)
        return second - first;
    else
        return UINT32_MAX - first + second;
}
void *qps_thread(void *args)
{
    house_keeper_t *keeper = (house_keeper_t *)args;
    rbnode_t *node;

    int timediff = 10;
    int clear_count = 0;
    while (1)
    {
        pthread_mutex_lock(&keeper->mlock);
        int i = 0;
        RBTREE_FOR(node, rbnode_t *, keeper->views->rbtree)
        {
           view_tree_node_t *tnode = (view_tree_node_t *)(node->value);
           
           if (strlen(tnode->name) == 0 )
               continue;
           view_stats_t *vs = tnode->vs;
           vs->qps = absolute_diff(vs->last_count, vs->count)*1.0 / timediff;
           vs->rate = absolute_diff(vs->last_bandwidth, vs->bandwidth) * 1.0 /timediff;
           vs->last_count = vs->count;
        }

        pthread_mutex_unlock(&keeper->mlock);
        sleep(timediff);
    }
}

typedef struct socket_thd_data {
    socket_t *sock;
    house_keeper_t *keeper;
}socket_thd_data_t;

void *socket_thread(void *args)
{
    socket_thd_data_t *std = (socket_thd_data_t *)args;
    house_keeper_t *keeper = std->keeper;
    socket_t *pending_socket = std->sock;
    StatsRequest *request;
    while (1)
    {
        char temp[10];
        if (socket_read(pending_socket, temp, 4) <= 0)
            break;
        unsigned int prefix_len = ntohl(*((int *)&temp));
        printf("read command prefix length was %d\n", prefix_len);
        
        char *buffer = malloc(sizeof(char ) *prefix_len);
        unsigned int len = socket_read(pending_socket, buffer, prefix_len);
        request = stats_request__unpack(NULL, len, buffer);
        printf("command==> key:%s, view:%s, topn:%d\n", request->key, request->view, request->topn);
        fflush(stdout);
        char *result_ptr = NULL;
        int topn = request->topn;
        if (topn == 0)
            topn = 1000;
        int rtn_msg_len = 0;
        pthread_mutex_lock(&keeper->mlock);
        if (strcmp(request->key, "views") == 0)
        {
            rtn_msg_len = return_all_views(keeper, &result_ptr);
		}
        else if (strcmp(request->key, "stats_all") == 0)
        {
            rtn_msg_len = return_all_stats(keeper, &result_ptr);
        }
        else if (strcmp(request->key, "bandwidth_all") == 0)
        {
            rtn_msg_len = return_all_views_bandwidth(keeper, &result_ptr);
        }
        else if (strcmp(request->key, "qps_all") == 0)
        {
            rtn_msg_len = return_all_views_qps(keeper, &result_ptr);
        }
        else if (strcmp(request->key, "flush") == 0)
        {
            rtn_msg_len = flush_stats(keeper, &result_ptr);
		}
        else if (strcmp(request->key, "reload") == 0)
        {
            rtn_msg_len = reload(keeper, &result_ptr);
		}
        else 
        {
            view_tree_node_t *vtnode = view_tree_find(keeper->views, request->view); 
            if (vtnode == NULL)
            {
                   rtn_msg_len = view_not_found(keeper, &result_ptr);
            }
            else
            {
	           view_stats_t *vs = vtnode->vs;
		       ASSERT(vs, "view %s in view tree, but has not corresponding view stats", request->view);
               if (strcmp(request->key, "domaintopn") == 0)
               {
                   rtn_msg_len = view_stats_name_topn(vs, topn, &result_ptr);
               }else if (strcmp(request->key, "iptopn") == 0)
               {
                   rtn_msg_len = view_stats_ip_topn(vs, topn, &result_ptr);
               }else if (strcmp(request->key, "qps") == 0)
               {
                   rtn_msg_len = view_stats_get_qps(vs, &result_ptr);
               }else if (strcmp(request->key, "success_rate") == 0)
               {
                   rtn_msg_len = view_stats_get_success_rate(vs, &result_ptr);
               }else if (strcmp(request->key, "bandwidth") == 0)
               {
                   rtn_msg_len = view_stats_get_bandwidth(vs, &result_ptr);
               }
            }
        }
        pthread_mutex_unlock(&keeper->mlock);
        prefix_len = htonl(rtn_msg_len);
        printf("return total len: %d\n", rtn_msg_len);
        fflush(stdout);
        
        stats_request__free_unpacked(request, NULL);
        socket_write(pending_socket, (char *)&prefix_len, 4);
        socket_write(pending_socket, result_ptr, rtn_msg_len);
        free(result_ptr);
        free(buffer);
    }
    free(std);
    socket_close(pending_socket);
    printf("socket closed\n");
}

void *command_handler(void *args)
{
    house_keeper_t *keeper = (house_keeper_t *)args;
    socket_listen(&keeper->socket, 5);
    while (1)
    {
        socket_t *pending_socket =  socket_accept(&keeper->socket);
        addr_t tempaddr;
        socket_get_peer_addr(pending_socket, &tempaddr);
        char clientip[128];
        addr_get_ip(&tempaddr, clientip);
        printf("accept socket ip: %s, port: %d\n", clientip, addr_get_port(&tempaddr));
        fflush(stdout);
        socket_thd_data_t *std = malloc(sizeof(socket_thd_data_t));
        std->keeper= keeper;
        std->sock = pending_socket;
        pthread_t pid;
        pthread_create(&pid, NULL, socket_thread, std);
    }
}

