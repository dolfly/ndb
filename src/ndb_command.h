/*
 * file   : ndb_cmd.h
 * author : ning
 * date   : 2014-08-02 10:03:51
 */

#ifndef _NDB_COMMAND_H_
#define _NDB_COMMAND_H_

#include "ndb.h"

typedef rstatus_t (*cmd_process_t)(struct conn*, msg_t *msg);

typedef struct command_s {
    char           *name;
    int             argc;
    cmd_process_t   proc;
} command_t;


rstatus_t command_init();
rstatus_t command_deinit();
rstatus_t command_process(struct conn* conn, msg_t *msg);

#endif
