//
// status.c
//
// Copyright (c) 2020 lalawue Holowaychuk
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/time.h>
#include "status.h"
#include "json.h"
#include "ms.h"

/** dump status as JSON
 */
void
mon_dump_status(mon_t *mon) {
	if (!mon || !mon->pidfile) {
		perror("dump status ");
		return;
	}

	int fd = open(mon->pidfile, O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR);
	if (fd < 0) {

	}

	const int buf_len = 4096;
	unsigned char *buf = malloc(buf_len);
	int bytes = 0;
	time_t ti = time(NULL);
	write(fd, "{\n", 2);
	bytes = snprintf((char *)buf, buf_len, "\t\"%s\" : {\n\t\t\"time\" : %ld, \"pid\" : %d\n\t},\n",
	 				mon->name, ti, getpid());
	write(fd, buf, bytes);					 
	{
		monitor_t *m = mon->monitors;
		while (m) 
		{	
			time_t mti = m->last_restart_at / 1000;
			bytes = snprintf((char *)buf, buf_len, "\t\"%s\" : {\n\t\t\"time\" : %ld, \"pid\" : %d\n\t}",
							 m->name, mti ? mti : ti, m->pid);
			write(fd, buf, bytes);
			m = m->next_monitor;
			if (m) {
				write(fd, ",\n", 2);
			} else {
				write(fd, "\n", 1);
				break;
			}
		}
	}
	write(fd, "}\n", 2);
	free(buf);
	close(fd);
}

typedef struct s_mon_status {
	const char name[128];
	time_t time;
	int pid;
	struct s_mon_status *next;
} mon_status_t;

mon_status_t* _status_parse_json(json_value *value);

static mon_status_t*
_get_status_list(const char *pid_file) {

	if (pid_file == NULL) {
		return NULL;
	}

	FILE* fp;
	struct stat filestatus;
	int file_size;
	char* file_contents;
	json_value* value;

	if (pid_file == NULL) {
		fprintf(stderr, "Invalid file path\n");
	}

	if (stat(pid_file, &filestatus) != 0) {
		fprintf(stderr, "File '%s' not found\n", pid_file);
		exit(1);
	}

	file_size = filestatus.st_size;
	file_contents = (char*)malloc(filestatus.st_size);
	if (file_contents == NULL) {
		fprintf(stderr, "Memory error: unable to allocate %d bytes\n", file_size);
		exit(1);
	}

	fp = fopen(pid_file, "rt");
	if (fp == NULL) {
		fprintf(stderr, "Unable to open %s\n", pid_file);
		fclose(fp);
		free(file_contents);
		exit(1);
	}
	if (fread(file_contents, file_size, 1, fp) != 1) {
		fprintf(stderr, "Unable t read content of %s\n", pid_file);
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

	mon_status_t *head = _status_parse_json(value);
	if (!head) {
		fprintf(stderr, "Invalid status json\n");
		exit(1);
	}

	json_value_free(value);
	free(file_contents);

	return head;
}

static void
_drop_status_list(mon_status_t *st) {
	if (st) {
		mon_status_t *next = st->next;
		free(st);
		st = next;
	}
}

extern bool _entry_name_equal(json_object_entry *entry, char *mark);

static void
_value_string_copy(json_value *value, const char *output) {
	if (value && output && value->type == json_string) {
		strncpy((char *)output, value->u.string.ptr, value->u.string.length);
	}
}

mon_status_t*
_status_parse_json(json_value *value)
{
	if (value == NULL || value->type != json_object) {
		fprintf(stderr, "Invalid json file\n");
		return NULL;
	}

	mon_status_t *st = NULL;

	int length = value->u.object.length;
	for (int i=length-1; i>=0; i--) {
		json_object_entry *entry = &value->u.object.values[i];
		if (st) {
			mon_status_t *next = st;
			st = calloc(1, sizeof(mon_status_t));
			st->next = next;
		} else {
			st = calloc(1, sizeof(mon_status_t));
		}
		strncpy((char *)st->name, entry->name, entry->name_length);		
		json_value *v = entry->value;
		if (v->type == json_object) {
			int len = v->u.object.length;
			for (int j=0; j<len; j++) {
				json_object_entry *entry = &v->u.object.values[j];
				if (_entry_name_equal(entry, "time")) {
					st->time = entry->value->u.integer;
					continue;
				}
				if (_entry_name_equal(entry, "pid")) {
					st->pid = entry->value->u.integer;
					continue;
				}
			}
		}
	}

	return st;
}

static int
_alive(pid_t pid) {
  return 0 == kill(pid, 0);
}

void
mon_show_status(const char *pid_file) {
	struct timeval t;
	gettimeofday(&t, NULL);
	const time_t now = t.tv_sec;

	mon_status_t *head = _get_status_list(pid_file);
	mon_status_t *st = head;
	while (st) {
		time_t modified = st->time;
		time_t secs = now - modified;
		if (_alive(st->pid)) {
			char *str = milliseconds_to_long_string(secs * 1000);
			printf("\e[90m%s [%d]\e[0m : \e[32malive\e[0m : uptime %s\e[m\n", st->name, st->pid, str);
			free(str);
		} else {
			printf("\e[90m%s [%d]\e[0m : \e[31mdead\e[0m\n", st->name, st->pid);
		}
		st = st->next;
	}

	_drop_status_list(head);
}

int
mon_get_pid(mon_t *mon)
{
	mon_status_t *st = _get_status_list(mon->pidfile);
	int pid = st ? st->pid : 0;
	_drop_status_list(st);
	return pid;
}