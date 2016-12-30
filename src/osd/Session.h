// -*- mode:C++; tab-width:8; c-basic-offset:2; indent-tabs-mode:t -*-
// vim: ts=8 sw=2 smarttab
/*
 * Ceph - scalable distributed file system
 *
 * Copyright (C) 2004-2006 Sage Weil <sage@newdream.net>
 *
 * This is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License version 2.1, as published by the Free Software
 * Foundation.  See file COPYING.
 *
 */

#ifndef CEPH_OSD_SESSION_H
#define CEPH_OSD_SESSION_H

#include "common/RefCountedObj.h"
#include "common/Mutex.h"
#include "include/Spinlock.h"
#include "OSDCap.h"
#include "Watch.h"
#include "OSDMap.h"

struct Session;
typedef boost::intrusive_ptr<Session> SessionRef;
struct Backoff;
typedef boost::intrusive_ptr<Backoff> BackoffRef;
class PG;
typedef boost::intrusive_ptr<PG> PGRef;

/*
 * A Backoff represents one instance of either a PG or an OID
 * being plugged at the client.  It's refcounted and linked from
 * the PG {pg_oid}_backoffs map and from the client Session
 * object.
 *
 * The Backoff has a lock that protects it's internal fields.
 *
 * The PG has a backoff_lock that protects it's maps to Backoffs.
 * This lock is *inside* of Backoff::lock.
 *
 * The Session has a backoff_lock that protects it's map of pg and
 * oid backoffs.  This lock is *inside* the Backoff::lock *and*
 * PG::backoff_lock.
 *
 * That's
 *
 *    Backoff::lock
 *       PG::backoff_lock
 *         Session::backoff_lock
 *
 * When the Session goes away, we move our backoff lists aside,
 * then we lock each of the Backoffs we
 * previously referenced and clear the Session* pointer.  If the PG
 * is still linked, we unlink it, too.
 *
 * When the PG clears the backoff, it will send an unblock message
 * if the Session* is still non-null, and unlink the session.
 *
 */

struct Backoff : public RefCountedObject {
  Mutex lock;
  // NOTE: the owning PG and session are either *both* set or both null.
  PGRef pg;             ///< owning pg
  SessionRef session;   ///< owning session
  boost::optional<pg_t> pgid;
  boost::optional<hobject_t> oid;
  ceph_tid_t first_tid = 0;
  uint32_t first_attempt = 0;

  Backoff(PGRef pg, SessionRef s, pg_t p, ceph_tid_t t, uint32_t a)
    : RefCountedObject(g_ceph_context, 0),
      lock("Backoff::lock"),
      pg(pg),
      session(s),
      pgid(p),
      first_tid(t),
      first_attempt(a) {}
  Backoff(PGRef pg, SessionRef s, hobject_t o, ceph_tid_t t, uint32_t a)
    : RefCountedObject(g_ceph_context, 0),
      lock("Backoff::lock"),
      pg(pg),
      session(s),
      oid(o),
      first_tid(t),
      first_attempt(a) {}
};


struct Session : public RefCountedObject {
  EntityName entity_name;
  OSDCap caps;
  int64_t auid;
  ConnectionRef con;
  WatchConState wstate;

  Mutex session_dispatch_lock;
  list<OpRequestRef> waiting_on_map;

  OSDMapRef osdmap;  /// Map as of which waiting_for_pg is current
  map<spg_t, list<OpRequestRef> > waiting_for_pg;

  Spinlock sent_epoch_lock;
  epoch_t last_sent_epoch;
  Spinlock received_map_lock;
  epoch_t received_map_epoch; // largest epoch seen in MOSDMap from here

  /// protects backoffs; orders inside Backoff::lock *and* PG::backoff_lock
  Mutex backoff_lock;
  map<hobject_t,BackoffRef, hobject_t::BitwiseComparator> oid_backoffs;
  map<pg_t,BackoffRef> pg_backoffs;

  explicit Session(CephContext *cct) :
    RefCountedObject(cct),
    auid(-1), con(0),
    session_dispatch_lock("Session::session_dispatch_lock"),
    last_sent_epoch(0), received_map_epoch(0),
    backoff_lock("Session::backoff_lock")
    {}
  void maybe_reset_osdmap() {
    if (waiting_for_pg.empty()) {
      osdmap.reset();
    }
  }

  // called by PG::release_*_backoffs and PG::clear_backoffs()
  void rm_backoff(BackoffRef b) {
    Mutex::Locker l(backoff_lock);
    assert(b->lock.is_locked_by_me());
    assert(b->session == this);
    if (b->oid) {
      auto p = oid_backoffs.find(*b->oid);
      // may race with clear_backoffs()
      if (p != oid_backoffs.end()) {
	oid_backoffs.erase(p);
      }
    } else {
      auto p = pg_backoffs.find(*b->pgid);
      // may race with clear_backoffs()
      if (p != pg_backoffs.end()) {
	pg_backoffs.erase(p);
      }
    }
  }
  void clear_backoffs();
};

#endif
