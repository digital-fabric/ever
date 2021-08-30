#include "ever.h"
#include "../libev/ev.h"

VALUE cWatcher;

enum watcher_type {
  WATCHER_IO,
  WATCHER_TIMER
};


typedef struct Watcher_t {
  union {
    struct ev_io io;
    struct ev_timer timer;
  };

  Loop_t *loop;
  enum watcher_type type;
  VALUE key;
  int oneshot;
} Watcher_t;

////////////////////////////////////////////////////////////////////////////////

static void Watcher_mark(void *ptr) {
  Watcher_t *watcher = ptr;
  rb_gc_mark(watcher->key);
}

static void Watcher_free(void *ptr) {
  xfree(ptr);
}

static size_t Watcher_size(const void *ptr) {
  return sizeof(Watcher_t);
}

static const rb_data_type_t Watcher_type = {
    "Ever::Watcher",
    {Watcher_mark, Watcher_free, Watcher_size,},
    0, 0, RUBY_TYPED_FREE_IMMEDIATELY
};

static VALUE Watcher_allocate(VALUE klass) {
  Watcher_t *watcher = ALLOC(Watcher_t);
  return TypedData_Wrap_Struct(klass, &Watcher_type, watcher);
}

#define GetWatcher(obj, watcher) \
  TypedData_Get_Struct((obj), Watcher_t, &Watcher_type, (watcher))

VALUE Watcher_initialize(VALUE self) {
  return self;
}

////////////////////////////////////////////////////////////////////////////////

inline void watcher_stop(Watcher_t *watcher) {
  switch (watcher->type) {
    case WATCHER_IO:
      ev_io_stop(watcher->loop->ev_loop, &watcher->io);
      return;
    case WATCHER_TIMER:
      ev_timer_stop(watcher->loop->ev_loop, &watcher->timer);
      return;
  }
}

void watcher_io_callback(EV_P_ ev_io *w, int revents)
{
  Watcher_t *watcher = (Watcher_t *)w;
  loop_emit(watcher->loop, watcher->key);
  if (watcher->oneshot) {
    watcher_stop(watcher);
    loop_release_watcher(watcher->loop, watcher->key);
  }
}

void watcher_timer_callback(EV_P_ ev_timer *w, int revents)
{
  Watcher_t *watcher = (Watcher_t *)w;
  loop_emit(watcher->loop, watcher->key);
  if (watcher->oneshot) {
    watcher_stop(watcher);
    loop_release_watcher(watcher->loop, watcher->key);
  }
}

static inline void watcher_setup_io(Watcher_t *watcher, Loop_t *loop, VALUE key, int fd, int events, int oneshot) {
  watcher->type = WATCHER_IO;
  watcher->loop = loop;
  watcher->key = key;
  watcher->oneshot = oneshot;

  ev_io_init(&watcher->io, watcher_io_callback, fd, events);
  ev_io_start(loop->ev_loop, &watcher->io);
}

static inline void watcher_setup_timer(Watcher_t *watcher, Loop_t *loop, VALUE key, double timeout, double interval) {
  watcher->type = WATCHER_TIMER;
  watcher->loop = loop;
  watcher->key = key;
  watcher->oneshot = interval == 0.;

  ev_timer_init(&watcher->timer, watcher_timer_callback, timeout, interval);
  ev_timer_start(loop->ev_loop, &watcher->timer);
}

////////////////////////////////////////////////////////////////////////////////

inline void Watcher_setup_io(VALUE self, Loop_t *loop, VALUE key, int fd, int events, int oneshot) {
  Watcher_t *watcher;
  GetWatcher(self, watcher);

  watcher_setup_io(watcher, loop, key, fd, events, oneshot);
}

inline void Watcher_setup_timer(VALUE self, Loop_t *loop, VALUE key, double timeout, double interval) {
  Watcher_t *watcher;
  GetWatcher(self, watcher);

  watcher_setup_timer(watcher, loop, key, timeout, interval);
}

inline void Watcher_stop(VALUE self) {
  Watcher_t *watcher;
  GetWatcher(self, watcher);

  watcher_stop(watcher);
}

void Init_Watcher() {
  cWatcher = rb_define_class_under(mEver, "Watcher", rb_cObject);
  rb_define_alloc_func(cWatcher, Watcher_allocate);
  rb_define_method(cWatcher, "initialize", Watcher_initialize, 0);
}
