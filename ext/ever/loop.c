#include "ever.h"
#include "ruby/io.h"

VALUE cLoop;
VALUE SYM_stop;
ID ID_ivar_io;

////////////////////////////////////////////////////////////////////////////////

static inline void loop_run_ev_loop(Loop_t *loop) {
  loop->in_ev_loop = 1;
  ev_run(loop->ev_loop, EVRUN_ONCE);
  loop->in_ev_loop = 0;
}

static inline void loop_yield_queued_events(Loop_t *loop) {
  int len = RARRAY_LEN(loop->queued_events);
  if (!len) return;

  VALUE events = loop->queued_events;
  loop->queued_events = rb_ary_new();
  for (int i = 0; i < len; i++) {
    rb_yield(RARRAY_AREF(events, i));
  }
  RB_GC_GUARD(events);
}

inline void loop_emit(Loop_t *loop, VALUE key) {
  rb_ary_push(loop->queued_events, key);
}

static inline void loop_signal(Loop_t *loop) {
  if (loop->in_ev_loop) {
    // Since the loop will run until at least one event has occurred, we signal
    // the selector's associated async watcher, which will cause the ev loop to
    // return. In contrast to using `ev_break` to break out of the loop, which
    // should be called from the same thread (from within the ev_loop), using an
    // `ev_async` allows us to interrupt the event loop across threads.
    ev_async_send(loop->ev_loop, &loop->break_async);
  }
}

static inline VALUE loop_get_free_watcher(Loop_t *loop) {
  VALUE watcher = rb_ary_pop(loop->free_watchers);
  if (watcher != Qnil) return watcher;

  return rb_class_new_instance(0, 0, cWatcher);
}

static inline void loop_watch_fd(Loop_t *loop, VALUE key, int fd, int events, int oneshot) {
  VALUE watcher = loop_get_free_watcher(loop);
  Watcher_setup_io(watcher, loop, key, fd, events, oneshot);
  rb_hash_aset(loop->active_watchers, key, watcher);
}

static inline void loop_watch_timer(Loop_t *loop, VALUE key, double timeout, double interval) {
  VALUE watcher = loop_get_free_watcher(loop);
  Watcher_setup_timer(watcher, loop, key, timeout, interval);
  rb_hash_aset(loop->active_watchers, key, watcher);
}

static inline int sym_to_events(VALUE rw) {
  return RTEST(rw) ? EV_WRITE : EV_READ;
}

static inline int fd_from_io(VALUE io) {
  rb_io_t *fptr;
  GetOpenFile(rb_convert_type(io, T_FILE, "IO", "to_io"), fptr);
  return fptr->fd;
}

inline void loop_release_watcher(Loop_t *loop, VALUE key) {
  VALUE watcher = rb_hash_delete(loop->active_watchers, key);
  if (watcher != Qnil)
    rb_ary_push(loop->free_watchers, watcher);
}

////////////////////////////////////////////////////////////////////////////////

static void Loop_mark(void *ptr) {
  Loop_t *loop = ptr;
  rb_gc_mark(loop->active_watchers);
  rb_gc_mark(loop->free_watchers);
  rb_gc_mark(loop->queued_events);
}

static void Loop_free(void *ptr) {
  Loop_t *loop = ptr;

  ev_async_stop(loop->ev_loop, &loop->break_async);
  ev_loop_destroy(loop->ev_loop);
}

static size_t Loop_size(const void *ptr) {
  return sizeof(Loop_t);
}

static const rb_data_type_t Loop_type = {
    "Ever::Loop",
    {Loop_mark, Loop_free, Loop_size,},
    0, 0, RUBY_TYPED_FREE_IMMEDIATELY
};

static VALUE Loop_allocate(VALUE klass) {
  Loop_t *loop = ALLOC(Loop_t);
  return TypedData_Wrap_Struct(klass, &Loop_type, loop);
}

#define GetLoop(obj, loop) \
  TypedData_Get_Struct((obj), Loop_t, &Loop_type, (loop))

void break_async_callback(struct ev_loop *ev_loop, struct ev_async *ev_async, int revents) {
  // This callback does nothing, the break async is used solely for breaking out
  // of a *blocking* event loop (waking it up) in a thread-safe, signal-safe manner
}

static VALUE Loop_initialize(VALUE self) {
  Loop_t *loop;
  GetLoop(self, loop);

  loop->ev_loop = ev_loop_new(EVFLAG_NOSIGMASK);
  // start async watcher used for breaking a poll op (from another thread)
  ev_async_init(&loop->break_async, break_async_callback);
  ev_async_start(loop->ev_loop, &loop->break_async);
  // the break_async watcher is unreferenced, in order for Loop_poll to not
  // block when no other watcher is active
  ev_unref(loop->ev_loop);

  loop->active_watchers = rb_hash_new();
  loop->free_watchers = rb_ary_new();
  loop->queued_events = rb_ary_new();

  loop->stop = 0;
  loop->in_ev_loop = 0;

  return Qnil;
}

VALUE Loop_each(VALUE self) {
  Loop_t *loop;
  GetLoop(self, loop);

  loop->stop = 0;
  while (!loop->stop) {
    if (RARRAY_LEN(loop->queued_events) == 0) loop_run_ev_loop(loop);
    loop_yield_queued_events(loop);
  }
  return self;
}

VALUE Loop_next_event(VALUE self) {
  Loop_t *loop;
  GetLoop(self, loop);

  if (RARRAY_LEN(loop->queued_events) == 0) loop_run_ev_loop(loop);
  return rb_ary_shift(loop->queued_events);
}

VALUE Loop_emit(VALUE self, VALUE key) {
  Loop_t *loop;
  GetLoop(self, loop);

  if (key == SYM_stop)
    loop->stop = 1;
  else
    rb_ary_push(loop->queued_events, key);

  loop_signal(loop);
  return key;
}

VALUE Loop_signal(VALUE self) {
  Loop_t *loop;
  GetLoop(self, loop);

  loop_signal(loop);
  return self;
}

VALUE Loop_stop(VALUE self) {
  Loop_t *loop;
  GetLoop(self, loop);

  loop->stop = 1;
  loop_signal(loop);
  return self;
}

VALUE Loop_watch_fd(VALUE self, VALUE key, VALUE fd, VALUE rw, VALUE oneshot) {
  Loop_t *loop;
  GetLoop(self, loop);

  if (rb_hash_aref(loop->active_watchers, key) != Qnil)
    rb_raise(rb_eRuntimeError, "Duplicate event key detected, event key must be unique");

  loop_watch_fd(loop, key, FIX2INT(fd), sym_to_events(rw), RTEST(oneshot));
  return self;
}

VALUE Loop_watch_io(VALUE self, VALUE key, VALUE io, VALUE rw, VALUE oneshot) {
  Loop_t *loop;
  GetLoop(self, loop);

  if (rb_hash_aref(loop->active_watchers, key) != Qnil)
    rb_raise(rb_eRuntimeError, "Duplicate event key detected, event key must be unique");

  loop_watch_fd(loop, key, fd_from_io(io), sym_to_events(rw), RTEST(oneshot));
  return self;
}

VALUE Loop_watch_timer(VALUE self, VALUE key, VALUE timeout, VALUE interval) {
  Loop_t *loop;
  GetLoop(self, loop);

  if (rb_hash_aref(loop->active_watchers, key) != Qnil)
    rb_raise(rb_eRuntimeError, "Duplicate event key detected, event key must be unique");

  loop_watch_timer(loop, key, NUM2DBL(timeout), NUM2DBL(interval));
  return self;
}

VALUE Loop_unwatch(VALUE self, VALUE key) {
  Loop_t *loop;
  GetLoop(self, loop);

  VALUE watcher = rb_hash_delete(loop->active_watchers, key);
  if (watcher == Qnil) return self;

  Watcher_stop(watcher);
  rb_ary_push(loop->free_watchers, watcher);

  return self;
}

void Init_Loop() {
  ev_set_allocator(xrealloc);

  cLoop = rb_define_class_under(mEver, "Loop", rb_cObject);
  rb_define_alloc_func(cLoop, Loop_allocate);

  rb_define_method(cLoop, "initialize", Loop_initialize, 0);

  rb_define_method(cLoop, "each", Loop_each, 0);
  rb_define_method(cLoop, "next_event", Loop_next_event, 0);
  rb_define_method(cLoop, "emit", Loop_emit, 1);
  rb_define_method(cLoop, "signal", Loop_signal, 0);
  rb_define_method(cLoop, "stop", Loop_stop, 0);
  rb_define_method(cLoop, "watch_fd", Loop_watch_fd, 4);
  rb_define_method(cLoop, "watch_io", Loop_watch_io, 4);
  rb_define_method(cLoop, "watch_timer", Loop_watch_timer, 3);
  rb_define_method(cLoop, "unwatch", Loop_unwatch, 1);

  SYM_stop = ID2SYM(rb_intern("stop"));
  ID_ivar_io = rb_intern("@io");
}
