
[![MIT licensed][1]][2]  [![Build Status][3]][4]

[1]: https://img.shields.io/badge/license-MIT-blue.svg
[2]: LICENSE

[3]: https://travis-ci.org/lalawue/mon_sched.svg?branch=master
[4]: https://travis-ci.org/lalawue/mon_sched

# mon_sched(1)

  group process supervisor, using a JSON config file, support cron style invoke interval, fork from https://github.com/tj/mon, depends on  https://github.com/udp/json-parser, and the origin `mon` was:

  Super-simple monitoring program.

  `mon` spawned from the needlessly complex
  frustration that tools like [monit](http://mmonit.com/monit/)
  provide, with their awkward DSLs and setup. `mon` is written
  in C, uses less than 400kb of memory, and is incredibly simple
  to set up.

## Usage

```

Usage: mon_sched options

Options:

  -r, <sched_json>              run group process from config
  -v,                           show version
  -h,                           show help
  -s, <pid_json>                show group and child status
  -k, <pid_json> [child_name]   kill [group | child] process
```

## Example

  The most simple use of `mon_sched(1)` is to simply keep a command running:

```js
➜  mon_sched git:(master) ✗ ./mon_sched -r example/mon_group.json
mon : mon1 pid 85415
mon : mon2 pid 85416
mon : mon1 sh -c "example/program.sh"
mon : mon2 sh -c "example/program2.sh"
11111
one
22222
two
33333
three
exiting
mon : mon2 exit(1)
mon : mon2 sleep(5)
exiting
mon : mon1 exit(1)
mon : mon1 sleep(10)
mon : mon2 last restart less than one second ago
mon : mon2 30 attempts remaining
mon : mon2 pid 85435
mon : mon2 sh -c "example/program2.sh"
```

  Check group process status

```js
➜  mon_sched git:(master) ✗ ./mon_sched -s mon_sched.pid
mon_group [85592] : alive : uptime 12 seconds
mon1 [-1] : dead
mon2 [85782] : alive : uptime 2 seconds
```

## Config format

```js
{
	"name" : "mon_group",
	"logfile" : "mon_sched.log",
	"pidfile" : "mon_sched.pid",
	"daemon" : false,
	"mon1" : {
		"pidfile": "example/mon1.pid",
		"cmd": "example/program.sh",
		"attempts": 1,
		"sleep": 2,
		"cron": "*/2 * * * *"
	},
	"mon2" : {
		"pidfile": "example/mon2.pid",
		"cmd": "example/program2.sh",
		"attempts": 2,
		"sleep": 3,
		"cron": "*/3 * * * *"
	}
}
```
You may daemonize mon_sched to set group 'daemon' key to true.

and the 'cron' entry format like:

```js
"0,30,45   */3     2,[4-9]     *       *"
# minius   hour    month_day   month   weak_day
# (0-59)   (0-23)  (1-31)      (0-11)  (0-6)
```

## Failure alerts

 `mon_sched(1)` will continue to attempt restarting your program unless the maximum number
 of `attempts` has been exceeded within 60 seconds. Each time a restart is performed
 the `on-restart` command is executed, and when `mon_sched(1)` finally bails the `on-error`
 command is then executed before mon itself exits and gives up.

  For example the following will echo "hey" three times before mon realizes that
  the program is unstable, since it's exiting immediately, thus finally invoking
  `./email.sh`, or any other script you like.

## Logs

  By default `mon_sched(1)` logs to stdio, however when daemonized it will default
  to writing to `/dev/null`.

## Signals

  - __SIGQUIT__ graceful shutdown
  - __SIGTERM__ graceful shutdown

