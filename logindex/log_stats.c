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
#include <assert.h>
#include "log_utils.h"
#include "log_config.h"
#include "statsmessage.pb-c.h"
#include "log_stats.h"
#include "log_domain_tree.h"
#include "dig_radix_tree.h"
#include "log_view_tree.h"
#define SP_NUM 50

enum  {
   HIT = 0,
   MISS = 1
};

enum {
   NGINX = 1,
   CLOUDCACHE = 2
};
void write_stats_to_file(house_keeper_t *keeper, const char *filename);
void house_keeper_destroy(house_keeper_t *keeper);
void add_init_view(house_keeper_t *keeper, const char *name);
house_keeper_t *keeper;

static inline
int name_compare(const void *value1, const void *value2)
{
    assert(value1 && value2);
    const char *name1 = ((view_tree_node_t *)value1)->name;
    const char *name2 = ((view_tree_node_t *)value2)->name;

    return strcmp(name1, name2);
}

static void delete_radix_node( void *data) 
{
    if (data)
    {
       free(data);
    }
}

static void free_list_node(void *node)
{
    if (node)
        free(node);
}

static int get_hit_status(const char *status) 
{
    if (0 == strcmp(status, "HIT") )
    {
       return HIT;
    }
    else if (0 == strcmp(status, "MISS") )
    {
       return MISS;
    }
    else if (0 == strcmp(status, "EXPIRED"))
    {
       return 2;
    }
    else if (0 == strcmp(status, "COLLAPSE"))
    {
       return MISS;
    }
    else if (0 == strcmp(status, "UPDATING"))
    {
       return 4;
    }
    else if (0 == strcmp(status, "STALE"))
    {
       return 5;
    }
    return -1;
}

static int get_cloudcache_hit_status(const char *status) 
{
    if (0 == strcmp(status, "CLOUD_CACHE_HIT_DISK") )
    {
       return HIT;
    }
    else if (0 == strcmp(status, "CLOUD_CACHE_MISS") )
    {
       return MISS;
    }
    else if (0 == strcmp(status, "CLOUD_CACHE_EXPIRED"))
    {
       return 2;
    }
    return -1;
}
void log_handle(house_keeper_t *keeper, const char *key, uint32_t size, int hit_status, time_t tt, const char *logline, int logornot)
{
    ASSERT(keeper && key,"invalid pointer\n");
    view_tree_node_t *vtnode = view_tree_find(keeper->views, key); 
	view_stats_t *vs = NULL;
    if (NULL == vtnode)
    {
        vs = view_stats_create(key);
        if (NULL == vs)
        {
            printf("not  enough memory\n");
            return;
        }
        view_tree_insert(keeper->views, key, vs);
    }
    else
    {
        vs = vtnode->vs;
    }

    if (logornot && vs->log_file_fp == NULL)
    {
        char *domain_name = strchr(key, ':') + 1;
        char domain_dir[320];
        sprintf(domain_dir, "%s/%s", keeper->conf->home_dir, domain_name);
        if (access(domain_dir, F_OK) != 0)
        {
            if (mkdir(domain_dir, 0755) != 0)
            {
                printf("create dir %s failed\n", domain_dir);
                return;
            }
        }
        char file_prefix_log[512];
        char file_prefix_idx[512];
        struct tm *t2 = localtime(&keeper->last_log_cut_time);
        char temp_tm_str[32];
        strftime(temp_tm_str, 32, "%Y%m%d%H", t2);

        sprintf(file_prefix_log, "%s/%s/%s_%s_%s.log", keeper->conf->home_dir, domain_name, keeper->hostname, domain_name, temp_tm_str);
        sprintf(file_prefix_idx, "%s/%s/%s_%s_%s.idx", keeper->conf->home_dir, domain_name, keeper->hostname, domain_name, temp_tm_str);
        vs->log_file_fp = fopen(file_prefix_log, "a");
        if (!vs->log_file_fp)
        {
            printf("open log file %s failed\n", file_prefix_log);
        }
        vs->idx_file_fp = fopen(file_prefix_idx, "a");
        if (!vs->idx_file_fp)
        {
            printf("open log file %s failed\n", file_prefix_idx);
        }
        vs->last_fp_offset = ftell(vs->log_file_fp);
    }

    view_stats_bandwidth_increment(vs, size);
    if (hit_status == 0) 
    {
        view_stats_hit_bandwidth_increment(vs, size); 
    }
    else if (hit_status == 1)
    {
        view_stats_lost_bandwidth_increment(vs, size); 
    }

    if (logornot)
    {

        if (vs->last_time != 0 )
        {
            if (tt == vs->last_time) //same seconds or new file
            {
                fwrite(logline, strlen(logline), 1, vs->log_file_fp);
            }
            else
            {
                int end_pos = ftell(vs->log_file_fp);
                fprintf(vs->idx_file_fp, "%ld %u %u\n", vs->last_time, vs->last_fp_offset, end_pos); 
                vs->last_time = tt;
                vs->last_fp_offset = end_pos;
                fwrite(logline, strlen(logline), 1, vs->log_file_fp);
            }
        }
        else //first
        {
            fwrite(logline, strlen(logline), 1, vs->log_file_fp);
            vs->last_time = tt;
        }

        vs->latest_time = tt;
    }

}
char *get_view_id_base_on_ip(radix_tree_t *radix_tree, char *clientip)
{
    void *id_node = radix_tree_find_str(radix_tree, clientip); 
    return (char *)id_node;
}

time_t my_mktime(struct tm *ts, char *tzinfo)
{
   char hour_str[3];
   char min_str[3];
   memcpy(hour_str, tzinfo +1, 2);
   memcpy(min_str, tzinfo +3, 2);
   hour_str[2] = '\0';
   min_str[2] = '\0';
   int hour = atoi(hour_str);
   int min = atoi(min_str);
   if (tzinfo[0] == '+')
      return kernel_mktime(ts) - hour * 3600 - min * 60;
   else if (tzinfo[0] == '-')
      return kernel_mktime(ts) + hour * 3600 + min * 60;
   return kernel_mktime(ts);
}

time_t get_timet_and_tm(const char *timestr, char *tzinfo, const char *format, struct tm *ts)
{
    strptime(timestr, format, ts); 
    return my_mktime(ts, tzinfo);
}

void stats_output_by_time(house_keeper_t *keeper)
{
    char filename[256];
    sprintf(filename, "%s/stats-%s", keeper->conf->stats_dir, keeper->timestamp_str);
    write_stats_to_file(keeper, filename);
}

void file_cut_by_time(house_keeper_t *keeper)
{
    rbnode_t *node;
    RBTREE_FOR(node, rbnode_t *, keeper->views->rbtree)
    {
       view_tree_node_t *tnode = (view_tree_node_t *)(node->value);
       if (strncmp(tnode->name, "name:", 5) == 0) 
       {
            view_stats_t *vs = tnode->vs;
            if (vs->idx_file_fp)
            {
               int end_pos = ftell(vs->log_file_fp);
               fprintf(vs->idx_file_fp, "%ld %u %u\n", vs->latest_time, vs->last_fp_offset, end_pos); 
               fclose(vs->idx_file_fp);
               vs->idx_file_fp = NULL;
               vs->last_time = 0; 
            }
            if (vs->log_file_fp)
            {
               fclose(vs->log_file_fp);
               vs->log_file_fp = NULL;
            }
       }
    }
}
bool ip_is_valid(const char *ip)
{
    int dot_count = 0;
    char *p = strchr(ip, '.');
    while (p) {
       dot_count++;
       p = strchr(p+1, '.');
    }
    if (dot_count == 3)
       return true;

    return false;
}

int ip_is_skipped(radix_tree_t *radix_tree, char *ip)
{
    void *id_node = radix_tree_find_str(radix_tree, ip); 
    if (id_node)
       return 1;
    return 0;
}

int domain_is_skipped(domain_tree_t *tree, char *domain)
{
    void *id_node = domain_tree_exactly_find(tree, domain); 
    if (id_node)
       return 1;

    return 0;
}

char *domain_is_wildcard(domain_tree_t *tree, char *domain)
{
    node_data_t *data = domain_tree_closely_find(tree, domain); 
    if (data)
       return data->fqdn;

    return NULL;
}


int handle_time_part(house_keeper_t *keeper, char *time_log_part, char *tzone,  time_t *tt_sec)
{
    struct tm ts;
    config_t *conf = keeper->conf;
    
    if (time_log_part[0] == '[' && tzone[strlen(tzone)-1] == ']')
    {
        tzone[strlen(tzone) -1] = '\0';
        time_t tt = get_timet_and_tm(time_log_part + 1, tzone, conf->timeformat, &ts);
        *tt_sec = tt;
        if (keeper->lasttime == 0 )
        {

            keeper->lasttime = tt - tt % conf->stats_interval;
            keeper->last_log_cut_time = tt - tt % conf->file_cut_interval;
            char timestr[32];
            strftime(timestr, 32, "%Y%m%d%H%M", &ts);
            strcpy(keeper->timestamp_str, timestr);
        }
        else
        {

            int minutes = tt - tt % conf->stats_interval;

            if ( minutes != keeper->lasttime )
            {
                stats_output_by_time(keeper);
                keeper->lasttime = minutes; 
                char timestr[32];
                strftime(timestr, 32, "%Y%m%d%H%M", &ts);
                strcpy(keeper->timestamp_str, timestr);
            }

            int hours = tt - tt % conf->file_cut_interval;

            if ( hours != keeper->last_log_cut_time)
            {
                file_cut_by_time(keeper);
                keeper->last_log_cut_time = hours; 
            }
        }
        return 0;
    }
    return 1;
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

    config_t *conf = keeper->conf;

    if (!ip_is_valid(ptr[conf->client_pos]))
    {
        printf("ip %s is invalid\n", ptr[conf->client_pos]);
        return;
    }

    if (conf->cache_server_type == CLOUDCACHE) 
    {
        char *p = strchr(ptr[conf->domain_pos], '/');  
        if (!p)
        {
            printf("wrong url format %s\n", ptr[conf->domain_pos]);
            return;
        }
        ptr[conf->domain_pos] = p + 2;
        p = strchr(p+2, '/'); 
        if (!p)
        {
            printf("wrong uri format %s\n", ptr[conf->domain_pos]);
            return;
        }
        *p = '\0';
    }

    int ret = ip_is_skipped(keeper->skip_ip_tree, ptr[conf->client_pos]); 
    if (ret)
    {
        printf("this ip %s skipped\n", ptr[conf->client_pos]);
        return;
    }

    ret = domain_is_skipped(keeper->skip_domain_tree, ptr[conf->domain_pos]); 
    if (ret)
    {
        printf("this domain %s skipped\n", ptr[conf->domain_pos]);
        return;
    }

    int hit_value = -1;
    if (conf->hit_pos == -1)
    {
        char *hit_status = strrchr(saveptr2, ' ');
        if (hit_status)
        {
            hit_status += 1;
            hit_status[strlen(hit_status) - 1] = '\0'; // remove \n
            hit_value = get_hit_status(hit_status);
        }
    }
    else
    {
        hit_value = get_cloudcache_hit_status(ptr[conf->hit_pos]);
    }
    //printf("clientip: %s, domain: %s, status: %s, content: %s\n", ptr[conf->client_pos], ptr[conf->domain_pos], ptr[conf->status_pos], ptr[conf->content_pos]);

    time_t tt_sec;
    ret = handle_time_part(keeper, ptr[conf->time_pos], ptr[conf->tzone_pos], &tt_sec);
    if (ret)
    {
       printf("timestamp format error\n");
       return;
    }

    /*"200" => 200*/
    if (ptr[conf->status_pos][0] == '\"') //nginx
    {
        ptr[conf->status_pos][strlen(ptr[conf->status_pos]) - 1] = '\0';
        ptr[conf->status_pos] = ptr[conf->status_pos] + 1;
    }

    if (hit_value != -1 && atoi(ptr[conf->status_pos]) >= 100 )
    {
       
       char *view_id = get_view_id_base_on_ip(keeper->radix_tree, ptr[conf->client_pos]);
       
       char *wildcard_domain = domain_is_wildcard(keeper->wildcard_tree, ptr[conf->domain_pos]);
       if (wildcard_domain)
       {
           printf("this domain is wildcard %s, ori:%s\n", wildcard_domain, ptr[conf->domain_pos]);
           ptr[conf->domain_pos] = wildcard_domain;
       }

       int size = atoi(ptr[conf->content_pos]);

       if ( view_id )
       {
           char temp_char[128];
           sprintf(temp_char, "view:%s:%s", view_id, ptr[conf->domain_pos]);
           log_handle(keeper, temp_char, size, hit_value, tt_sec, NULL, 0);
       } 
       else
       {
           printf("not find view of ip %s\n", ptr[conf->client_pos]);
           char temp_char[128];
           sprintf(temp_char, "view:-1\t-1:%s", ptr[conf->domain_pos]);
           log_handle(keeper, temp_char, size, hit_value, tt_sec, NULL, 0);
       }
       char temp_char[128];
       sprintf(temp_char, "name:%s", ptr[conf->domain_pos]);
       log_handle(keeper, temp_char, size, hit_value, tt_sec, line, conf->enable_auto_log);

       sprintf(temp_char, "code:%s:%s",  ptr[conf->status_pos], ptr[conf->domain_pos]);
       log_handle(keeper, temp_char,  size, hit_value, tt_sec, NULL, 0);
    }
    else 
    {
       printf("format error %s\n", line);
    }
}

void house_keeper_destroy(house_keeper_t *keeper)
{
    if (keeper->lasttime != 0)
       stats_output_by_time(keeper);
    file_cut_by_time(keeper);
    view_tree_destory(keeper->views);
    radix_tree_delete(keeper->radix_tree);
    radix_tree_delete(keeper->skip_ip_tree);
    domain_tree_destroy(keeper->wildcard_tree);
    domain_tree_destroy(keeper->skip_domain_tree);
    if (keeper->conf->use_breakpoint_record)
        fclose(keeper->breakpoint_fp);

    config_destroy(keeper->conf);

    free(keeper);
}

void house_keeper_reload(house_keeper_t *keeper)
{
    return;
    radix_tree_delete(keeper->radix_tree);
    radix_tree_delete(keeper->skip_ip_tree);
    domain_tree_destroy(keeper->wildcard_tree);
    domain_tree_destroy(keeper->skip_domain_tree);

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

house_keeper_t *house_keeper_create(config_t *conf)
{
    ASSERT(conf, "config NULL\n");
    house_keeper_t *keeper = malloc(sizeof(house_keeper_t));
    ASSERT(keeper, "create house keeper failed\n");
    keeper->count = 0;
    keeper->skip_count = 0;
    keeper->conf = conf;
    keeper->max_pos = config_max_pos(conf) + 1;
    keeper->views = view_tree_create(name_compare);
    keeper->lasttime = 0;

    if (gethostname(keeper->hostname, 255) == 0)
    {
        printf("hostname is %s\n", keeper->hostname);
    }
    else
    {
        strcpy(keeper->hostname, "127.0.0.1");
    }

    keeper->radix_tree = radix_tree_create(delete_radix_node); 
    ASSERT(keeper->radix_tree != NULL, "create radix tree failed\n");

    keeper->skip_ip_tree = radix_tree_create(NULL); 
    ASSERT(keeper->skip_ip_tree != NULL, "create skip ip radix tree failed\n");

    keeper->skip_domain_tree = domain_tree_create();
    keeper->wildcard_tree = domain_tree_create();

    int ret = read_skip_ip_from_file(keeper->skip_ip_tree, keeper->conf->skip_ip_file);
    ASSERT( ret == 0, "load skip ip library from %s failed", keeper->conf->skip_ip_file);

    ret= read_iplib_from_file(keeper->radix_tree, keeper->conf->iplib_file);
    ASSERT( ret == 0, "load ip library from %s failed", keeper->conf->iplib_file);

    ret = read_skip_domain_from_file(keeper->skip_domain_tree, keeper->conf->skip_domain_file);
    ASSERT( ret == 0, "load skip domain library from %s failed", keeper->conf->skip_domain_file);

    ret = read_wildcard_domain_from_file(keeper->wildcard_tree, keeper->conf->wildcard_file);
    ASSERT( ret == 0, "load wildcard domain library from %s failed", keeper->conf->wildcard_file);

    if (keeper->conf->use_breakpoint_record)
    {
        keeper->breakpoint_fp = fopen(conf->breakpoint_file, "a");
        ASSERT( keeper->breakpoint_fp, "open breakpoint file %s failed", keeper->conf->breakpoint_file);
    }
    return keeper;
}

void house_keeper_set_configfile(house_keeper_t *keeper, const char *config_file)
{
    strcpy(keeper->config_file, config_file);
}

unsigned int return_all_stats(house_keeper_t *keeper,  char **buff)
{
    int n = keeper->views->count;
    StatsReply reply = STATS_REPLY__INIT;
    struct tm *t2 = localtime(&keeper->lasttime);
    char temp_tm_str[32];
    strftime(temp_tm_str, 32, "%Y-%m-%d %H:%M:%S", t2);
    reply.timestr = temp_tm_str;
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
           prate[i] = bw / 60;
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



void write_stats_to_file(house_keeper_t *keeper, const char *filename)
{
    char *result_ptr = NULL;
    int rtn_msg_len;
    rtn_msg_len = return_all_stats(keeper, &result_ptr);
    FILE *fp = fopen(filename, "ab");
    if (!fp)
    {
        printf("open filename error\n");
        return;
    }
    fwrite(&rtn_msg_len, sizeof(int), 1, fp);
    fwrite(result_ptr, rtn_msg_len, 1, fp);
    fclose(fp);
}


