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

int mon_get_pid(const char *pid_file);

#endif
