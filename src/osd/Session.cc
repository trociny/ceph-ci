// -*- mode:C++; tab-width:8; c-basic-offset:2; indent-tabs-mode:t -*-
// vim: ts=8 sw=2 smarttab

#include "PG.h"
#include "Session.h"

void Session::clear_backoffs()
{
  map<hobject_t,BackoffRef,hobject_t::BitwiseComparator> oid;
  map<pg_t,BackoffRef> pg;
  {
    Mutex::Locker l(backoff_lock);
    oid.swap(oid_backoffs);
    pg.swap(pg_backoffs);
  }
  for (auto& p : oid) {
    Mutex::Locker l(p.second->lock);
    assert(p.second->session == this);
    p.second->session.reset();
    if (p.second->pg) {
      p.second->pg->rm_backoff(p.second);
    }
  }
  for (auto& p : pg) {
    Mutex::Locker l(p.second->lock);
    assert(p.second->session == this);
    p.second->session.reset();
    if (p.second->pg) {
      p.second->pg->rm_backoff(p.second);
    }
  }
}
