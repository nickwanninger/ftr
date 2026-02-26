#include <condition_variable>
#include <deque>
#include <ftr.h>
#include <mutex>
#include <thread>
#include <vector>

__attribute__((noinline)) int fib(int n) {
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

static std::deque<work_item *> queue;
static std::mutex queue_mtx;
static std::condition_variable queue_cv;
static bool done = false;

static void enqueue(work_item *item) {
  std::lock_guard<std::mutex> lk(queue_mtx);
  queue.push_back(item);
  queue_cv.notify_one();
}

static work_item *dequeue() {
  std::unique_lock<std::mutex> lk(queue_mtx);
  queue_cv.wait(lk, [] { return !queue.empty() || done; });
  if (queue.empty())
    return nullptr;
  work_item *item = queue.front();
  queue.pop_front();
  return item;
}

static void consumer_thread() {
  work_item *item;
  while ((item = dequeue()) != nullptr) {

    if (item->value > 0) {
      {
        FTR_SCOPE_FLOW_STEP("work", item);
        usleep(10 + item->value);
      }
      item->value--;
      enqueue(item);
    } else {
      {
        FTR_SCOPE_FLOW_END("work", item);
        usleep(10 + item->value);
      }
      delete item;
    }
  }
}

int main() {
  int ncpus = std::thread::hardware_concurrency();
  if (ncpus < 1)
    ncpus = 1;

  // Start one consumer thread per CPU
  std::vector<std::thread> workers;
  for (int i = 0; i < ncpus; i++)
    workers.emplace_back(consumer_thread);

  // Producer: use the work_item pointer as the flow ID
  for (int j = 0; j < 1000; j++) {
    auto *item = new work_item{100};

    FTR_SCOPE_FLOW_BEGIN("enqueue", item);

    enqueue(item);
  }

  // Signal consumers to finish
  {
    std::lock_guard<std::mutex> lk(queue_mtx);
    done = true;
  }
  queue_cv.notify_all();

  for (auto &w : workers)
    w.join();

  return 0;
}
