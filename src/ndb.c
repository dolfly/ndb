/*
 * file   : ndb.c
 * author : ning
 * date   : 2014-07-28 14:51:07
 */

#include "ndb.h"

#define NDB_VERSION_STRING "0.0.1"

static struct option long_options[] = {
    { "help",           no_argument,        NULL,   'h' },
    { "version",        no_argument,        NULL,   'V' },
    { "conf",           required_argument,  NULL,   'c' },
    { NULL,             0,                  NULL,    0  }
};

static char short_options[] = "hVc:";

static void
ndb_show_usage(void)
{
    log_stderr(
        "Usage: ndb [-?hV] -c conf-file"
        "");
}

static void
ndb_show_version(void)
{
    log_stderr("ndb-" NDB_VERSION_STRING);
}

static rstatus_t
ndb_get_options(int argc, const char **argv, instance_t *instance)
{
    int c;

    opterr = 0;
    instance->configfile = NULL;

    for (;;) {
        c = getopt_long(argc, (char **)argv, short_options, long_options, NULL);
        if (c == -1) {
            /* no more options */
            break;
        }

        switch (c) {
        case 'h':
            ndb_show_usage();
            exit(0);

        case 'V':
            ndb_show_version();
            exit(0);

        case 'c':
            instance->configfile = optarg;
            break;

        case '?':
            switch (optopt) {
            case 'c':
                log_stderr("ndb: option -%c requires a file name",
                           optopt);
                break;
            }

            return NC_ERROR;

        default:
            log_stderr("ndb: invalid option -- '%c'", optopt);

            return NC_ERROR;
        }
    }

    if (instance->configfile == NULL) {
        return NC_ERROR;
    }
    return NC_OK;
}

static void
ndb_print_run(instance_t *instance)
{
    loga("ndb-%s started (pid %d)", NDB_VERSION_STRING, instance->pid);
}

static void
ndb_print_done(instance_t *instance)
{
    loga("ndb-%s done (pid %d)", NDB_VERSION_STRING, instance->pid);
}

#define KB *1024
#define MB *1024*1024

static rstatus_t
ndb_load_conf(instance_t *instance)
{
    rstatus_t status;

    status = nc_conf_init(&instance->conf, instance->configfile);
    if (status != NC_OK) {
        return status;
    }

    /* TODO: check invalid value */
    instance->daemonize                  = nc_conf_get_num(&instance->conf, "daemonize", false);
    instance->loglevel                   = nc_conf_get_num(&instance->conf, "loglevel", LOG_NOTICE);
    instance->logfile                    = nc_conf_get_str(&instance->conf, "logfile", "log/ndb.log");

    instance->srv.listen                 = nc_conf_get_str(&instance->conf, "listen", "0.0.0.0:5527");
    instance->srv.backlog                = nc_conf_get_num(&instance->conf, "backlog", 1024);
    instance->srv.mbuf_size              = nc_conf_get_num(&instance->conf, "mbuf_size", 512);

    instance->store.dbpath               = nc_conf_get_str(&instance->conf, "leveldb.dbpath", "data/db");
    instance->store.block_size           = nc_conf_get_num(&instance->conf, "leveldb.block_size", 32 KB);
    instance->store.cache_size           = nc_conf_get_num(&instance->conf, "leveldb.cache_size", 1 MB);
    instance->store.write_buffer_size    = nc_conf_get_num(&instance->conf, "leveldb.write_buffer_size", 1 MB);
    instance->store.compression          = nc_conf_get_num(&instance->conf, "leveldb.compression", 0);
    instance->store.read_verify_checksum = nc_conf_get_num(&instance->conf, "leveldb.read_verify_checksum", 0);
    instance->store.write_sync           = nc_conf_get_num(&instance->conf, "leveldb.write_sync", 0);

    instance->oplog.enable               = nc_conf_get_num(&instance->conf, "oplog.enable", true);
    instance->oplog.oplog_path           = nc_conf_get_str(&instance->conf, "oplog.path", "data/oplog");
    instance->oplog.oplog_segment_size   = nc_conf_get_num(&instance->conf, "oplog.segment_size", 1024*1024);
    instance->oplog.oplog_segment_cnt    = nc_conf_get_num(&instance->conf, "oplog.segment_cnt", 100);

    instance->repl.master                = NULL;
    instance->repl.repl_pos              = 0;
    instance->repl.connect_timeout       = nc_conf_get_num(&instance->conf, "repl.connect_timeout", 1000);
    instance->repl.connect_retry         = nc_conf_get_num(&instance->conf, "repl.connect_retry", 2);
    instance->repl.sleep_time            = nc_conf_get_num(&instance->conf, "repl.sleep_time", 200); /* 200 ms */

    return NC_OK;
}

#undef KB
#undef MB

static rstatus_t
ndb_init(instance_t *instance)
{
    rstatus_t status;

    status = log_init(instance->loglevel, instance->logfile);
    if (status != NC_OK) {
        return status;
    }

    if (instance->daemonize) {
        status = nc_daemonize(1);
        if (status != NC_OK) {
            return status;
        }
    }

    instance->pid = getpid();

    status = signal_init();
    if (status != NC_OK) {
        return status;
    }

    msg_init();
    command_init();
    cursor_init();

    status = store_init(instance, &instance->store);
    if (status != NC_OK) {
        return status;
    }

    status = oplog_init(instance, &instance->oplog);
    if (status != NC_OK) {
        return status;
    }

    status = repl_init(instance, &instance->repl);
    if (status != NC_OK) {
        return status;
    }

    status = server_init(instance, &instance->srv,
            ndb_conn_recv_done, ndb_conn_send_done);
    if (status != NC_OK) {
        return status;
    }

    status = job_init(instance);
    if (status != NC_OK) {
        return status;
    }

    ndb_print_run(instance);

    return NC_OK;
}

static rstatus_t
ndb_deinit(instance_t *instance)
{
    job_deinit();

    signal_deinit();

    msg_deinit();

    cursor_deinit();

    command_deinit();

    server_deinit(&instance->srv);

    store_deinit(&instance->store);

    oplog_deinit(&instance->oplog);

    repl_deinit(&instance->repl);

    ndb_print_done(instance);

    log_deinit();

    return NC_OK;
}

static rstatus_t
ndb_run(instance_t *instance)
{
    return server_run(&instance->srv);
}

int
main(int argc, const char **argv)
{
    instance_t instance;
    rstatus_t status;

    status = ndb_get_options(argc, argv, &instance);
    if (status != NC_OK) {
        ndb_show_usage();
        exit(1);
    }

    status = ndb_load_conf(&instance);
    if (status != NC_OK) {
        log_stderr("ndb: configuration file '%s' syntax is invalid",
                   instance.configfile);
        exit(1);
    }

    status = ndb_init(&instance);
    if (status != NC_OK) {
        exit(1);
    }

    status = ndb_run(&instance);
    if (status != NC_OK) {
        ndb_deinit(&instance);
        exit(1);
    }

    ndb_deinit(&instance);
    exit(1);
}
