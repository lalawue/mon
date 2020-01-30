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
	void *opaque; 			// user data
	struct s_cron *next;	// for user to link as a list
} cron_t;

cron_t* cron_create(const char *entry, int entry_len);
void cron_destroy(cron_t *);

bool cron_should_invoke(cron_t *, struct tm *);

#endif