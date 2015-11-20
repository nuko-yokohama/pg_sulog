# PostgreSQL Super user logging Extension

The PostgreSQL simple logging extension (`pg_sulog`) provides supe ruser role's operation logging for server log.
And super user role's operation blocking.

```
$ psql -U postgres sampledb -t -c "SELECT 1;"
WARNING:  pg_sulog: 2015-11-19 21:51:20 JST [logging] user=postgres SELECT 1
```

## Settings

### shared_preload_libraries

```
shared_preload_libraries = pg_sulog
```

### pg_sulog.block

* 'on', super user role's all operation is blocked and logged.
* 'off', super user role's all operation is logged.

pg_sulog.block = 'on' example.

```
$ psql -U postgres sampledb -t -c "SELECT 3;"
WARNING:  pg_sulog: 2015-11-19 22:51:28 JST [blocked] user=postgres SELECT 3;
SELECT 0
$ 
```

The default is `off`.

## Validated environment

* OS
  * CentOS 7
* PostgreSQL
  * 9.5-beta2
  * 9.4
  * 9.3

## Todo

* Output format customize.
* Other platform verification.
  * 9.6 (not work)
* Regression test
* Stability verification

## Using hook interface

* Executor_Run_hook
* ProcessUtility_hook

## Authors

@nuko_yokohama (https://twitter.com/nuko_yokohama)

