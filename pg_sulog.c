/*------------------------------------------------------------------------------
 * pg_sulog.c
 *
 * PostgreSQL hook sample
 *
 * author @nuko_yokohama
 *
 *------------------------------------------------------------------------------
 */
#include "postgres.h"

#include "executor/executor.h"
#include "executor/spi.h"
#include "miscadmin.h"
#include "libpq/auth.h"
#include "tcop/utility.h"
#include "utils/builtins.h"
#include "utils/guc.h"
#include "utils/memutils.h"
#include "utils/timestamp.h"
#include "libpq/auth.h"
#include "port.h"


#include "pgtime.h"
#include <time.h>

PG_MODULE_MAGIC;

#define SU_LIST_MAX  64
#define BUFFER_SIZE NAMEDATALEN * 4

bool creatingFlag = false;

bool sulogDisableCommand = false;
char* sulogMode = "LOGGING";

int suNum;
char suList[SU_LIST_MAX][BUFFER_SIZE];

void _PG_init(void);
static int createSuperuserList(void);
static void debugPrintSuperuserList(void);
static void clearSuperuserList(void);
static bool isSuperuser();
static bool checkSuperuser(char* username);

static void write_query_sulog(bool mode, const char* query);


/*
  Judge current user is superuser role 
*/
static bool isSuperuser() {
    char* user_name;
    if (MyProcPort == NULL) 
        return false;
    user_name = MyProcPort->user_name;

    // prototype
    if ( user_name == NULL || *user_name == '\0' )
        return false;

    if ( checkSuperuser(user_name)) {
        return true;
    } else {
        return false;
    }
}

static bool checkSuperuser (char* user_name) {
    int i;
    for (i = 0; i < suNum; i++) {
        if ( !strcmp(suList[i], user_name)) {
            /* user_name is superuser. */
            return true;
        }
    }
    return false;
}

static int createSuperuserList() {
    const char *command = "SELECT rolname FROM pg_roles WHERE rolsuper = true";
    bool ret = false;
    int proc;

    creatingFlag = true;

    elog(DEBUG1, "SPI_CONNECT\n");
    SPI_connect();
    elog(DEBUG1, "SPI_exec\n");
    ret = SPI_exec(command, 0);
    elog(DEBUG1, "SPI_processed\n");
    proc = SPI_processed;
    proc = (proc > SU_LIST_MAX) ? SU_LIST_MAX : proc;

    if (ret > 0 && SPI_tuptable != NULL)
    {
        TupleDesc tupdesc = SPI_tuptable->tupdesc;
        SPITupleTable *tuptable = SPI_tuptable;
        int j;

        for (j = 0; j < proc; j++)
        {
            HeapTuple tuple = tuptable->vals[j];
            strcpy(suList[j], SPI_getvalue(tuple, tupdesc, 1));
        }
        suNum = proc;
        debugPrintSuperuserList();
    }

    SPI_finish();
    return proc;
}

static void debugPrintSuperuserList(void) {
    int i;
    elog(DEBUG1, "pg_sulog: suNum=%d", suNum);
    for (i = 0; i < suNum; i++) {
       elog(DEBUG1, "pg_sulog: suList[%d]=%s", i, suList[i]);
    }
}

static void clearSuperuserList(void) {
    int i;
    for (i = 0; i < suNum; i++) {
        strcpy(suList[i], "");
    }
    suNum = 0;
    creatingFlag = false;
}

static bool
toUpperCompare(char* s1, const char* s2) {
    int i = 0;
    int l;

    l = strlen(s1);
    for (i=0; i < l; i++) {
       if ( toupper( *(s1 + i)) == toupper( *(s2 + i)) ) {
           continue;
       } else {
           return false;
       }
    }
    return true;
}

static bool
isMaintenanceCommand(const char* query) {
    /* TODO: custmize configuration? */
    if (toUpperCompare("VACUUM", query))
        return true;
    if (toUpperCompare("ANALYZE", query))
        return true;
    if (toUpperCompare("REINDEX", query))
        return true;
    if (toUpperCompare("CLUSTER", query))
        return true;

    /* else, not Maintenance command. */
    return false;
}

/*
 * Hook functions
 */
static ProcessUtility_hook_type next_ProcessUtility_hook = NULL;
static ExecutorRun_hook_type next_ExecutorRun_hook = NULL;

/*
 * Hook ExecutorRun to logging for DML.
 */
static void
pg_sulog_ExecutorRun_hook(QueryDesc *queryDesc, ScanDirection direction, long count)
{
    bool suFlag = false;

    elog(DEBUG1, "pg_sulog: pg_sulog_ExecutorRun_hook start, creatingFlag = %d", creatingFlag);
    debugPrintSuperuserList();
    if (suNum == 0 && (creatingFlag == false))
        createSuperuserList(); // only 1 time called

    suFlag = isSuperuser();
    if (suFlag) {
        if ( strcmp(sulogMode, "LOGGING")) {
            /* superuser role logging (blocked) */
            write_query_sulog(true, queryDesc->sourceText);
        } else {
            /* superuser role logging */
            write_query_sulog(false, queryDesc->sourceText);
        }
    }

    debugPrintSuperuserList();
    clearSuperuserList();

    /* Call the previous hook or standard function */
    if (next_ExecutorRun_hook)
        next_ExecutorRun_hook(queryDesc, direction, count);
    else {
        if ( suFlag && strcmp(sulogMode, "LOGGING") )  {
            // standard_ExecutorRun(queryDesc, eflags);
        } else {
            standard_ExecutorRun(queryDesc, direction, count);
        }
    }

}

/*
 * Hook ProcessUtility to logging for DDL and utility commands.
 */
static void
pg_sulog_ProcessUtility_hook(Node *parsetree,
                             const char *queryString,
                             ProcessUtilityContext context,
                             ParamListInfo params,
                             DestReceiver *dest,
                             char *completionTag)
{
    bool suFlag;
    bool maintenanceFlag;

    elog(DEBUG1, "pg_sulog: pg_sulog_ProcessUtility_hook sulogMode = %s\n", sulogMode); 
    elog(DEBUG1, "pg_sulog: pg_sulog_ProcessUtility_hook completionTag = %s\n", completionTag); 
    elog(DEBUG1, "pg_sulog: pg_sulog_ProcessUtility_hook queryString= %s\n", queryString); 
    if (params)
        elog(DEBUG1, "pg_sulog: pg_sulog_ProcessUtility_hook params->numParams= %d\n", params->numParams); 

    debugPrintSuperuserList();
    if (suNum == 0 && (creatingFlag == false))
        createSuperuserList(); // only 1 time called

    debugPrintSuperuserList();
    suFlag = isSuperuser();
    elog(DEBUG1, "pg_sulog: pg_sulog_ProcessUtility_hook suFlag = %d, suLogmode= %s\n", suFlag, sulogMode); 

    if ( !strcmp(sulogMode, "MAINTENANCE")) {
        maintenanceFlag = isMaintenanceCommand(queryString);
    } else {
        maintenanceFlag = false;
    }

    if (suFlag) {
        if (( strcmp(sulogMode, "LOGGING") ) && !maintenanceFlag ) {
            /* superuser role logging (blocked) */
            write_query_sulog(true, queryString);
        } else {
            /* superuser role logging */
            write_query_sulog(false, queryString);
        }
    }

    debugPrintSuperuserList();
    clearSuperuserList();
    /* Call the standard process utility chain. */
    if (next_ProcessUtility_hook)
        (*next_ProcessUtility_hook) (parsetree, queryString, context,
                                     params, dest, completionTag);
    else {
        if ( suFlag && ( strcmp(sulogMode, "LOGGING_ONLY") ) && !maintenanceFlag ) {
            // block
        } else {
            standard_ProcessUtility(parsetree, queryString, context,
                                    params, dest, completionTag);
        }
    }
}

/*
 * write pg_sulog to server log
 */
static void write_query_sulog(bool block, const char* query) {

    pg_time_t       stamp_time = (pg_time_t) time(NULL);
    char            strfbuf[128];  // timestamp

    /* create PostgreSQL timestamp format string. */
    pg_strftime(strfbuf, sizeof(strfbuf),
        "%Y-%m-%d %H:%M:%S %Z",
        pg_localtime(&stamp_time, log_timezone));

        ereport(WARNING, (errmsg("pg_sulog: %s [%s] user=%s %s",
            strfbuf,
            (block == true) ? "blocked" : "logging" ,
            MyProcPort->user_name, query)));
}


void
_PG_init(void)
{
    /* Must be loaded with shared_preload_libaries */
    if (IsUnderPostmaster)
        ereport(ERROR, (errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE),
                errmsg("pg_sulog must be loaded via shared_preload_libraries")));

    DefineCustomBoolVariable(
        "pg_sulog.block",
        "disable superuser command .",

        NULL,
        &sulogDisableCommand,
        false,
        PGC_SUSET,
        GUC_NOT_IN_SAMPLE,
        NULL, NULL, NULL);

    DefineCustomStringVariable(
        "pg_sulog.mode",
        "set blocking mode.",
        "set blocking mode.(LOGGING_ONLY,MAINTENANCE,BLOCK)",
        &sulogMode,
        "LOGGING",

        PGC_SUSET,
        GUC_NOT_IN_SAMPLE,
        NULL, NULL, NULL);
    creatingFlag = false;

    /*
     * Install our hook functions after saving the existing pointers to
     * preserve the chains.
     */
    next_ExecutorRun_hook = ExecutorRun_hook;
    ExecutorRun_hook = pg_sulog_ExecutorRun_hook;

    next_ProcessUtility_hook = ProcessUtility_hook;
    ProcessUtility_hook = pg_sulog_ProcessUtility_hook;

    /* Log that the extension has completed initialization */
    ereport(LOG, (errmsg("pg_sulog: initialized. mode=%s", sulogMode)));

}

