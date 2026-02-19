#include "ftr.h"

int fib(int n) {
  FTR_SCOPE("fib");
  if (n <= 1) {
    return n;
  }
  return fib(n - 1) + fib(n - 2);
}

int main() {
  ftr_open("trace.fxt");

  // for (int i = 0; i < 1000; i++) {
  //   ftr_write_span(0, 1, "span one", 100, 300);
  // }

  {
    FTR_SCOPE("main");
    fib(16);
  }


  ftr_close();
  return 0;
}