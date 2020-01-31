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

#define K_INVALID_MONITOR_PID -1

/*
 * Monitor.
 */
struct s_monitor {
	int pid;					// 0 for dead state
    const char *name;			// monitor name
    const char *cmd;			// program with it's parameters
    const char *logfile;
    const char *on_error;		// cmd on error
    const char *on_restart;		// cmd when restart
	cron_t *cron;				// cron info
    int64_t last_restart_at;	// last restart time ms
    int64_t clock;
    int max_sleepsec;			// sleep seconds when restart
	int sleepsec;				// sleeped time before next restart
    int max_attempts;			// max restart count
    int attempts;				// restart count
	int status;					// exit status
    struct s_monitor *next_monitor;
};

typedef struct s_monitor monitor_t;

/*
 * Mon
 */
typedef struct {
    const char *name;			// group name
	const char *logfile;		// mon_sched logfile
    const char *pidfile;		// group process pid
	int daemon;					// daemonize
	time_t time;				// start time
	monitor_t *monitors;		// monitor list
} mon_t;

mon_t* mon_create(const char *file_path);
void mon_destory(mon_t *mon);

// return true when monitor is not a cron task, can be removed
bool mon_monitor_try_remove(mon_t *mon, monitor_t *monitor);
void mon_monitor_reset(monitor_t *monitor);

#endif