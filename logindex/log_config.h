#ifndef __LOG_CONFIG__H
#define __LOG_CONFIG__H

struct config {
    int enable_auto_log;
    int max_file_count;
    int file_size_limit;
    char home_dir[256];
    char stats_dir[256];
    char file_prefix[128];
    int rate_write_interval;
    double rate_threshold;
    char server_ip[32];
    char iplib_file[512];
    char wildcard_file[512];
    char breakpoint_file[512];
    char skip_domain_file[512];
    char skip_ip_file[512];
    char monitor_log_file[512];
    char timeformat[32];
    char log_file_change_mode[32];
    int wait_seconds_when_file_end;
    int wait_even_file_not_exists;
    int cache_server_type;
    int stop_when_file_end;
    int read_from_end;
    int port;
    int use_breakpoint_record;
    int memory_recycle;
    int topn_stats;
    int topn;
    int max_memory;
    int key_pos;
    int client_pos;
    int domain_pos;
    int time_pos;
    int tzone_pos;
    int status_pos;
    int content_pos;
    int hit_pos;
    int file_cut_interval;
    int stats_interval;
    float sample_rate;
};
typedef struct config config_t;
config_t * config_create(const char *filename);
void config_destroy(config_t *conf);
void config_output(config_t *conf, FILE *fp);
void config_write_file(config_t *conf, const char *filename);
char * strip(char *src);
const char *do_ioctl_get_ipaddress(const char *dev);
int rev_find_str(int recv_len, char *str, char ch);
int config_max_pos(config_t *conf);

#endif
