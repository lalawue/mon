# mon_sched(1)

  group process supervisor, using a JSON config file, fork from https://github.com/tj/mon, depends on  https://github.com/udp/json-parser, and the origin `mon` was:

  Super-simple monitoring program.

  `mon` spawned from the needlessly complex
  frustration that tools like [monit](http://mmonit.com/monit/)
  provide, with their awkward DSLs and setup. `mon` is written
  in C, uses less than 400kb of memory, and is incredibly simple
  to set up.

## Usage

```

Usage: mon_sched [options | JSON_CONFIG]

Options:

  -v, 		                    output program version
  -h, 		                    output help information
  -s, <pidfile>                 check group child process status

```

## Example

  The most simple use of `mon_sched(1)` is to simply keep a command running:

```js
➜  mon_sched git:(master) ✗ ./mon_sched example/mon_group.json
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
mon1 [85593] : dead
mon2 [85782] : alive : uptime 2 seconds
```

## Config format

the 'cron' not supported now.

```js
{
        "name" : "mon_group",
        "logfile" : "mon_sched.log",
        "pidfile" : "mon_sched.pid",
        "daemon" : false,
        "mon1" : {
                "logfile": "log1.txt",
                "cmd": "example/program.sh",
                "attempts": 20,
		"sleep": 10,
		"on_error" : "",
		"on_restart" : "",
                "cron": "* * * * 2"
        },
        "mon2" : {
                "cmd": "example/program2.sh",
                "logfile": "log2.txt",
                "attempts": 30,
                "sleep": 5,
                "cron": "1 * * * *"
        }
}
```
You may daemonize mon_sched to set group 'daemon' key to true.

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

# License

  MIT

# Build Status

  [![Build Status](https://travis-ci.org/lalawue/mon_sched.png)](http://travis-ci.org/lalawue/mon_sched)
