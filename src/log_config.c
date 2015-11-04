#include <netinet/in.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "log_config.h"



config_t * config_create(const char *filename)
{
    FILE *fp = fopen(filename, "r");
    if (!fp) 
    {
        fprintf(stderr, "Error: not find config file %s\n", filename);
        return NULL;
    }
    config_t * conf = malloc(sizeof(config_t));
    memset(conf, '\0', sizeof(config_t));
    //set default value
    conf->topn_stats = 0; 
    conf->memory_recycle = 0;
    conf->sample_rate = 1.0;
    conf->enable_auto = 0;
    char tempconf[1024];
    while(fgets(tempconf, 1024, fp))
    {
        int len = strlen(tempconf);
        if (len <= 1)
            continue;
        strip(tempconf);
        if (tempconf[0] == '#')
            continue;
        tempconf[len - 1] = '\0';
        char *p = strchr(tempconf, '='); 
        if (p)
            *p = '\0';
        strip(tempconf);
        p++;
        strip(p);
        printf("[key=%s\t", tempconf);
        printf("value=%s]\n", p);
        if (strcmp("enable_auto", tempconf) == 0 )
           conf->enable_auto = atoi(p);
        else if (strcmp("home_dir", tempconf) == 0 )
           strcpy(conf->home_dir, p);
        else if (strcmp("file_prefix", tempconf) == 0 )
           strcpy(conf->file_prefix, p);
        else if (strcmp("file_size_limit", tempconf) == 0 )
           conf->file_size_limit = atoi(p);
        else if (strcmp("max_file_count", tempconf) == 0 )
           conf->max_file_count = atoi(p);
        else if (strcmp("port", tempconf) == 0 )
           conf->port = atoi(p);
        else if (strcmp("server_ip", tempconf) == 0 )
           strcpy(conf->server_ip, p);
        else if (strcmp("iplib_file", tempconf) == 0 )
           strcpy(conf->iplib_file, p);
        else if (strcmp("read_from_end", tempconf) == 0 )
          conf->read_from_end = atoi(p);
        else if (strcmp("monitor_log_file", tempconf) == 0 )
           strcpy(conf->monitor_log_file, p);
        else if (strcmp("memory_recycle", tempconf) == 0 )
           conf->memory_recycle = atoi(p);
        else if (strcmp("topn_stats", tempconf) == 0 )
           conf->topn_stats = atoi(p);
        else if (strcmp("topn", tempconf) == 0 )
           conf->topn = atoi(p);
        else if (strcmp("max_memory", tempconf) == 0 )
           conf->max_memory = atoi(p);
        else if (strcmp("key_pos", tempconf) == 0 )
           conf->key_pos = atoi(p);
        else if (strcmp("client_pos", tempconf) == 0 )
           conf->client_pos = atoi(p);
        else if (strcmp("time_pos", tempconf) == 0 )
           conf->time_pos = atoi(p);
        else if (strcmp("domain_pos", tempconf) == 0 )
           conf->domain_pos = atoi(p);
        else if (strcmp("status_pos", tempconf) == 0 )
           conf->status_pos = atoi(p);
        else if (strcmp("content_pos", tempconf) == 0 )
           conf->content_pos = atoi(p);
        else if (strcmp("sample_rate", tempconf) == 0 )
           conf->sample_rate = atof(p);
        else if (strcmp("timeformat", tempconf) == 0 )
           strcpy(conf->timeformat, p);
    }
    fclose(fp);
    return conf;
}
void config_output(config_t *conf, FILE *fp)
{   
    fprintf(fp, "home_dir = %s\n", conf->home_dir);
    fprintf(fp, "enable_auto = %d\n", conf->enable_auto);
    fprintf(fp, "file_prefix = %s\n", conf->file_prefix);
    fprintf(fp, "file_size_limit = %d\n", conf->file_size_limit);
    fprintf(fp, "max_file_count = %d\n", conf->max_file_count);
    fflush(fp);
}

void config_write_file(config_t *conf, const char *filename)
{
    FILE *fp = fopen(filename, "w");
    if (fp == NULL) 
    {
        printf("open file failed\n");
        return;
    }
    config_output(conf, fp);
    fclose(fp);
}

void config_destroy(config_t *conf)
{
    free(conf);
}
char * strip(char *src)
{
    char *p = src + strlen(src) - 1;
    for (; p >= src && *p == ' '; p--);
    *(p + 1) = '\0';
    for (p=src; *p == ' '; p++);
    int i = 0;
    while(src[i] = p[i++]);
    return src;
}

const char *
do_ioctl_get_ipaddress(const char *dev)
{
    struct ifreq ifr;
    int fd;
    unsigned long ip;
    struct in_addr tmp_addr;

    strcpy(ifr.ifr_name, dev);
    fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (ioctl(fd, SIOCGIFADDR, &ifr))
    {
        perror("ioctl error");
        return (char*)"ipunkwown";
    }
    close(fd);
    memcpy(&ip, ifr.ifr_ifru.ifru_addr.sa_data + 2, 4);
    tmp_addr.s_addr = ip;
    const char *ip_addr = inet_ntoa(tmp_addr);
    fprintf(stdout,"%s : %s\n", dev, ip_addr);

    return ip_addr;
}

int rev_find_str(int recv_len, char *str, char ch)
{
    if (!str)
        return -1;
    int i = 0;
    for (; i < recv_len; i++)
    {
        if (str[recv_len - i -1] == ch)
            return recv_len - i;
    }
    return 0;
}

