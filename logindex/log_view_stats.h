#ifndef __HEADER_LOG_VIEW_STATS__H
#define __HEADER_LOG_VIEW_STATS__H
#include <stdio.h>
#include <time.h>
#include "statsmessage.pb-c.h"
#define MAX_VIEW_LENGTH 128
struct view_stats {
    // name can  be domain , or domain + view 
    char name[MAX_VIEW_LENGTH];
    FILE *log_file_fp;
    FILE *idx_file_fp;
    time_t last_time;
    time_t latest_time;
    uint32_t last_fp_offset;
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

typedef struct view_stats view_stats_t;
view_stats_t *view_stats_create(const char *view_name);
void view_stats_destory(view_stats_t *vs);
void view_stats_bandwidth_increment(view_stats_t *vs, int content_size);
void view_stats_hit_bandwidth_increment(view_stats_t *vs, int content_size);
void view_stats_lost_bandwidth_increment(view_stats_t *vs, int content_size);
unsigned int view_stats_bandwidth(view_stats_t *vs);

void view_stats_set_bandwidth(view_stats_t *vs, unsigned int bw);
void view_stats_init(view_stats_t *vs);

uint64_t view_stats_get_size(view_stats_t *vs);
#endif
