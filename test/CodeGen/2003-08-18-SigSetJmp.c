// RUN: %clang_cc1 -emit-llvm %s  -o /dev/null


#include <setjmp.h>

sigjmp_buf B;
int foo() {
  sigsetjmp(B, 1);
  bar();
}
