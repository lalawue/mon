//
// config.c
//
// Copyright (c) 2020 lalawue
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "config.h"
#include "json.h"

mon_t* _mon_parse_json(json_value *value);

mon_t*
mon_create(const char* file_path)
{
	FILE* fp;
	struct stat filestatus;
	int file_size;
	char* file_contents;
	json_value* value;

	if (file_path == NULL) {
		fprintf(stderr, "Invalid file path\n");
	}

	if (stat(file_path, &filestatus) != 0) {
		fprintf(stderr, "File '%s' not found\n", file_path);
		exit(1);
	}

	file_size = filestatus.st_size;
	file_contents = (char*)malloc(filestatus.st_size);
	if (file_contents == NULL) {
		fprintf(stderr, "Memory error: unable to allocate %d bytes\n", file_size);
		exit(1);
	}

	fp = fopen(file_path, "rt");
	if (fp == NULL) {
		fprintf(stderr, "Unable to open %s\n", file_path);
		fclose(fp);
		free(file_contents);
		exit(1);
	}
	if (fread(file_contents, file_size, 1, fp) != 1) {
		fprintf(stderr, "Unable t read content of %s\n", file_path);
		fclose(fp);
		free(file_contents);
		exit(1);
	}

	value = json_parse((json_char*)file_contents, file_size);

	if (value == NULL) {
		fprintf(stderr, "Unable to parse content\n");
		free(file_contents);
		exit(1);
	}

	mon_t *m = _mon_parse_json(value);
	if (!m) {
		fprintf(stderr, "Invalid json\n");
		exit(1);
	}

	json_value_free(value);
	free(file_contents);

	return m;
}

static void
_moniter_free(void *p)
{
	if (p) {
		free(p);
	}
}

void
mon_destory(mon_t* mon)
{
	if (mon) {
		monitor_t *m = mon->monitors;
		while (m)  {
			monitor_t *next = m->next_monitor;
			_moniter_free((void *)m->name);
			_moniter_free((void *)m->cmd);
			_moniter_free((void *)m->logfile);
			_moniter_free((void *)m->on_error);
			_moniter_free((void *)m->on_restart);
			m = next;
		}
		_moniter_free((void *)mon->name);
		_moniter_free((void *)mon->pidfile);
		free(mon);
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

	m->on_restart = NULL;
	m->on_error = NULL;
	m->logfile = "/dev/null";
	m->max_sleepsec = 1;
	m->max_attempts = 0; // infinite
	m->clock = 60000;
	m->name = strndup(entry->name, entry->name_length);

	json_value *value = entry->value;

	int length = value->u.object.length;
	for (int i=0; i<length; i++) {
		entry = &value->u.object.values[i];
		if (_entry_name_equal(entry, "name")) {
			_value_string_dup(entry->value, &m->name);
		}
		if (_entry_name_equal(entry, "cmd")) {
			_value_string_dup(entry->value, &m->cmd);
		}
		if (_entry_name_equal(entry, "logfile")) {
			_value_string_dup(entry->value, &m->logfile);
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
	monitor_t *last_monitor = NULL;

	mon->logfile = "/dev/null";
	
	int length = value->u.object.length;
	for (int i=0; i<length; i++) {
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
			if (mon->monitors == NULL) {
				mon->monitors = m;
			} else {
				last_monitor->next_monitor = m;
			}
			last_monitor = m;
		}
	}

	if (!mon->name || !mon->monitors) {
		return NULL;
	}

	monitor_t *m = mon->monitors;
	while (m) {
		monitor_t *next = m->next_monitor;
		if (!m->cmd) {
			return NULL;
		}
		m = next;
	}

	return mon;
}