#!/bin/bash

# Run API integration tests
# This script sets up the environment and runs the API tests

echo "=== LLMEngine API Integration Test Runner ==="
echo ""

# Check if we're in the right directory
if [ ! -f "test_api.cpp" ]; then
    echo "Error: Please run this script from the test/ directory"
    exit 1
fi

# Build the test if it doesn't exist
if [ ! -f "build_test/test_api" ]; then
    echo "Building test_api executable..."
    ./build_test.sh
    if [ $? -ne 0 ]; then
        echo "Build failed!"
        exit 1
    fi
fi

echo "Running API integration tests..."
echo ""

# Change to build directory and run tests
cd build_test

# Check for API key
if [ -z "$QWEN_API_KEY" ]; then
    echo "⚠️  No QWEN_API_KEY environment variable set"
    echo "   API tests will be skipped (config tests will still run)"
    echo ""
    echo "To test with Qwen API:"
    echo "   export QWEN_API_KEY='your-api-key'"
    echo "   ./run_api_tests.sh"
    echo ""
fi

# Run the test
./test_api

echo ""
echo "=== Test Complete ==="
