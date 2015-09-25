/*
 * log_topn.h
 * Copyright (C) 2015 liuben <liuben@ubuntu>
 *
 * Distributed under terms of the MIT license.
 */

#ifndef LOG_TOPN_H
#define LOG_TOPN_H

#define CIP_P 1
#define VIEW_P 1
#define DOMAIN_P 0
#define RTYPE_P 9
#define RCODE_P 10
#define ZONE_P 17

typedef struct house_keeper house_keeper_t;
void house_keeper_destroy(house_keeper_t *keeper);
house_keeper_t *house_keeper_create();

#endif /* !LOG_TOPN_H */
