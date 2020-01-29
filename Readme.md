# mon_sched(1)

  group process supervisor, fork from https://github.com/tj/mon, and `mon` was:

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

  The most simple use of `mon(1)` is to simply keep a command running:

```js
$ mon example/mon_group.json
mon : mon1 pid 43805
mon : mon1 sh -c "example/program.sh"
mon : mon2 pid 43807
mon : mon2 sh -c "example/program2.sh"
11111
one
two
22222
three
33333
exiting
mon : mon2 exit(1)
mon : mon2 sleep(5)
```

## JSON format

```js
{
        "name" : "mon_group",
        "logfile" : "mon_log.log",
        "pidfile" : "mon_pid.pid",
        "daemon" : false,
        "mon1" : {
                "logfile": "log1.txt",
                "cmd": "example/program.sh",
                "attempts": 20,
                "sleep": 10,
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
You may daemonize mon_sched to set JSON config 'daemon' key to true.

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
