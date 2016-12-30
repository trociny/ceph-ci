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
    Backoff *b = p.second.get();
    Mutex::Locker l(b->lock);
    if (b->pg) {
      assert(b->session == this);
      b->pg->rm_backoff(b);
      b->pg.reset();
      b->session.reset();
    }
  }
  for (auto& p : pg) {
    Backoff *b = p.second.get();
    Mutex::Locker l(b->lock);
    if (b->pg) {
      assert(b->session == this);
      b->pg->rm_backoff(b);
      b->pg.reset();
      b->session.reset();
    }
  }
}
