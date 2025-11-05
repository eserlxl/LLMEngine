# Performance Guidelines

## Overview

This document defines performance guidelines for LLMEngine. These rules ensure efficient code while maintaining readability and maintainability. Remember: **premature optimization is the root of all evil**—profile first, optimize second.

## Zero-Cost Abstractions

### Compile-Time Optimizations

- **Templates**: Zero runtime overhead when properly used
- **constexpr**: Evaluate at compile time when possible
- **Inline functions**: Let compiler decide; use `inline` for header-only functions

```cpp
constexpr int compute_size(int count) {
  return count * sizeof(int);
}

template<typename T>
void process(const T& value) {
  // Zero overhead if T is known at compile time
}
```

### Standard Library Abstractions

- Standard library algorithms and containers are optimized
- Prefer `std::algorithm` over manual loops (compiler can optimize better)
- Use `std::vector` by default; switch only when profiling shows benefit

```cpp
// Good: Compiler can optimize
std::sort(vec.begin(), vec.end());
auto it = std::find_if(vec.begin(), vec.end(), predicate);

// Bad: Manual loop may be less optimal
for (size_t i = 0; i < vec.size() - 1; ++i) {
  for (size_t j = i + 1; j < vec.size(); ++j) {
    // Manual sort
  }
}
```

## Move Semantics

### Prefer Moves Over Copies

- Use `std::move` for expensive-to-copy types
- Return by value (compiler optimizes with RVO/move)
- Accept parameters by value or const reference (move when appropriate)

**Good**:
```cpp
class Container {
public:
  void add(std::string value) {
    data_.push_back(std::move(value)); // Move, not copy
  }
  
  std::string get_name() && { // R-value qualifier
    return std::move(name_); // Move out
  }
};

// Return by value (RVO/move)
std::vector<int> generate_data() {
  std::vector<int> result;
  // ... fill ...
  return result; // Moved or RVO
}
```

**Bad**:
```cpp
void process(const std::string& data) {
  std::string copy = data; // Unnecessary copy
  // ...
}
```

### Avoid Unnecessary Copies

- Use `const&` for read-only parameters
- Use `std::string_view` for string parameters (C++17)
- Use `std::span` for array/vector parameters (C++20)

```cpp
void process(const std::string& data); // Read-only
void process(std::string_view data);   // Better: no ownership
void process(std::span<const int> data); // Better: no ownership
```

## Container Optimization

### Reserve Capacity

- Use `reserve()` when size is known in advance
- Reduces reallocations and copies

```cpp
std::vector<int> vec;
vec.reserve(1000); // Pre-allocate
for (int i = 0; i < 1000; ++i) {
  vec.push_back(i); // No reallocations
}
```

### Small-Buffer Optimization (SBO)

- `std::string` and some containers use SBO
- Small objects stored inline, avoiding heap allocation
- Consider for custom string-like types

### Container Choice

- **`std::vector`**: Default choice, cache-friendly
- **`std::deque`**: When frequent insertions at both ends
- **`std::list`**: Rarely needed; use when pointer stability required
- **`std::unordered_map`**: O(1) lookup, use when order doesn't matter
- **`std::map`**: O(log n) lookup, use when order matters

Choose based on access patterns, not premature optimization.

## Cache Friendliness

### Data Locality

- Keep related data together
- Prefer arrays/vectors over linked structures
- Access data sequentially when possible

```cpp
// Good: Cache-friendly
struct Point {
  float x, y, z; // Contiguous in memory
};
std::vector<Point> points;

// Bad: Cache-unfriendly (pointer chasing)
std::list<Point> points;
```

### Structure Layout

- Group frequently accessed fields together
- Consider padding and alignment
- Use `alignas` when needed for SIMD

```cpp
struct HotData {
  int frequently_used;
  float also_hot;
  // ...
};

struct ColdData {
  std::string rarely_used;
  // ...
};
```

## Avoid Allocations in Hot Paths

### Stack vs Heap

- Prefer stack allocation for small, short-lived objects
- Use heap (smart pointers) only when necessary
- Consider object pools for frequently allocated types

```cpp
// Good: Stack allocation
void process() {
  std::array<int, 100> buffer; // Stack
  // ...
}

// Bad: Unnecessary heap allocation
void process() {
  auto buffer = std::make_unique<int[]>(100); // Heap
  // ...
}
```

### String Operations

- Avoid string concatenation in loops
- Use `std::string::reserve()` or `std::ostringstream`
- Consider `std::string_view` to avoid allocations

```cpp
// Bad: Multiple allocations
std::string result;
for (const auto& item : items) {
  result += item + ", "; // Allocations in loop
}

// Good: Pre-allocate
std::string result;
result.reserve(estimated_size);
for (const auto& item : items) {
  result += item;
  result += ", ";
}
```

## Profile-Before-Optimize Principle

### Measurement First

1. **Profile**: Use profilers (perf, Valgrind, Instruments)
2. **Identify bottlenecks**: Focus on hot paths
3. **Optimize**: Make targeted improvements
4. **Verify**: Measure again to confirm improvement

### Benchmarking

- Use benchmarks for critical paths
- Compare before/after optimization
- Use tools like Google Benchmark

```cpp
// Example benchmark structure
void benchmark_function() {
  auto start = std::chrono::high_resolution_clock::now();
  // ... code to benchmark ...
  auto end = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
  // Report duration
}
```

### Optimization Targets

Focus optimization on:

1. **Hot paths**: Frequently executed code
2. **Bottlenecks**: Code consuming significant time
3. **Critical sections**: Code affecting user experience

**Avoid** optimizing:

- Cold paths (rarely executed)
- Code that's already fast enough
- Code without profiling data

## Common Performance Pitfalls

### Unnecessary Copies

```cpp
// Bad
void process(std::string data) { // Copy
  // ...
}

// Good
void process(const std::string& data) { // Reference
  // ...
}
```

### Virtual Function Calls

- Avoid virtual calls in tight loops when possible
- Use templates or CRTP for compile-time polymorphism

### Branch Prediction

- Prefer predictable branches (e.g., sorted data)
- Use `[[likely]]`/`[[unlikely]]` hints (C++20) when appropriate

### I/O Operations

- Batch I/O operations
- Use buffered I/O
- Consider async I/O for non-blocking operations

## Summary

1. **Zero-cost abstractions**: Use templates, constexpr, standard library
2. **Move semantics**: Prefer moves over copies
3. **Container optimization**: Reserve capacity, choose appropriate container
4. **Cache friendliness**: Keep data local, prefer arrays
5. **Avoid allocations**: Stack allocation, pre-allocate, reuse
6. **Profile first**: Measure, identify, optimize, verify

## References

- See `40-threading.md` for concurrency performance
- See `60-tests.md` for performance testing
- [C++ Core Guidelines: Performance](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#S-performance)

