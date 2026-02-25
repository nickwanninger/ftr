#include "ftr.h"
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>

__attribute__((noinline)) int fib(int n) {
  // FTR_FUNCTION();

  if (n <= 1) {
    return n;
  }
  return fib(n - 1) + fib(n - 2);
}

// A simple work queue to demonstrate flow events across threads.
// The work_item pointer itself is used as the flow correlation ID —
// no need for ftr_new_flow_id().
struct work_item {
  int value;
};

static work_item *queue[256];
static volatile int queue_head = 0;
static volatile int queue_tail = 0;
static volatile int done = 0;
static pthread_mutex_t queue_mtx = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t queue_cond = PTHREAD_COND_INITIALIZER;

static void enqueue(work_item *item) {
  pthread_mutex_lock(&queue_mtx);
  queue[queue_tail++ % 256] = item;
  pthread_cond_signal(&queue_cond);
  pthread_mutex_unlock(&queue_mtx);
}

static work_item *dequeue() {
  pthread_mutex_lock(&queue_mtx);
  while (queue_head == queue_tail && !done)
    pthread_cond_wait(&queue_cond, &queue_mtx);
  work_item *item = nullptr;
  if (queue_head != queue_tail)
    item = queue[queue_head++ % 256];
  pthread_mutex_unlock(&queue_mtx);
  return item;
}

static void *consumer_thread(void *) {
  work_item *item;
  while ((item = dequeue()) != nullptr) {
    FTR_SCOPE_FLOW_END("work", item); // creates its own scope
    fib(item->value);
    ftr_logf("processed fib(%d)", item->value);
    free(item);
  }
  return nullptr;
}

int main() {
  FTR_FUNCTION();

  // Start consumer threads
  pthread_t workers[2];
  for (int i = 0; i < 2; i++)
    pthread_create(&workers[i], nullptr, consumer_thread, nullptr);

  // Producer: use the work_item pointer as the flow ID
  for (int j = 0; j < 25; j++) {
    work_item *item = (work_item *)malloc(sizeof(work_item));
    item->value = 12 + (j % 5);

    FTR_SCOPE_FLOW_BEGIN("work", item); // creates its own scope
    enqueue(item);

    FTR_SCOPE_FLOW_STEP("work submitted", item);
    ftr_logf("enqueued job %d", j);
  }

  // Signal consumers to finish
  pthread_mutex_lock(&queue_mtx);
  done = 1;
  pthread_cond_broadcast(&queue_cond);
  pthread_mutex_unlock(&queue_mtx);

  for (int i = 0; i < 2; i++)
    pthread_join(workers[i], nullptr);

  return 0;
}
