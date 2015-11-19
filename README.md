# PostgreSQL Super user logging Extension

The PostgreSQL simple logging extension (`pg_sulog`) provides supe ruser role's operation logging.
And super user role's operation blocking.

```
pg_sulog: 2015-11-19 21:51:20 JST [logging] user=postgres SELECT 1
```

## Settings

### pgsulog.block

'on', super user role's all operation is blocked.

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

## Todo

* Output format customize.
* Other platform verification.

## Using hook interface

* Executor_Run_hook
* ProcessUtility_hook

## Authors

@nuko_yokohama (https://twitter.com/nuko_yokohama)

