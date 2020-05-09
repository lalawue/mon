//
// status.h
//
// Copyright (c) 2020 lalawue
//

#ifndef _STATUS_H
#define _STATUS_H

#include "config.h"

/** dump monitor list status into JSON
 */
void mon_dump_group(mon_t *mon);

void mon_dump_monitor(monitor_t *monitor);

void mon_show_status(const char *pid_file);

typedef struct s_mon_status {
	const char name[128];
	time_t time;
	int pid;
	struct s_mon_status *next;
} mon_status_t;

mon_status_t* mon_status_list(const char *pid_file);
void mon_status_destroy(mon_status_t *);

#endif
