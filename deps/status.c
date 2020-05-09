//
// status.c
//
// Copyright (c) 2020 lalawue
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
#include "json_file.h"

#define kBufSize 2048

static inline void
_dummy_write(int fd, const void *buf, size_t nbyte) {
    ssize_t ret = write(fd, buf, nbyte);
    (void)ret;
}

static void
_dump_monitor(monitor_t *monitor, int fd, int dump_all) {
    if (!monitor) {
        return;
    }
    const time_t ti = time(NULL);
	unsigned char buf[kBufSize];
    monitor_t *m = monitor;
    while (m) {
        // write child process start time and pid
        time_t mti = m->last_restart_at / 1000;
        int bytes = snprintf((char *)buf, kBufSize, "\t\"%s\" : {\n\t\t\"time\" : %ld,\n\t\t\"pid\" : %d\n\t}",
                            m->name, mti ? mti : ti, m->pid);
        _dummy_write(fd, buf, bytes);
        m = m->next_monitor;
        if (m && dump_all) {
            _dummy_write(fd, ",\n", 2);
        } else {
            _dummy_write(fd, "\n", 1);
            break;
        }
    }
}

/** dump status as JSON
 */
void
mon_dump_group(mon_t *mon) {
	if (!mon || !mon->pidfile) {
		perror("dump status ");
		return;
	}

	int fd = open(mon->pidfile, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
	if (fd < 0) {
        perror("failed to open file");
		return;
	}
    
    unsigned char buf[kBufSize];
	_dummy_write(fd, "{\n", 2);
	// write group start time and pid
	int bytes = snprintf((char *)buf, kBufSize, "\t\"%s\" : {\n\t\t\"time\" : %ld,\n\t\t\"pid\" : %d\n\t},\n",
                        mon->name, mon->time, getpid());
	_dummy_write(fd, buf, bytes);
    _dump_monitor(mon->monitors, fd, 1);
	_dummy_write(fd, "}\n", 2);
	close(fd);
}

void
mon_dump_monitor(monitor_t *monitor) {
    if (!monitor || !monitor->pidfile) {
        return;
    }
  	int fd = open(monitor->pidfile, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
	if (fd < 0) {
        perror("failed to open file for monitor");
		return;
	}
    _dummy_write(fd, "{\n", 2);
    _dump_monitor(monitor, fd, 0);
    _dummy_write(fd, "}\n", 2);
    close(fd);
}

mon_status_t* _status_parse_json(json_value *value);

static mon_status_t*
_get_status_list(const char *pid_file) {
    json_file_t jf;

    if (pid_file == NULL) {
		return NULL;
	}

    if (!json_file_load(pid_file, &jf)) {
        exit(1);
    }

	mon_status_t *head = _status_parse_json(jf.json);
	if (!head) {
		fprintf(stderr, "Invalid status json\n");
		exit(1);
	}

    json_file_destroy(&jf);
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
		// read process config name
		strncpy((char *)st->name, entry->name, entry->name_length);		
		json_value *v = entry->value;
		if (v->type == json_object) {
			int len = v->u.object.length;
			for (int j=0; j<len; j++) {
				// read process start time and pid
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
  return (pid >= 0) && (0 == kill(pid, 0));
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

/** get the first pid of pid file, it may be group
 */
mon_status_t*
mon_status_list(const char *pidfile) {
    return _get_status_list(pidfile);
}

void
mon_status_destroy(mon_status_t *list) {
    _drop_status_list(list);
}