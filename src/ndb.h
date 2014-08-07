/*
 * file   : ndb.h
 * author : ning
 * date   : 2014-07-31 16:49:17
 */

#ifndef _NDB_H_
#define _NDB_H_

typedef struct instance_s instance_t;

#include "nc_util.h"
#include "ndb_message.h"
#include "ndb_leveldb.h"
#include "ndb_command.h"
#include "ndb_job.h"
#include "ndb_ttl.h"

struct instance_s {
    server_t            srv;
    store_t             store;

    bool                daemonize;
    int                 loglevel;                   /* log level */
    char                *logfile;                   /* log filename */
    pid_t               pid;                        /* process id */

    char                *configfile;                /* configuration filename */
    nc_conf_t           conf;
};

#endif

