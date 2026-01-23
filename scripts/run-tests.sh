#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2026 Şevket Eser KUBALI
# Run all unit tests and integration tests
# Usage: ./scripts/run-tests.sh [options]
# Options:
#   --unit-only      Run only unit tests
#   --integration-only    Run only integration tests
#   --verbose        Show verbose output
#   --output-dir     Specify output directory for test logs (default: build/test-results)

# Default options
RUN_UNIT=true
RUN_INTEGRATION=true
VERBOSE=false
OUTPUT_DIR="build/test-results"
BUILD_DIR="build"

# Parse arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --unit-only)
            RUN_INTEGRATION=false
            shift
            ;;
        --integration-only)
            RUN_UNIT=false
            shift
            ;;
        --verbose|-v)
            VERBOSE=true
            shift
            ;;
        --output-dir)
            OUTPUT_DIR="$2"
            shift 2
            ;;
        --build-dir)
            BUILD_DIR="$2"
            shift 2
            ;;
        --help|-h)
            echo "Usage: $0 [options]"
            echo "Options:"
            echo "  --unit-only           Run only unit tests"
            echo "  --integration-only    Run only integration tests"
            echo "  --verbose, -v         Show verbose output"
            echo "  --output-dir DIR      Specify output directory for test logs (default: build/test-results)"
            echo "  --build-dir DIR       Specify build directory (default: build)"
            echo "  --help, -h            Show this help message"
            exit 0
            ;;
        *)
            echo "Unknown option: $1"
            echo "Use --help for usage information"
            exit 1
            ;;
    esac
done

# Check if build directory exists
if [ ! -d "$BUILD_DIR" ]; then
    echo "Error: Build directory '$BUILD_DIR' not found."
    echo "Please run cmake and make first."
    exit 1
fi

# Change to build directory
cd "$BUILD_DIR" || { echo "Error: Cannot change to build directory"; exit 1; }

# Create output directory if it doesn't exist (relative to build directory or absolute)
if [[ "$OUTPUT_DIR" = /* ]]; then
    # Absolute path
    mkdir -p "$OUTPUT_DIR"
else
    # Relative path from build directory
    mkdir -p "$OUTPUT_DIR"
fi

# Set up LD_LIBRARY_PATH for test executables
export LD_LIBRARY_PATH="$(pwd):$LD_LIBRARY_PATH"

# Function to run tests with ctest
run_ctest() {
    local test_type=$1
    local test_label=$2
    local log_file="$OUTPUT_DIR/${test_type}-tests.log"
    
    echo "=========================================="
    echo "Running ${test_type} tests"
    echo "=========================================="
    
    local ctest_args="-L ${test_label}"
    if [ "$VERBOSE" = true ]; then
        ctest_args="$ctest_args -V"
    fi
    ctest_args="$ctest_args --output-on-failure"
    
    # Run ctest and capture both stdout and stderr
    # Use unbuffered output and ensure we see results
    set -o pipefail
    if ctest $ctest_args 2>&1 | tee "$log_file"; then
        echo ""
        echo "✓ ${test_type} tests completed successfully"
        return 0
    else
        echo ""
        echo "✗ ${test_type} tests failed. See log: $log_file"
        return 1
    fi
}

# Track overall result
UNIT_RESULT=0
INTEGRATION_RESULT=0

# Run unit tests
if [ "$RUN_UNIT" = true ]; then
    if ! run_ctest "unit" "unit"; then
        UNIT_RESULT=1
    fi
    echo ""
fi

# Run integration tests
if [ "$RUN_INTEGRATION" = true ]; then
    if ! run_ctest "integration" "integration"; then
        INTEGRATION_RESULT=1
    fi
    echo ""
fi

# Calculate overall result
OVERALL_RESULT=0
if [ "$RUN_UNIT" = true ] && [ $UNIT_RESULT -ne 0 ]; then
    OVERALL_RESULT=1
fi
if [ "$RUN_INTEGRATION" = true ] && [ $INTEGRATION_RESULT -ne 0 ]; then
    OVERALL_RESULT=1
fi

# Summary
echo "=========================================="
echo "Test Summary"
echo "=========================================="
if [ "$RUN_UNIT" = true ]; then
    if [ $UNIT_RESULT -eq 0 ]; then
        echo "Unit tests: PASSED"
    else
        echo "Unit tests: FAILED"
    fi
fi
if [ "$RUN_INTEGRATION" = true ]; then
    if [ $INTEGRATION_RESULT -eq 0 ]; then
        echo "Integration tests: PASSED"
    else
        echo "Integration tests: FAILED"
    fi
fi
echo ""
echo "Test logs saved to: $OUTPUT_DIR/"

# Exit with appropriate code
exit $OVERALL_RESULT
