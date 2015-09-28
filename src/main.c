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
#include "log_topn.h"
#include <signal.h>

extern house_keeper_t *keeper;

int goon_stats = 1;

void signal_cb(int sig)
{
    if (sig ==  SIGINT || sig == SIGTERM)
    {
        house_keeper_destroy(keeper);
        goon_stats = 0;
    }
}

int main(int argc, char **argv)
{
    if (argc != 2)
    {
        printf("usage %s config_file\n", argv[0]);
        return 1;
    }
    keeper = house_keeper_create(argv[1]);
    if (keeper)
        printf("create log server manager success\n");
    else
    {
        printf("craete log server manager failed\n");
        return 1;
    }
    signal(SIGINT, signal_cb);
    signal(SIGTERM, signal_cb);
    char filename[1024];
    bool first = false;
    get_monitor_logfile(keeper, filename);
    char line[1024];
    FILE *file;
again:    file = fopen(filename, "r");
    if (!file)
	{
        printf("query log file %s not exists\n", filename);
		exit(1);
	}
    if (false == first) //first open file, and move file pointer to file-end
    {
        fseek(file, 0, SEEK_END);
        first = true;
    }
    struct stat sbuf;
    if (lstat(filename, &sbuf) == -1)
    {
        printf("read file stat error\n");
    }
    int last_inode = get_inode(&sbuf);
    char strbuf[1024]; 
goon: while (NULL != fgets(strbuf, 1024, file)) 
    {
       handle_string_log(keeper, strbuf);
    }
    if (feof(file))
    {
        lstat(filename, &sbuf);
        int new_inode = get_inode(&sbuf);
        if (new_inode == last_inode)
        {
            if (goon_stats == 0)
            {
                fclose(file);
                exit(0);
            }
            sleep(1);
            goto goon;
        }
        else
        {
            fclose(file);
            if (goon_stats == 0)
                exit(0);
            goto again;
        }
    }

    return 0;
}

