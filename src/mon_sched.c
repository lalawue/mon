
//
// mon_sched.c
//
// Copyright (c) 2012 TJ Holowaychuk <tj@vision-media.ca>
//
// hacked by lalawue, http://github.com/lalawue

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include "ms.h"
#include "config.h"
#include "status.h"

/*
 * Program version.
 */

#define kVERSION "1.3.1"

static mon_t *g_mon;

/*
 * Logger.
 */

#define log(fmt, args...) \
	do { \
		printf("mon : " fmt "\n", ##args); \
		fflush(stdout); \
	} while(0)

/*
 * Output error `msg`.
 */

void
error(char *msg) {
  fprintf(stderr, "Error: %s\n", msg);
  exit(1);
}

/*
 * Return a timestamp in milliseconds.
 */

int64_t
timestamp() {
  struct timeval tv;
  int ret = gettimeofday(&tv, NULL);
  if (-1 == ret) return -1;
  return (int64_t) ((int64_t) tv.tv_sec * 1000 + (int64_t) tv.tv_usec / 1000);
}

/*
 * Write group info to `file`.
 */

void
write_pidfile() {
	mon_dump_status(g_mon);
}

/*
 * Read pid `file`.
 */

pid_t
read_pidfile() {
	return mon_get_pid(g_mon);
}

/*
 * Output status of `pidfile`.
 */

void
show_status_of(const char *pidfile) {
	mon_show_status(pidfile);
}

/*
 * Redirect stdio to `file`.
 */

void
redirect_stdio_to(const char *file) {
	int nullfd = open("/dev/null", O_RDONLY, 0);
	int logfd = nullfd;
	if (file && strncmp(file, "/dev/null", 9) != 0) {
		logfd = open(file, O_WRONLY | O_CREAT | O_APPEND, S_IRUSR | S_IWUSR);
	}

	if (-1 == logfd) {
		perror("open()");
		exit(1);
	}

	if (-1 == nullfd) {
		perror("open()");
		exit(1);
	}

	dup2(nullfd, 0);
	dup2(logfd, 1);
	dup2(logfd, 2);
}

/*
 * Graceful exit, signal process group.
 */

void
graceful_exit(int sig) {
  int status;
  pid_t pid = getpid();
  log("shutting down");
  log("kill(-%d, %d)", pid, sig);
  kill(-pid, sig);
  log("waiting for exit");
  waitpid(read_pidfile(), &status, 0);
  log("bye :)");
  exit(0);
}

/*
 * Daemonize the program.
 */

void
daemonize() {
  if (fork()) exit(0);

  if (setsid() < 0) {
    perror("setsid()");
    exit(1);
  }
}

/*
 * Invoke the --on-restart command.
 */

void
exec_restart_command(monitor_t *monitor, pid_t pid) {
  char buf[1024] = {0};
  snprintf(buf, 1024, "%s %d", monitor->on_restart, pid);
  log("%s on restart `%s`", monitor->name, buf);
  int status = system(buf);
  if (status) {
	  log("%s exit(%d)", monitor->name, status);
  }
}

/*
 * Invoke the --on-error command.
 */

void
exec_error_command(monitor_t *monitor, pid_t pid) {
  char buf[1024] = {0};
  snprintf(buf, 1024, "%s %d", monitor->on_error, pid);
  log("%s on error `%s`", monitor->name, buf);
  int status = system(buf);
  if (status) {
	  log("%s exit(%d)", monitor->name, status);
  }
}

/*
 * Return the ms since the last restart.
 */

int64_t
ms_since_last_restart(monitor_t *monitor) {
  if (0 == monitor->last_restart_at) return 0;
  int64_t now = timestamp();
  return now - monitor->last_restart_at;
}

/*
 * Check if the maximum restarts within 60 seconds
 * have been exceeded and return 1, 0 otherwise.
 */

int
attempts_exceeded(monitor_t *monitor, int64_t ms) {
	monitor->attempts++;
	monitor->clock -= ms;

	// reset
	if (monitor->clock <= 0) {
		monitor->clock = 60000;
		monitor->attempts = 0;
		return 0;
	}

	// all good
	if (monitor->attempts < monitor->max_attempts) {
		return 0;
	}

	return 1;
}

/*
 * find monitor with pid
 */

static monitor_t*
_monitor_with_pid(pid_t pid)
{
	monitor_t *m = g_mon->monitors;
	while (m) {
		if (pid == m->pid) {
			return m;
		}
		m = m->next_monitor;
	}
	return NULL;
}

/*
 * find monitor in sleep state, increase sleep time, return the close to restart one,
 * if no sleep monitor, return NULL
 */

static monitor_t*
_increase_sleep_monitor(mon_t *mon) {
	int max = 0x7fffffff;
	monitor_t *close_to_restart = NULL;
	monitor_t *m = mon->monitors;
	while (m) {
		if (m->sleepsec > 0) {
			m->sleepsec += 1;
			if (m->max_sleepsec - m->sleepsec < max) {
				max = m->max_sleepsec - m->sleepsec;
				close_to_restart = m;	
			}
		}
		m = m->next_monitor;
	}
	return close_to_restart;
}

/*
 * find cron monitor ready to run, return NULL otherwise
 * set in-active to cro
 */

static monitor_t*
_ready_cron_monitor(mon_t *mon, struct tm *ptm) {
	monitor_t *m = mon->monitors, *ready_m = NULL;
	for ( ; m; m=m->next_monitor) {
		if (!m->cron || m->pid > K_INVALID_MONITOR_PID) {
			continue;
		}
		if (cron_in_timearea(m->cron, ptm)) {
			if (!m->cron->has_running) {
				ready_m = m;
			}
		} else {
			cron_set_has_running(m->cron, false);
		}
	}
	return ready_m;
}

static struct tm*
_current_tm() {
	static struct tm current_tm;
	time_t ti = time(NULL);
	gmtime_r(&ti, &current_tm);
	return &current_tm;
}

static char*
_current_tm_string(struct tm *t) {
	static char buf[32];
	snprintf(buf, 32, "%d/%d/%d %d:%d",
			1900 + t->tm_year, t->tm_mon + 1, t->tm_mday, t->tm_hour, t->tm_min);
	return buf;
}

typedef void (*last_one_callback)();

/*
 * Monitor the given `cmd`.
 */

void
start(monitor_t *monitor, last_one_callback last_callback) {
monitor_exec: {
	if (monitor->cron) {
		// cron skip when not in time area, or has running before
		struct tm *t = _current_tm();
		if (!cron_in_timearea(monitor->cron, t) || monitor->cron->has_running) {
			log("%s cron skip %s", monitor->name, _current_tm_string(t));
			goto monitor_skip;
		}
	}

	pid_t pid = fork();

	if (pid == -1) {
		perror("fork()");
		exit(1);
	} else if (pid == 0)  {		
		signal(SIGTERM, SIG_DFL);
		signal(SIGQUIT, SIG_DFL);
		log("%s sh -c \"%s\"", monitor->name, monitor->cmd);
		execl("/bin/sh", "sh", "-c", monitor->cmd, 0);
		perror("execl()");
		exit(1);
	} else {
		monitor->pid = pid;
		cron_set_has_running(monitor->cron, true);
		log("%s pid %d at %s", monitor->name, pid, _current_tm_string(_current_tm()));

		monitor_skip: {
			if (!last_callback) {
				// callback at last one
				return;
			}
			last_callback();
		}

		// wait cron or sleep monitors
		minitor_wait: {
			int status;
			for (;;) {			
				sleep(1); // always sleep 1 seconds

				monitor = _ready_cron_monitor(g_mon, _current_tm());
				if (monitor) {
					goto monitor_exec;
				}

				pid = waitpid(-1, &status, WNOHANG);				
				if (pid > 0) {
					monitor = _monitor_with_pid(pid);
					monitor->status = status;
					goto monitor_signal;
				} 
				
				monitor = _increase_sleep_monitor(g_mon);
				if (monitor && (monitor->sleepsec > monitor->max_sleepsec)) {
					monitor->sleepsec = 0;
					goto monitor_error;
				}
			}
		}

		monitor_signal: {
			monitor->pid = K_INVALID_MONITOR_PID;
			if (WIFSIGNALED(monitor->status)) {
				log("%s signal(%s)", monitor->name, strsignal(WTERMSIG(monitor->status)));
				log("%s sleep(%d)", monitor->name, monitor->max_sleepsec);
				if (monitor->max_sleepsec > 1) {
					monitor->sleepsec = 1;
					goto minitor_wait;
				} else {
					sleep(1);
					goto monitor_error;
				}
			}
			if (WEXITSTATUS(monitor->status)) {
				log("%s exit(%d)", monitor->name, WEXITSTATUS(monitor->status));
				log("%s sleep(%d)", monitor->name, monitor->max_sleepsec);
				if (monitor->max_sleepsec > 1) {
					monitor->sleepsec = 1;
					goto minitor_wait;
				} else {
					sleep(1);
					goto monitor_error;
				}
			}
		} // monitor_signal
	}

	// restart
	monitor_error: {
		monitor->pid = K_INVALID_MONITOR_PID;
		int64_t ms = ms_since_last_restart(monitor);
		bool exit_normal = false; // for no cron
		if (monitor->cron) {
			// for cron one, check exit code
			exit_normal = (WIFSIGNALED(monitor->status) | WEXITSTATUS(monitor->status)) == 0;
		}
		if (exit_normal || attempts_exceeded(monitor, ms)) {
			if (!exit_normal) {
				char *time = milliseconds_to_long_string(60000 - monitor->clock);
				log("%s %d restarts within %s, bailing", monitor->name, monitor->max_attempts, time);
				if (monitor->on_error) {
					exec_error_command(monitor, pid);
				}
			}
			if (mon_monitor_try_remove(g_mon, monitor)) {
				log("%s bye :)", monitor->name);
			} else {
				mon_monitor_reset(monitor);
			}
			// if no monitors
			if (!g_mon->monitors) {
				log("%s exit, no monitors", g_mon->name);
				exit(2);
			}
		} else {
			log("%s %d attempts remaining", monitor->name, monitor->max_attempts - monitor->attempts);			
			if (monitor->on_restart) {
				exec_restart_command(monitor, pid);
			}
			monitor->last_restart_at = timestamp();
			log("%s last restart %s ago", monitor->name, milliseconds_to_long_string(ms));
		}
		goto monitor_exec;
	}
}}

/*
 * [options] <cmd>
 */

static void
_show_help(char *app_name) {
	printf("Usage:\n");
	printf("%s -r config_json, run with group config\n", app_name);
	printf("%s -v,\t\tshow version\n", app_name);
	printf("%s -h,\t\tshow help\n", app_name);
	printf("%s -s pid_file, show group pid status\n", app_name);
}

int
main(int argc, char *argv[]) {
	if (argc < 3) {
		_show_help(argv[0]);
		return 0;
	}

	if (strcmp(argv[1], "-v") == 0) {
		printf("mon_sched %s\n", kVERSION);
		return 0;
	}

	if (strcmp(argv[1], "-h") == 0) {
		_show_help(argv[0]);
		return 0;
	}

	if (strcmp(argv[1], "-s") == 0) {
		show_status_of(argv[2]);
		return 0;
	}

	if (strcmp(argv[1], "-r") != 0) {
		_show_help(argv[0]);
		return 0;
	}

	// crete global mon instance
	g_mon = mon_create(argv[2]);
	if (g_mon == NULL) {
		return 0;
	} else {
		g_mon->time = time(NULL);
	}
	
	// signals
	signal(SIGTERM, graceful_exit);
	signal(SIGQUIT, graceful_exit);

	// daemonize
  	if (g_mon->daemon) {
    	daemonize();
    	redirect_stdio_to(g_mon->logfile);
  	}

	last_one_callback callback = g_mon->pidfile ? write_pidfile : NULL;

	monitor_t *m = g_mon->monitors;
	while (m) {
	  	start(m, !m->next_monitor ? callback : NULL);
		m = m->next_monitor;
	}

	return 0;
}
