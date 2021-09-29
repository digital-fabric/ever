#ifndef EVER_H
#define EVER_H

#include <execinfo.h>
#include "ruby.h"
#include "../libev/ev.h"

// debugging
#define OBJ_ID(obj) (NUM2LONG(rb_funcall(obj, rb_intern("object_id"), 0)))
#define INSPECT(str, obj) { printf(str); VALUE s = rb_funcall(obj, rb_intern("inspect"), 0); printf(": %s\n", StringValueCStr(s)); }
#define TRACE_CALLER() { VALUE c = rb_funcall(rb_mKernel, rb_intern("caller"), 0); INSPECT("caller: ", c); }
#define TRACE_C_STACK() { \
  void *entries[10]; \
  size_t size = backtrace(entries, 10); \
  char **strings = backtrace_symbols(entries, size); \
  for (unsigned long i = 0; i < size; i++) printf("%s\n", strings[i]); \
  free(strings); \
}

// exceptions
#define TEST_EXCEPTION(ret) (rb_obj_is_kind_of(ret, rb_eException) == Qtrue)
#define RAISE_EXCEPTION(e) rb_funcall(e, ID_invoke, 0);
#define RAISE_IF_EXCEPTION(ret) if (rb_obj_is_kind_of(ret, rb_eException) == Qtrue) { RAISE_EXCEPTION(ret); }
#define RAISE_IF_NOT_NIL(ret) if (ret != Qnil) { RAISE_EXCEPTION(ret); }

extern VALUE mEver;
extern VALUE cLoop;
extern VALUE cWatcher;

typedef struct Loop_t {
  struct ev_loop *ev_loop;
  struct ev_async break_async;

  VALUE active_watchers;
  VALUE free_watchers;
  VALUE queued_events;

  int stop;
  int in_ev_loop;
} Loop_t;

void loop_emit(Loop_t *loop, VALUE key);
void loop_release_watcher(Loop_t *loop, VALUE key);

void Watcher_setup_io(VALUE watcher, Loop_t *loop, VALUE key, int fd, int events, int oneshot);
void Watcher_setup_timer(VALUE watcher, Loop_t *loop, VALUE key, double timeout, double interval);
void Watcher_stop(VALUE watcher);

#endif /* EVER_H */