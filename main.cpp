#include "ftr.h"
#include <stdexcept>

__attribute__((noinline)) int fib(int n) {
  FTR_FUNCTION();

  if (n <= 1) {
    return n;
  }
  return fib(n - 1) + fib(n - 2);
}


int main() {
  FTR_FUNCTION();
  volatile int sum = 0;

  for (int i = 0; i < 4; i++) {
    for (int j = 0; j < 25; j++) {
      FTR_SCOPE("thread work");
      // ftr_begin("main", "A");
      // nanosleep((const struct timespec[]){{0, 10000000L}}, NULL);
      // ftr_end("main", "A");


      // ftr_begin("main", "B");
      // nanosleep((const struct timespec[]){{0, 10000000L}}, NULL);
      // ftr_end("main", "B");

      fib(12);
      // ftr_log("message");
      // try {
      //   foo(j);
      // } catch (const std::exception &e) {
      //   FTR_MARK("caught exception");
      //   FTR_MARK("caught exception");
      //   FTR_MARK("caught exception");
      //   FTR_MARK("caught exception");
      //   FTR_MARK("caught exception");
      //   FTR_MARK("caught exception");
      //   FTR_MARK("caught exception");
      //   // fprintf(stderr, "Caught exception: %s\n", e.what());
      // }
      // FTR_SCOPE("thread work");
      // sum += fib(j % 25);
    }
  }
  // usleep(10000);
  // ftr_close();
  return 0;
}
