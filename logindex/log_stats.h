/*
 * log_topn.h
 * Copyright (C) 2015 liuben <liuben@ubuntu>
 *
 * Distributed under terms of the MIT license.
 */

#ifndef LOG_STATS_H
#define LOG_STATS_H

#define RECORD_LEN 2048 

#include "log_config.h"
#include "log_domain_tree.h"
#include "dig_radix_tree.h"
#include "log_view_tree.h"
#include "log_config.h"

typedef struct house_keeper house_keeper_t;

struct house_keeper {
    uint64_t count;
    uint64_t valid_count;
    uint64_t skip_count;
    int max_pos;
    view_tree_t *views;
    radix_tree_t *radix_tree;     
    radix_tree_t *skip_ip_tree;
    domain_tree_t *skip_domain_tree;
    domain_tree_t *wildcard_tree;
    config_t *conf;
    char config_file[256];
    time_t lasttime;
    time_t last_log_cut_time;
    char timestamp_str[32];
    char hostname[256];
    struct tm last_ts;
    FILE *breakpoint_fp;
};
void house_keeper_destroy(house_keeper_t *keeper);
void *house_keeper_config(house_keeper_t *keeper);
house_keeper_t *house_keeper_create(config_t *conf);
void house_keeper_set_configfile(house_keeper_t *keeper, const char *config_file);
void house_keeper_reload(house_keeper_t *keeper);

#endif /* !LOG_STATS_H */
