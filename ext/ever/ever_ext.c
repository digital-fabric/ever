#include "ever.h"

void Init_Loop();
void Init_Watcher();

VALUE mEver;

void Init_ever_ext() {
  mEver = rb_define_module("Ever");
  Init_Loop();
  Init_Watcher();
}
