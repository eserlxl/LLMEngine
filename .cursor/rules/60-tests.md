# Testing Rules

## Overview

This document defines testing guidelines for LLMEngine. These rules ensure comprehensive test coverage, deterministic tests, and maintainable test code.

## Test Framework

### Recommended: GoogleTest

- **Preferred framework**: GoogleTest for new tests
- **Why**: Industry standard, rich features, good CMake integration
- **Alternative**: Catch2 if GoogleTest unavailable

### Using GoogleTest

```cpp
#include <gtest/gtest.h>

TEST(ClientTest, Initialization) {
  Client client;
  EXPECT_TRUE(client.is_initialized());
}

TEST(ClientTest, RequestProcessing) {
  Client client;
  auto result = client.process("test");
  EXPECT_EQ(result.status(), Status::Ok);
}
```

### Test Fixtures

Use test fixtures for shared setup:

```cpp
class ClientTest : public ::testing::Test {
protected:
  void SetUp() override {
    client_ = std::make_unique<Client>(config_);
  }
  
  void TearDown() override {
    client_.reset();
  }
  
  Config config_;
  std::unique_ptr<Client> client_;
};

TEST_F(ClientTest, ProcessRequest) {
  auto result = client_->process("test");
  EXPECT_TRUE(result.ok());
}
```

### Parameterized Tests

Use parameterized tests for multiple inputs:

```cpp
class ClientParamTest : public ::testing::TestWithParam<std::string> {};

TEST_P(ClientParamTest, ValidEndpoints) {
  Client client(GetParam());
  EXPECT_TRUE(client.is_valid());
}

INSTANTIATE_TEST_SUITE_P(
  ValidEndpoints,
  ClientParamTest,
  ::testing::Values("https://api.example.com", "https://api2.example.com")
);
```

## Test Layout

### Directory Structure

- **Test directory**: `test/` at project root
- **Mirror source structure**: `test/src/ClientTest.cpp` for `src/Client.cpp`
- **Test support**: `test/support/` for mocks and test utilities
- **Integration tests**: `test/integration/` or `test_*_integration.cpp`

```
test/
  test_client.cpp           # Unit tests for Client
  test_parser.cpp           # Unit tests for Parser
  test_integration.cpp      # Integration tests
  support/
    MockAPIClient.hpp       # Mock implementations
    TestUtils.hpp           # Test utilities
```

### Test Naming

- **Unit tests**: `test_*.cpp` or `*_test.cpp`
- **Integration tests**: `test_*_integration.cpp` or `integration/*_test.cpp`
- **Test names**: Descriptive, indicate what is being tested

```cpp
TEST(ClientTest, HandlesInvalidRequest) { }
TEST(ClientTest, RetriesOnTimeout) { }
TEST(ClientIntegrationTest, EndToEndWorkflow) { }
```

## Coverage Target

### CMake Configuration

- Enable coverage via `ENABLE_COVERAGE` option
- Coverage in Debug builds only
- Generate coverage reports for analysis

```cmake
option(ENABLE_COVERAGE "Enable coverage" OFF)

if(ENABLE_COVERAGE AND CMAKE_BUILD_TYPE STREQUAL "Debug")
  if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(MyLib PRIVATE -fprofile-instr-generate -fcoverage-mapping)
    target_link_options(MyLib PRIVATE -fprofile-instr-generate -fcoverage-mapping)
  elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(MyLib PRIVATE --coverage -O0 -g)
    target_link_options(MyLib PRIVATE --coverage)
  endif()
endif()
```

### Coverage Goals

- **Aim for**: 80%+ line coverage
- **Critical paths**: 100% coverage (error handling, edge cases)
- **Focus on**: Public API, error paths, edge cases

## Mandatory Test Rule

### New/Changed Code → New/Updated Tests

- **New code**: Must include tests
- **Changed code**: Update existing tests or add new tests
- **Bug fixes**: Add regression test

**Rule**: No code merged without corresponding tests (unless explicitly exempted).

### Test Requirements

1. **Unit tests**: Test individual functions/classes in isolation
2. **Integration tests**: Test component interactions
3. **Error path tests**: Test failure modes and error handling
4. **Edge case tests**: Test boundary conditions

## Deterministic Tests

### No Sleeps or Delays

- **No `sleep()` or `std::this_thread::sleep_for()`**: Makes tests slow and flaky
- **Use timeouts**: If waiting is necessary, use timeouts with proper synchronization
- **Mock time**: Use time mocks for time-dependent code

**Bad**:
```cpp
TEST(ClientTest, WaitForResponse) {
  client.send_request();
  std::this_thread::sleep_for(std::chrono::seconds(1)); // Bad!
  EXPECT_TRUE(client.has_response());
}
```

**Good**:
```cpp
TEST(ClientTest, WaitForResponse) {
  client.send_request();
  auto result = client.wait_for_response(std::chrono::seconds(1));
  EXPECT_TRUE(result.has_value());
}
```

### No Network Calls

- **Mock network**: Use mocks for network operations
- **No real API calls**: Tests should not depend on external services
- **Use test doubles**: Stubs, mocks, fakes for external dependencies

**Bad**:
```cpp
TEST(ClientTest, RealAPI) {
  Client client("https://api.example.com");
  auto result = client.send_request(); // Real network call!
  EXPECT_TRUE(result.ok());
}
```

**Good**:
```cpp
class MockAPIClient : public APIClientInterface {
public:
  MOCK_METHOD(Response, send_request, (const Request&), (override));
};

TEST(ClientTest, MockedAPI) {
  auto mock_client = std::make_unique<MockAPIClient>();
  EXPECT_CALL(*mock_client, send_request(_))
    .WillOnce(Return(Response{Status::Ok}));
  
  Client client(std::move(mock_client));
  auto result = client.process();
  EXPECT_TRUE(result.ok());
}
```

### Deterministic Behavior

- **No random data**: Use fixed test data or seeded random
- **No file system**: Use in-memory or temporary files (use `/tmp` for temp files)
- **No environment dependencies**: Don't rely on environment variables (mock them)

## Test Organization

### Unit Tests

- **Purpose**: Test individual units in isolation
- **Scope**: Single function, class, or small component
- **Dependencies**: Mocked or stubbed

```cpp
TEST(ParserTest, ParsesValidJSON) {
  Parser parser;
  auto result = parser.parse(R"({"key": "value"})");
  EXPECT_TRUE(result.has_value());
  EXPECT_EQ(result->at("key"), "value");
}
```

### Integration Tests

- **Purpose**: Test component interactions
- **Scope**: Multiple components working together
- **Dependencies**: May use real implementations (but still mock external services)

```cpp
TEST(IntegrationTest, EndToEndWorkflow) {
  Config config = load_test_config();
  Client client(config);
  auto response = client.process_request("test");
  EXPECT_TRUE(response.ok());
  EXPECT_FALSE(response.data().empty());
}
```

### Test Fixtures and Utilities

- **Shared setup**: Use test fixtures for common setup
- **Test utilities**: Create helper functions in `test/support/`
- **Mock factories**: Create factories for common mocks

```cpp
// test/support/TestUtils.hpp
namespace TestSupport {
  Config create_test_config();
  Request create_test_request();
  std::unique_ptr<MockAPIClient> create_mock_client();
}
```

## Testing Error Paths

### Error Handling Tests

- **All error paths must be tested**: Every `return error` path
- **Exception tests**: Test exception paths if exceptions are used
- **Edge cases**: Boundary conditions, invalid inputs

```cpp
TEST(ClientTest, HandlesInvalidEndpoint) {
  Client client("invalid://endpoint");
  auto result = client.send_request();
  EXPECT_FALSE(result.ok());
  EXPECT_EQ(result.error(), Error::InvalidEndpoint);
}

TEST(ClientTest, HandlesTimeout) {
  auto mock_client = create_slow_mock_client();
  Client client(std::move(mock_client));
  auto result = client.send_request_with_timeout(std::chrono::milliseconds(100));
  EXPECT_FALSE(result.ok());
  EXPECT_EQ(result.error(), Error::Timeout);
}
```

### Failure Mode Testing

- **Network failures**: Test connection errors, timeouts
- **Invalid data**: Test malformed input, parsing errors
- **Resource exhaustion**: Test out-of-memory, file handle limits

## CTest Integration

### Register Tests

- Use CTest for test discovery and execution
- Register all test executables with `add_test()`

```cmake
add_executable(test_client test/test_client.cpp)
target_link_libraries(test_client PRIVATE MyLib gtest::gtest gtest::main)

add_test(NAME ClientTest COMMAND test_client)
```

### Test Labels

- Use labels to categorize tests
- Enable filtering by test type

```cmake
set_tests_properties(ClientTest PROPERTIES
  LABELS "unit;client"
)
```

## Best Practices

1. **Test naming**: Descriptive, indicate what is tested
2. **One assertion per test**: Focus on one behavior (when possible)
3. **Arrange-Act-Assert**: Clear structure in tests
4. **Test isolation**: Tests should not depend on each other
5. **Fast tests**: Tests should run quickly (< 1 second each)
6. **Clear failures**: Test failures should clearly indicate what went wrong

## Summary

1. **Framework**: Use GoogleTest (or Catch2)
2. **Layout**: Mirror source structure in `test/`
3. **Coverage**: Aim for 80%+, enable via `ENABLE_COVERAGE`
4. **Mandatory**: New/changed code requires tests
5. **Deterministic**: No sleeps, no network, no randomness
6. **Error paths**: All error paths must be tested
7. **Integration**: Use CTest for test discovery

## References

- See `50-cmake.md` for CMake test configuration
- See `70-ci.md` for CI test execution
- [GoogleTest Documentation](https://google.github.io/googletest/)

