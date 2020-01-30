//
// status.h
//
// Copyright (c) 2020 lalawue
//

#ifndef _STATUS_H
#define _STATUS_H

#include "config.h"

/*
 * Status
 */

void mon_dump_status(mon_t *mon);

void mon_show_status(const char *pid_file);

int mon_get_pid(mon_t *mon);

#endif
