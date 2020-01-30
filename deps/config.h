//
// config.h
//
// Copyright (c) 2020 lalawue
//

#ifndef _CONFIG_H
#define _CONFIG_H

#include <stdint.h>
#include <stdbool.h>
#include <time.h>
#include "cron.h"

/*
 * Monitor.
 */
struct s_monitor {
	int pid; // for match
    const char *name;
    const char *cmd; // program with it's parameters
    const char *logfile;
    const char *on_error;
    const char *on_restart;
	cron_t *cron;
    int64_t last_restart_at;
    int64_t clock;
    int max_sleepsec;
	int sleepsec;
    int max_attempts;
    int attempts;
    struct s_monitor *next_monitor;
};

typedef struct s_monitor monitor_t;

/*
 * Mon
 */
typedef struct {
    const char *name;
	const char *logfile;
    const char *pidfile;
	int daemon;
	time_t time; // start time
	monitor_t *monitors;
	cron_t *crons;
} mon_t;

mon_t* mon_create(const char *file_path);
void mon_destory(mon_t *mon);

#endif