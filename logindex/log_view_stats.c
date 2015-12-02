#include <stdlib.h>
#include <string.h>
#include "log_view_stats.h"
#include "log_utils.h"
#include "statsmessage.pb-c.h"
view_stats_t *view_stats_create(const char *view_name)
{
	if (strlen(view_name) > MAX_VIEW_LENGTH )
		return NULL;

    view_stats_t *vs = malloc(sizeof(view_stats_t));
    if (NULL == vs)
        return NULL;

    vs->log_file_fp = NULL;
    vs->idx_file_fp = NULL;
    vs->last_time = 0;
    vs->latest_time = 0;
    vs->last_fp_offset = 0;
    view_stats_init(vs);
    strcpy(vs->name, view_name);
}

void view_stats_destory(view_stats_t *vs)
{
    ASSERT(vs, "empty pointer when destory view stats\n");
    if (vs->log_file_fp)
        fclose(vs->log_file_fp);
    if (vs->idx_file_fp)
        fclose(vs->idx_file_fp);
    free(vs);
}

void view_stats_hit_bandwidth_increment(view_stats_t *vs, int content_size)
{
    ASSERT(vs , "view stats is NULL when insert\n");
    vs->hit_bandwidth += content_size;
    vs->hit_count++;
}
void view_stats_lost_bandwidth_increment(view_stats_t *vs, int content_size)
{
    ASSERT(vs , "view stats is NULL when insert\n");
    vs->lost_bandwidth += content_size;
    vs->lost_count++;
}
void view_stats_bandwidth_increment(view_stats_t *vs, int content_size)
{
    ASSERT(vs , "view stats is NULL when insert\n");
    vs->bandwidth += content_size;
    vs->count++;
}
void view_stats_init(view_stats_t *vs)
{
    vs->rate = 0;
    vs->count = 0;
    vs->last_count = 0;
    vs->bandwidth = 0;
    vs->last_bandwidth = 0;
    vs->hit_count = 0;
    vs->hit_bandwidth = 0;
    vs->lost_count = 0;
    vs->lost_bandwidth = 0;
}

void view_stats_set_bandwidth(view_stats_t *vs, unsigned int bw)
{
    //unit is k
    vs->bandwidth = bw;
}
unsigned int view_stats_bandwidth(view_stats_t *vs )
{
    //unit is k
    return vs->bandwidth;
}
