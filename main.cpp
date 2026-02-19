#include "ftr.h"
#include <stdexcept>

__attribute__((noinline))
int fib(int n) {
  FTR_FUNCTION();

  if (n <= 1) {
    return n;
  }
  return fib(n - 1) + fib(n - 2);
}



__attribute__((noinline))
void foo(int x) {
  FTR_FUNCTION();
  // usleep(1000);
  if (x == 10) {
    FTR_SCOPE("throwing");
    throw std::runtime_error("bad");
  }
}

int main() {
  {

    FTR_FUNCTION();
    volatile int sum = 0;
    for (int j = 0; j < 25; j++) {
      FTR_SCOPE("thread work");
      try {
        foo(j);
      } catch (const std::exception &e) {
        FTR_SCOPE("AYO");
        fprintf(stderr, "Caught exception: %s\n", e.what());
      }
      FTR_SCOPE("thread work");
      sum += fib(j % 25);
    }

  }
  // ftr_close();
  return 0;
}