//
// config.h
//
// Copyright (c) 2020 lalawue
//

#ifndef _CRON_H
#define _CRON_H

#include <stdbool.h>
#include <sys/time.h>

typedef struct s_cron {
	unsigned char min[8];	// 64 bits, 0 - 59
	unsigned char hour[3];	// 24 bits, 0 - 23
	unsigned char mdays[4];	// 32 bits, 1 - 31
	unsigned char mon[2];	// 16 bits, 0 - 11
	unsigned char wdays[1];	// 8 bits, 0 - 6, (Sunday - Staturday), ignored if mdays are seted
	bool has_running;		// mark true when in active time area and invoked
} cron_t;

cron_t* cron_create(const char *entry, int entry_len);
void cron_destroy(cron_t *);

// invoke when has_running toggle from false to true
bool cron_in_timearea(cron_t *, struct tm *);
void cron_set_has_running(cron_t *, bool);

#endif