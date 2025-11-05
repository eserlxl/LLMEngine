# Threading and Concurrency Rules

## Overview

This document defines threading and concurrency patterns for LLMEngine. These rules ensure thread-safe code, prevent data races, and promote structured concurrency.

## Thread-Safe Patterns

### Immutability by Default

- **Prefer immutable data**: Const objects are inherently thread-safe
- **Immutable after construction**: Once created, data doesn't change
- **Shared read-only access**: Multiple threads can safely read immutable data

```cpp
class Config {
public:
  Config(std::string endpoint, int timeout)
    : endpoint_(std::move(endpoint)), timeout_(timeout) {}
  
  // No mutators after construction
  const std::string& endpoint() const { return endpoint_; }
  int timeout() const { return timeout_; }
  
private:
  const std::string endpoint_;
  const int timeout_;
};

// Thread-safe: multiple threads can read concurrently
void process(const Config& config) {
  // Read-only access, no synchronization needed
}
```

### Thread-Local Storage

- Use `thread_local` for per-thread state
- Avoid global mutable state
- Useful for caches, buffers, or context

```cpp
thread_local std::vector<int> local_buffer;

void process() {
  local_buffer.clear(); // Each thread has its own buffer
  // ...
}
```

### Synchronization Primitives

#### std::mutex

- Use for mutual exclusion
- Prefer `std::lock_guard` or `std::unique_lock` for RAII
- Keep critical sections small

```cpp
class ThreadSafeCounter {
public:
  void increment() {
    std::lock_guard<std::mutex> lock(mutex_);
    ++count_;
  }
  
  int get() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return count_;
  }
  
private:
  mutable std::mutex mutex_;
  int count_ = 0;
};
```

#### std::shared_mutex (C++17)

- Use for read-write locks
- Multiple readers, single writer
- Prefer `std::shared_lock` for readers

```cpp
class ThreadSafeCache {
public:
  std::optional<Value> get(const Key& key) const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    auto it = cache_.find(key);
    return it != cache_.end() ? it->second : std::nullopt;
  }
  
  void put(const Key& key, const Value& value) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    cache_[key] = value;
  }
  
private:
  mutable std::shared_mutex mutex_;
  std::unordered_map<Key, Value> cache_;
};
```

## std::jthread and stop_token (C++20)

### Cancellable Tasks

- Use `std::jthread` for cancellable threads
- Use `std::stop_token` for cooperative cancellation
- Prefer over `std::thread` when cancellation is needed

```cpp
void worker(std::stop_token token) {
  while (!token.stop_requested()) {
    // Do work
    process_item();
    
    // Check cancellation periodically
    if (token.stop_requested()) {
      break;
    }
  }
}

// Usage
std::jthread t(worker);
// ... later ...
t.request_stop(); // Cooperative cancellation
t.join();
```

### Stop Callbacks

- Register callbacks for cleanup on stop request
- Use `std::stop_callback` for automatic cleanup

```cpp
void worker(std::stop_token token) {
  Resource resource;
  
  // Cleanup when stop is requested
  std::stop_callback callback(token, [&resource]() {
    resource.cleanup();
  });
  
  while (!token.stop_requested()) {
    // ...
  }
}
```

## Atomics vs Mutex Guidance

### When to Use Atomics

- **Simple operations**: Load, store, compare-and-swap
- **Lock-free data structures**: When performance is critical
- **Flags and counters**: Simple shared state

```cpp
class AtomicCounter {
public:
  void increment() {
    count_.fetch_add(1, std::memory_order_relaxed);
  }
  
  int get() const {
    return count_.load(std::memory_order_acquire);
  }
  
private:
  std::atomic<int> count_{0};
};
```

### When to Use Mutex

- **Complex operations**: Multiple related operations
- **Read-write locks**: When readers outnumber writers
- **Condition variables**: Waiting for conditions

```cpp
class ThreadSafeQueue {
public:
  void push(const Item& item) {
    std::lock_guard<std::mutex> lock(mutex_);
    queue_.push(item);
    condition_.notify_one();
  }
  
  Item pop() {
    std::unique_lock<std::mutex> lock(mutex_);
    condition_.wait(lock, [this] { return !queue_.empty(); });
    Item item = queue_.front();
    queue_.pop();
    return item;
  }
  
private:
  std::mutex mutex_;
  std::condition_variable condition_;
  std::queue<Item> queue_;
};
```

### Memory Ordering

- **Default**: Use `std::memory_order_seq_cst` (strongest, safest)
- **Relaxed**: Use `std::memory_order_relaxed` for simple counters/flags
- **Acquire-Release**: Use for synchronization without full sequential consistency
- **Avoid**: `std::memory_order_consume` (deprecated in C++17)

```cpp
// Relaxed: Simple counter, no ordering requirements
counter_.fetch_add(1, std::memory_order_relaxed);

// Acquire-Release: Synchronization point
ready_.store(true, std::memory_order_release);
if (ready_.load(std::memory_order_acquire)) {
  // Synchronized access
}
```

## No Data Races

### Data Race Definition

A data race occurs when:
- Two or more threads access the same memory location
- At least one access is a write
- Accesses are not synchronized

### Preventing Data Races

1. **Synchronization**: Use mutexes, atomics, or other synchronization primitives
2. **Immutability**: Make data const/immutable
3. **Thread-local**: Use `thread_local` for per-thread state
4. **Ownership**: Ensure only one thread owns mutable data

```cpp
// Bad: Data race
int counter = 0;
void increment() {
  ++counter; // Race condition!
}

// Good: Synchronized
std::atomic<int> counter{0};
void increment() {
  counter.fetch_add(1);
}

// Good: Thread-local
thread_local int local_counter = 0;
void increment() {
  ++local_counter; // No race, each thread has own copy
}
```

### Thread Sanitizer (TSan)

- Use ThreadSanitizer to detect data races
- Enable in Debug builds: `-fsanitize=thread`
- Run tests with TSan to catch race conditions

## Structured Concurrency

### RAII for Threads

- Use `std::jthread` (automatically joins on destruction)
- Or wrap `std::thread` in RAII class

```cpp
// Good: Automatic cleanup
std::jthread worker([]() {
  // Work
});

// Worker automatically joined on destruction

// Bad: Manual management
std::thread t([]() {
  // Work
});
// Must remember to join() or detach()
```

### Thread Pools

- Use thread pools for bounded parallelism
- Prefer `std::execution` (C++17) or third-party libraries
- Avoid creating unlimited threads

```cpp
// Example: Use std::execution::par for parallel algorithms
std::vector<int> data = /* ... */;
std::sort(std::execution::par, data.begin(), data.end());
```

### Futures and Promises

- Use `std::future` and `std::promise` for async results
- Use `std::async` for simple async tasks
- Prefer `std::future` over raw threads for result communication

```cpp
auto future = std::async(std::launch::async, []() {
  return compute_result();
});

// Do other work...

auto result = future.get(); // Wait for result
```

## Avoid Global Mutable State

### Problems with Global State

- Hard to test (global state persists between tests)
- Hard to reason about (unclear ownership)
- Thread-safety issues (requires synchronization)
- Hidden dependencies

### Alternatives

- **Dependency injection**: Pass dependencies as parameters
- **Context objects**: Pass context through call chain
- **Thread-local storage**: Use `thread_local` if needed
- **Singleton pattern**: Avoid; use dependency injection instead

```cpp
// Bad: Global state
Logger* g_logger = nullptr;

// Good: Dependency injection
class Client {
public:
  explicit Client(std::shared_ptr<Logger> logger)
    : logger_(std::move(logger)) {}
  
private:
  std::shared_ptr<Logger> logger_;
};
```

## Summary

1. **Immutability**: Prefer const/immutable data
2. **std::jthread**: Use for cancellable threads
3. **Atomics**: Simple operations, lock-free structures
4. **Mutexes**: Complex operations, read-write locks
5. **No data races**: Always synchronize shared mutable state
6. **Structured concurrency**: RAII for threads, thread pools
7. **Avoid global state**: Use dependency injection

## References

- See `20-error-handling.md` for error handling in concurrent code
- See `30-performance.md` for performance considerations
- [C++ Core Guidelines: Concurrency](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#S-concurrency)

