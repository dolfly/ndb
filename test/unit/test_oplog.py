#!/usr/bin/env python
#coding: utf-8

import common
from common import *
from redis import Connection
import struct
import StringIO

ndb2 = NDB('127.0.0.5', 5529, '/tmp/r/ndb-5529/', {'loglevel': T_VERBOSE})

def setup():
    print 'xxxxx setup'
    common.setup()
    ndb2.deploy()
    ndb2.start()

def teardown():
    print 'xxxxx teardown'
    common.teardown()
    assert(ndb2._alive())
    ndb2.stop()

def test_oplog():
    k = 'kkkkk'
    v = 'vvvvv'
    ndb.stop()
    ndb.clean()
    ndb.deploy()
    ndb.start()

    conn = get_conn(flush = False)

    info = conn.info()
    assert(info['oplog.first'] == 0)
    assert(info['oplog.last'] == 0)

    last_oplog = info['oplog.last']

    #set
    rst = conn.set(k, v)
    op = conn.getop(last_oplog + 1)
    print op
    assert(op[0] == ['SET', k, v, '0'])

    info = conn.info()
    assert(info['oplog.first'] == 0)
    assert(info['oplog.last'] == 1)

    #expire
    rst = conn.expire(k, 10)
    op = conn.getop(last_oplog + 2)
    cmd, key, val, expire = op[0]
    assert (cmd, key, val) == ('SET', k, v)
    assert(abs(1000 * (time.time() + 10) - int(expire)) < 10)

    # print int(1000 * (time.time() + 10))
    # print expire

    #del
    rst = conn.delete(k)
    op = conn.getop(last_oplog + 3)
    assert(op[0] == ['DEL', k])

    op = conn.getop(last_oplog + 4)
    assert op == None

    op = conn.getop(last_oplog + 1)
    print op
    assert len(op) == 3

    op = conn.getop(last_oplog + 1, 2)
    print op
    assert len(op) == 2

def _get_all_keys(conn):
    all_keys = []
    cursor = '0'
    while True:
        cursor, keys = conn.scan(cursor)
        all_keys = all_keys + keys

        if '0' == cursor:
            break
    return all_keys

def test_repl():
    conn = get_conn()
    conn2 = get_conn(ndb2)

    kv = {'kkk-%s' % i : 'vvv-%s' % i for i in range(12)}
    for k, v in kv.items():
        conn.set(k, v)
        conn.expire(k, 100)

    conn2.slaveof('%s:%s' % (ndb.host(), ndb.port()))

    time.sleep(2)
    print _get_all_keys(conn)
    print _get_all_keys(conn2)
    assert(_get_all_keys(conn) == _get_all_keys(conn2))

    #new write
    conn.set('new-key', 'new-val')
    time.sleep(1)
    print _get_all_keys(conn)
    print _get_all_keys(conn2)
    # time.sleep(500)
    assert(_get_all_keys(conn) == _get_all_keys(conn2))

    #new write
    conn.delete('new-key')
    time.sleep(1)
    assert(_get_all_keys(conn) == _get_all_keys(conn2))

    print 'done'

def test_repl_master_restart():
    conn = get_conn()
    conn2 = get_conn(ndb2)

    kv = {'kkk-%s' % i : 'vvv-%s' % i for i in range(12)}
    for k, v in kv.items():
        conn.set(k, v)
        conn.expire(k, 100)

    conn2.slaveof('%s:%s' % (ndb.host(), ndb.port()))

    time.sleep(2)
    assert(_get_all_keys(conn) == _get_all_keys(conn2))

    ndb.stop()
    ndb.start()
    conn = get_conn(flush = False)

    #new write
    conn.set('new-key', 'new-val')
    time.sleep(1)
    print _get_all_keys(conn)
    print _get_all_keys(conn2)
    # time.sleep(500)
    assert(_get_all_keys(conn) == _get_all_keys(conn2))

    ndb.stop()
    ndb.start()
    conn = get_conn(flush = False)
    #new write
    conn.delete('new-key')
    time.sleep(1)
    assert(_get_all_keys(conn) == _get_all_keys(conn2))

    print 'done'


