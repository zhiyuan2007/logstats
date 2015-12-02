#include "log_utils.h"
#define MINUTE 60
#define HOUR (60*MINUTE)
#define DAY (24*HOUR)
#define YEAR (365*DAY)

#define EXISTS_V (void *)1
static int month[12] = {
         0,
         DAY*(31),
         DAY*(31+29),
         DAY*(31+29+31),
         DAY*(31+29+31+30),
         DAY*(31+29+31+30+31),
         DAY*(31+29+31+30+31+30),
         DAY*(31+29+31+30+31+30+31),
         DAY*(31+29+31+30+31+30+31+31),
         DAY*(31+29+31+30+31+30+31+31+30),
         DAY*(31+29+31+30+31+30+31+31+30+31),
         DAY*(31+29+31+30+31+30+31+31+30+31+30)
};
long kernel_mktime(struct tm * tm )
{
         long res;
         int year;
         year= tm->tm_year - 70; 
         res= YEAR*year + DAY*((year+1)/4);               
         res+= month[tm->tm_mon];
         if(tm->tm_mon>1 && ((year+2)%4))
               res-= DAY;
         res+= DAY*(tm->tm_mday-1);
         res+= HOUR*tm->tm_hour;
         res+= MINUTE*tm->tm_min;
         res+= tm->tm_sec;
         return res;
}

int read_skip_ip_from_file(radix_tree_t *radix_tree, const char *filename)
{   
    FILE *fp = fopen(filename, "r") ;
    if (fp == NULL)
    {
        printf("open file %s failed!\n", filename);
        return 1;
    }
    char line[256] = {'\0'};
    while (fgets(line, 256, fp) != NULL) 
    {
       line[strlen(line) - 1] = '\0';
       char *p = strchr(line, '/');
       if (!p)
       {
          strcat(line, "/32");
       }
       radix_tree_insert_subnet(radix_tree, line, EXISTS_V);
    }

    fclose(fp);
    return 0;
}
int read_skip_domain_from_file(domain_tree_t *domain_tree, const char *filename)
{   
    FILE *fp = fopen(filename, "r") ;
    if (fp == NULL)
    {
        printf("open file %s failed!\n", filename);
        return 1;
    }
    char line[256] = {'\0'};
    while (fgets(line, 256, fp) != NULL) 
    {
       line[strlen(line) - 1] = '\0';
       domain_tree_insert(domain_tree, line);
    }

    fclose(fp);
    return 0;
}

int read_wildcard_domain_from_file(domain_tree_t *domain_tree, const char *filename)
{   
    FILE *fp = fopen(filename, "r") ;
    if (fp == NULL)
    {
        printf("open file %s failed!\n", filename);
        return 1;
    }
    char line[256] = {'\0'};
    while (fgets(line, 256, fp) != NULL) 
    {
       line[strlen(line) - 1] = '\0';
       if (line[0] == '*')
       {
           printf("wildcard domain is %s\n", line);
           domain_tree_insert(domain_tree, line+2);
       }
       else
       {
           printf("normal domain is %s\n", line);
           domain_tree_insert(domain_tree, line);
       }
    }

    fclose(fp);
    return 0;
}

int read_iplib_from_file(radix_tree_t *radix_tree, const char *filename)
{   
    FILE *fp = fopen(filename, "r") ;
    if (fp == NULL)
    {
        printf("open file %s failed\n", filename);
        return 1;
    }
    char line[256] = {'\0'};
    while (fgets(line, 256, fp) != NULL) 
    {
        char *p = NULL;
        char *subnet = NULL;
        p = strtok(line, " \t"); 
        int i = 0;
        while (p)
        {
            if (i == 0) 
               subnet = p; 
            else if (i == 1)
            {
               break; 
            }
            i++;
            p = strtok(NULL, "\n");
        }
        if (subnet != NULL && i == 1) 
        {
            char *t = strchr(subnet, '/');
            if (!t)
            {
               strcat(subnet, "/32");
            }
            char *view_id = malloc(sizeof(char ) * 8); 
            strcpy(view_id, p) ;
            //printf("view_id %s, network %s\n", view_id, subnet);
            radix_tree_insert_subnet(radix_tree, subnet, view_id);
        }
        
    }

    fclose(fp);
    return 0;
}


