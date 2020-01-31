
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

#define kVERSION "1.3.0"

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
		logfd = open(file, O_WRONLY | O_CREAT | O_APPEND, 0755);  
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
  if (monitor->attempts < monitor->max_attempts) return 0;

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
 */

static monitor_t*
_ready_cron_monitor(mon_t *mon, struct tm *ptm) {
	monitor_t *m = NULL;
	cron_t *cron = mon->crons;
	while (cron) {
		m = (monitor_t *)cron->opaque;
		if (cron_should_invoke(cron, ptm) && m->pid == 0) {
			return m;
		}
		cron = cron->next;
	}
	return NULL;
}

static struct tm*
_current_tm() {
	static struct tm current_tm;
	time_t ti = time(NULL);
	gmtime_r(&ti, &current_tm);
	return &current_tm;
}

typedef void (*last_one_callback)();

/*
 * Monitor the given `cmd`.
 */

void
start(monitor_t *monitor, struct tm *start_tm, last_one_callback last_callback) {
monitor_exec: {	
	if (monitor->cron && !cron_should_invoke(monitor->cron, start_tm)) {
		goto monitor_skip;
	}

	pid_t pid = fork();
	int status;

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
		log("%s pid %d", monitor->name, pid);
		monitor->pid = pid;

		monitor_skip: {
			if (!last_callback) {
				// callback at last one
				return;
			}
			last_callback();
			last_callback = NULL;
		}

		// wait cron or sleep monitors
		minitor_wait: {
			monitor_t *sleep_m = NULL;
			monitor_t *cron_m = NULL;
			struct tm *ptm = _current_tm();
			while ((sleep_m = _increase_sleep_monitor(g_mon)) || g_mon->crons)
			{
				// always sleep 1 seconds
				sleep(1);

				cron_m = _ready_cron_monitor(g_mon, ptm);
				if (cron_m) {
					monitor = cron_m;
					goto monitor_exec;
				}

				pid_t tmp_pid = waitpid(-1, &status, WNOHANG);
				
				if (tmp_pid > 0) {
					pid = tmp_pid;
					monitor = _monitor_with_pid(pid);
					goto monitor_signal;
				} 
				
				if (sleep_m && (sleep_m->sleepsec >= sleep_m->max_sleepsec)) {
					sleep_m->sleepsec = 0;
					monitor = sleep_m;
					goto monitor_error;
				}
			}
		}
					
		// no cron definition, or sleep monitors, wait any child exit
		pid = waitpid(-1, &status, 0);
		monitor = _monitor_with_pid(pid);
		if (monitor == NULL) {
			return;
		}

		monitor_signal: {
			monitor->pid = 0; // reset
			if (WIFSIGNALED(status)) {
				log("%s signal(%s)", monitor->name, strsignal(WTERMSIG(status)));
				log("%s sleep(%d)", monitor->name, monitor->max_sleepsec);
				sleep(1);					
				if (monitor->max_sleepsec > 1) {
					monitor->sleepsec = 1;
					goto minitor_wait;
				} else {
					goto monitor_error;
				}
			}			
			if (WEXITSTATUS(status)) {
				log("%s exit(%d)", monitor->name, WEXITSTATUS(status));
				log("%s sleep(%d)", monitor->name, monitor->max_sleepsec);
				sleep(1);					
				if (monitor->max_sleepsec > 1) {
					monitor->sleepsec = 1;
					goto minitor_wait;
				} else {
					goto monitor_error;
				}
			}
		} // monitor_signal
	}

	// restart
	monitor_error: {
		monitor->pid = 0; // reset
		if (monitor->on_restart) {
			exec_restart_command(monitor, pid);
		}
		int64_t ms = ms_since_last_restart(monitor);
		monitor->last_restart_at = timestamp();
		log("%s last restart %s ago", monitor->name, milliseconds_to_long_string(ms));
		log("%s %d attempts remaining", monitor->name, monitor->max_attempts - monitor->attempts);
		if (attempts_exceeded(monitor, ms)) {
			char *time = milliseconds_to_long_string(60000 - monitor->clock);
			log("%s %d restarts within %s, bailing", monitor->name, monitor->max_attempts, time);
			if (monitor->on_error) {
				exec_error_command(monitor, pid);
			}
			log("%s bye :)", monitor->name);
			exit(2);
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
	printf("%s JSON_CONFIG\n", app_name);
	printf("%s -v,\t\tshow version\n", app_name);
	printf("%s -h,\t\tshow help\n", app_name);
	printf("%s -s pidfile,\tshow status\n", app_name);
}

int
main(int argc, char *argv[]) {
	if (argc < 2) {
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
		if (argc > 2) {
			show_status_of(argv[2]);
		} else {
			_show_help(argv[0]);
		}
		return 0;
	}

	// crete global mon instance
	g_mon = mon_create(argv[1]);
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
	struct tm *ptm = _current_tm();

	monitor_t *m = g_mon->monitors;
	while (m) {
	  	start(m, ptm, (!m->next_monitor ? callback : NULL));
		m = m->next_monitor;
	}

	return 0;
}
