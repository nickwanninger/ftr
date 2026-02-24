#include "ftr.h"

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

  for (int j = 0; j < 25; j++) {
    FTR_SCOPE("thread work");

    ftr_logf("message");

    int res = fib(12);

    ftr_logf("fib(%d) = %d", j, res);
    ftr_logf("Looping around");
    ftr_logf("still going...");
  }
  return 0;
}
