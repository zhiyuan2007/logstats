/*
 * main.c
 * Copyright (C) 2015 liuben <liuben@ubuntu>
 *
 * Distributed under terms of the MIT license.
 */

#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>
#include <time.h>
#include <stdbool.h>
#include "log_stats.h"
#include <signal.h>
#include "log_config.h"

extern house_keeper_t *keeper;

FILE *file = NULL;

static int get_inode(struct stat *ptr)
{
    return (int)ptr->st_ino;
}
void recycle_all_resource()
{
    house_keeper_destroy(keeper);
    if (file)
       fclose(file);
    exit(0);
}
void signal_cb(int sig)
{
    if (sig ==  SIGINT || sig == SIGTERM)
    {
        printf("quit\n");
        recycle_all_resource();
    }
    else if (sig == SIGHUP)
    {
        house_keeper_reload(keeper);
    }
}

int main(int argc, char **argv)
{
    if (argc != 2)
    {
        printf("usage %s config_file\n", argv[0]);
        return 1;
    }

    config_t *conf = config_create(argv[1]);
    if (!conf) 
    {
        printf("create config failed\n");
        return 1;
    }

    keeper = house_keeper_create(conf);
    if (keeper)
    {
        printf("create log server manager success\n");
    }
    else
    {
        printf("craete log server manager failed\n");
        return 1;
    }
    house_keeper_set_configfile(keeper, argv[1]);

    signal(SIGHUP, signal_cb);
    signal(SIGINT, signal_cb);
    signal(SIGTERM, signal_cb);

    if (conf->cache_server_type == 1)
        start_stats_nginx_log(keeper);
    else if (conf->cache_server_type == 2)
        start_stats_cloudcache_log(keeper);
}

void truncate_and_rewrite(FILE *fp, int inode, int offset)
{
   ftruncate(fileno(fp), 0); 
   rewind(fp);
   fprintf(fp, "inode %d, offset %d\n", inode, offset);
   fflush(keeper->breakpoint_fp);
}

int start_stats_nginx_log(house_keeper_t *keeper)
{
    char filename[1024];
    int first = 0;
    config_t *conf = keeper->conf; 
    if (conf->read_from_end == -1)
    {
        first = 1;
    }
    time_t tt;
    time(&tt);
    struct tm *ts = localtime(&tt);
    memcpy(&keeper->last_ts, ts, sizeof(struct tm)); 

    strcpy(filename, conf->monitor_log_file);
again:    file = fopen(filename, "r");
    if (!file)
	{
        printf("query log file %s not exists\n", filename);
        if (conf->wait_even_file_not_exists)
        {
            sleep(conf->wait_seconds_when_file_end);
            usleep(100000);
            time(&tt);
            ts = localtime(&tt);
            memcpy(&keeper->last_ts, ts, sizeof(struct tm)); 
            goto again;
        }
        else
        {
            recycle_all_resource();
        }
	}

    if (1 == first) //first open file, and move file pointer to file-end
    {
        fseek(file, 0, SEEK_END);
        first = -1;
    }
    else if (0 == first)
    {
        fseek(file, conf->read_from_end, SEEK_SET);
        first = -1;
    }

    struct stat sbuf;
    if (lstat(filename, &sbuf) == -1)
    {
        printf("read file stat error\n");
    }
    int last_inode = get_inode(&sbuf);
    char strbuf[RECORD_LEN]; 
goon: while (NULL != fgets(strbuf, RECORD_LEN, file)) 
    {
       handle_string_log(keeper, strbuf);
       if (conf->use_breakpoint_record)
       {
           truncate_and_rewrite(keeper->breakpoint_fp, last_inode, ftell(file));
       }
    }
    if (feof(file))
    {
        if (conf->stop_when_file_end)
        {
           recycle_all_resource();
        }
        lstat(filename, &sbuf);
        int new_inode = get_inode(&sbuf);
        if (new_inode == last_inode)
        {
            usleep(100000);
            goto goon;
        }
        else
        {
            fclose(file);
            goto again;
        }
    }
    return 0;
}

int generate_log_filename(char *filename, config_t *conf, struct tm *ts)
{
    char timestamp[32];
    char *pl = strchr(conf->monitor_log_file, '%');
    if (pl == NULL) 
    {
        strcpy(filename, conf->monitor_log_file);
        return 0;
    }

    char *pr = strrchr(conf->monitor_log_file, '%');
    char format[32];
    char first_part[256];
    memset(first_part, '\0', 256);
    strncpy(first_part, conf->monitor_log_file, pl - conf->monitor_log_file);
    char last_part[128];
    strcpy(last_part, pr+2);

    if (pl == pr)
       strncpy(format, pl, 2);
    else
       strncpy(format, pl, pr + 1 - pl + 1);
    strftime(timestamp, 32, format, ts); 
    sprintf(filename, "%s%s%s", first_part, timestamp, last_part);

    return 0;
}

int is_file_cut_time(struct tm *ts1, struct tm *ts2, const char *file_change_mode)
{
    if (strcmp(file_change_mode, "hour") == 0) 
    {
        return (ts1->tm_hour != ts2->tm_hour);
    }
    if (strcmp(file_change_mode, "day") == 0) 
    {
        return (ts1->tm_mday != ts2->tm_mday);
    }
    return 0;
}

int start_stats_cloudcache_log(house_keeper_t *keeper)
{
    char filename[1024];
    int first = 0;
    time_t tt;
    time(&tt);
    struct tm *ts = localtime(&tt);
    memcpy(&keeper->last_ts, ts, sizeof(struct tm)); 
    config_t *conf = keeper->conf; 
    if (conf->read_from_end == -1)
    {
        first = 1;
    }

again:
    generate_log_filename(filename, conf, ts);
    printf("start stats file %s\n", filename);
    file = fopen(filename, "r");
    if (!file)
	{
        printf("query log file %s not exists\n", filename);
        if (conf->wait_even_file_not_exists)
        {
            sleep(conf->wait_seconds_when_file_end);
            usleep(100000);
            time(&tt);
            ts = localtime(&tt);
            memcpy(&keeper->last_ts, ts, sizeof(struct tm)); 
            goto again;
        }
        else
        {
            recycle_all_resource();
        }
	}
    if (1 == first) //first open file, and move file pointer to file-end
    {
        fseek(file, 0, SEEK_END);
        first = -1;
    }
    else if (0 == first)
    {
        fseek(file, conf->read_from_end, SEEK_SET);
        first = -1;
    }
    char strbuf[RECORD_LEN]; 
    int prevent_log_lost = 1;
    struct stat sbuf;
    if (lstat(filename, &sbuf) == -1)
    {
        printf("read file stat error\n");
    }
    int last_inode = get_inode(&sbuf);
goon: while (NULL != fgets(strbuf, RECORD_LEN, file)) 
    {
       handle_string_log(keeper, strbuf);
       if (conf->use_breakpoint_record)
       {
           truncate_and_rewrite(keeper->breakpoint_fp, last_inode, ftell(file));
       }
    }
    if (feof(file))
    {
        if (conf->stop_when_file_end)
        {
           recycle_all_resource();
        }
        time(&tt);
        ts = localtime(&tt);
        if (is_file_cut_time(ts, &keeper->last_ts, conf->log_file_change_mode) == 0)
        {
            usleep(100000);
            goto goon;
        }
        else
        {
            if (prevent_log_lost)
            {
               printf("process wait %d seconds for the last read\n", conf->wait_seconds_when_file_end); 
               sleep(conf->wait_seconds_when_file_end);
               prevent_log_lost = 0;
               goto goon;
            }

            fclose(file);
            memcpy(&keeper->last_ts, ts, sizeof(struct tm)); 
            goto again;
        }
    }
    return 0;
}
