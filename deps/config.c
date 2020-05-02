//
// config.c
//
// Copyright (c) 2020 lalawue
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "config.h"
#include "json_file.h"

mon_t* _mon_parse_json(json_value *value);

mon_t*
mon_create(const char* file_path) {
    json_file_t jf;

    if (!json_file_load(file_path, &jf)) {
        exit(1);
    }

	mon_t *mon = _mon_parse_json(jf.json);
	if (!mon) {
		fprintf(stderr, "Invalid json\n");
		exit(1);
	}

    json_file_destroy(&jf);

	return mon;
}

static void
_check_free(void *p) {
	if (p) {
		free(p);
	}
}

static void
_monitor_free(monitor_t *m) {
	_check_free((void *)m->name);
	_check_free((void *)m->cmd);
	_check_free((void *)m->on_error);
	_check_free((void *)m->on_restart);
	if (m->cron) {
		cron_destroy(m->cron);
	}
	_check_free(m);
}

void
mon_destory(mon_t* mon)
{
	if (mon) {
		monitor_t *m = mon->monitors;
		while (m)  {
			monitor_t *next = m->next_monitor;
			_monitor_free(m);
			m = next;
		}
		_check_free((void *)mon->name);
		_check_free((void *)mon->pidfile);
		free(mon);
	}
}

bool
mon_monitor_try_remove(mon_t *mon, monitor_t *monitor) {
	if (mon && monitor && !monitor->cron) {
		if (mon->monitors == monitor) {
			mon->monitors = monitor->next_monitor;
		} else {
			monitor_t *pre = mon->monitors;
			while (pre->next_monitor != monitor) {
				pre = pre->next_monitor;
			}
			if (pre && pre->next_monitor == monitor) {
				pre->next_monitor = monitor->next_monitor;
			}
		}
		_monitor_free(monitor);
		return true;
	}
	return false;
}

void
mon_monitor_reset(monitor_t *monitor) {
	if (monitor) {
		monitor->pid = K_INVALID_MONITOR_PID;
		monitor->attempts = 0;
		monitor->clock = 60000;
	}
}

bool
_entry_name_equal(json_object_entry *entry, char *mark) {
	return strncmp(entry->name, mark, entry->name_length) == 0;
}

static void
_value_string_dup(json_value *value, const char **output) {
	if (value && output && value->type == json_string) {
		*output = strndup(value->u.string.ptr, value->u.string.length);
	}	
}

static monitor_t*
_mon_parse_object(json_object_entry *entry)
{
	if (entry == NULL || entry->value->type != json_object) {
		return NULL;
	}

	monitor_t *m = calloc(1, sizeof(*m));

	m->pid = K_INVALID_MONITOR_PID;
	m->on_restart = NULL;
	m->on_error = NULL;
	m->max_sleepsec = 1;
	m->max_attempts = 10;
	m->name = strndup(entry->name, entry->name_length);
	mon_monitor_reset(m);

	json_value *value = entry->value;

	int length = value->u.object.length;
	for (int i=0; i<length; i++) {
		entry = &value->u.object.values[i];
		if (_entry_name_equal(entry, "name")) {
			_value_string_dup(entry->value, &m->name);
		}
        if (_entry_name_equal(entry, "pidfile")) {
			_value_string_dup(entry->value, &m->pidfile);
		}
		if (_entry_name_equal(entry, "cmd")) {
			_value_string_dup(entry->value, &m->cmd);
		}
		if (_entry_name_equal(entry, "on_error")) {
			_value_string_dup(entry->value, &m->on_error);
		}
		if (_entry_name_equal(entry, "on_restart")) {
			_value_string_dup(entry->value, &m->on_restart);
		}
		if (_entry_name_equal(entry, "cron")) {
			m->cron = cron_create(entry->value->u.string.ptr, entry->value->u.string.length);
		}
		if (_entry_name_equal(entry, "attempts")) {
			m->max_attempts = entry->value->u.integer;
		}
		if (_entry_name_equal(entry, "sleep")) {
			m->max_sleepsec = entry->value->u.integer;
		}
	}

	return m;
}

mon_t*
_mon_parse_json(json_value *value)
{
	if (value == NULL || value->type != json_object) {
		fprintf(stderr, "Invalid json file\n");
		return NULL;
	}

	mon_t *mon = calloc(1, sizeof(*mon));

	mon->logfile = "/dev/null";
	
	int length = value->u.object.length;
	for (int i=length-1; i>=0; i--) {
		json_object_entry *entry = &value->u.object.values[i];
		if (_entry_name_equal(entry, "name")) {
			_value_string_dup(entry->value, &mon->name);
			continue;
		}
		if (_entry_name_equal(entry, "logfile")) {
			_value_string_dup(entry->value, &mon->logfile);
			continue;
		}
		if (_entry_name_equal(entry, "pidfile")) {
			_value_string_dup(entry->value, &mon->pidfile);
			continue;
		}
		if (_entry_name_equal(entry, "daemon")) {
			mon->daemon = entry->value->u.boolean;
			continue;
		}
		monitor_t *m = _mon_parse_object(entry);
		if (m) {
			m->next_monitor = mon->monitors;
			mon->monitors = m;
		}
	}

	if (!mon->name || !mon->monitors) {
		return NULL;
	}

	// validate all monitors cmd
	monitor_t *m = mon->monitors;
	while (m) {
		if (!m->cmd) {
			return NULL;
		}
		m = m->next_monitor;
	}

	return mon;
}