#include "ftr.h"
#include <thread>
#include <vector>


__attribute__((noinline))
int fib(int n) {
  FTR_FUNCTION();

  if (n <= 1) {
    return n;
  }
  return fib(n - 1) + fib(n - 2);
}

int main() {

  ftr_open("trace.fxt");
  {

    FTR_FUNCTION();
    volatile int sum = 0;
    for (int j = 0; j < 25; j++) {
      FTR_SCOPE("thread work");
      sum += fib(j % 25);
    }

  }
  // ftr_close();
  return 0;
}