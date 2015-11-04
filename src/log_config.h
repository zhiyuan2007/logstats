#ifndef __LOG_CONFIG__H
#define __LOG_CONFIG__H

struct config {
    int enable_auto;
    int max_file_count;
    int file_size_limit;
    char home_dir[256];
    char file_prefix[128];
    int rate_write_interval;
    double rate_threshold;
    char server_ip[32];
    char iplib_file[512];
    char monitor_log_file[512];
    char timeformat[32];
    int read_from_end;
    int port;
    int memory_recycle;
    int topn_stats;
    int topn;
    uint32_t max_memory;
    int key_pos;
    int client_pos;
    int domain_pos;
    int time_pos;
    int status_pos;
    int content_pos;
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

#endif
