#ifndef __HEADER_LOG_VIEW_STATS__H
#define __HEADER_LOG_VIEW_STATS__H
#define RCODE_MAX_NUM 7
#define RTYPE_MAX_NUM 12
#include "log_name_tree.h"
#include "dig_logger.h"
#include "statsmessage.pb-c.h"
#define MAX_VIEW_LENGTH 64
struct view_stats {
    // name can  be domain , or domain + view 
    char name[MAX_VIEW_LENGTH];
    name_tree_t *name_tree;
    name_tree_t *ip_tree;
    zddi_logger_t *logger;
    float qps;
    float success_rate;
    float rate; 
    uint32_t count;
    uint32_t last_count;
    uint64_t bandwidth;
    uint32_t last_bandwidth;
    uint32_t hit_count;
    uint32_t hit_bandwidth;
    uint32_t lost_count;
    uint32_t lost_bandwidth;
};

typedef enum stats_type {
    stats_rcode_noerror = 0, 
    stats_rcode_formaterr = 1,
    stats_rcode_servfail = 2,
    stats_rcode_nxdomain = 3,
    stats_rcode_notimp = 4,
    stats_rcode_refused = 5,
    stats_rcode_otherfail = 6,
    stats_rtype_a = 0,
    stats_rtype_mx = 1,
    stats_rtype_aaaa = 2,
    stats_rtype_cname =3,
    stats_rtype_ns = 4,
    stats_rtype_soa = 5,
    stats_rtype_txt = 6,
    stats_rtype_srv = 7,
    stats_rtype_dname = 8,
    stats_rtype_naptr = 9,
    stats_rtype_other= 10
}stats_type_t;

typedef struct view_stats view_stats_t;
view_stats_t *view_stats_create(const char *view_name);
void view_stats_destory(view_stats_t *vs);
void view_stats_insert_ip(view_stats_t *vs, const char *ip);
void view_stats_insert_name(view_stats_t *vs, const char *name);
unsigned int view_stats_name_topn(view_stats_t *vs, int topn, char **buff);
unsigned int view_stats_ip_topn(view_stats_t *vs, int topn, char **buff);
unsigned int view_stats_get_success_rate(view_stats_t *vs, char **buff);
unsigned int view_stats_get_qps(view_stats_t *vs, char **buff);
void view_stats_set_memsize(view_stats_t *vs, uint64_t expect_size);
void view_stats_bandwidth_increment(view_stats_t *vs, int content_size);
void view_stats_hit_bandwidth_increment(view_stats_t *vs, int content_size);
void view_stats_lost_bandwidth_increment(view_stats_t *vs, int content_size);
unsigned int view_stats_bandwidth(view_stats_t *vs);

unsigned int view_stats_get_bandwidth(view_stats_t *vs, char **buff);
void view_stats_set_bandwidth(view_stats_t *vs, unsigned int bw);
void view_stats_init(view_stats_t *vs);

uint64_t view_stats_get_size(view_stats_t *vs);
#endif
