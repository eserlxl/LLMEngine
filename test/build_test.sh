#!/bin/bash

# Copyright © 2025 Eser KUBALI <lxldev.contact@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later
#
# This script is part of the LLMEngine test suite (build helper) and is
# licensed under the GNU General Public License v3.0 or later.
# See the LICENSE file in the project root for details.

# Build script for LLMEngine test
# This script builds the test executable using the existing libLLMEngine.a

echo "Building LLMEngine test..."

# Create build directory for test
mkdir -p build_test
cd build_test

# Clean the build directory
make clean

# Run CMake configuration
cmake ..

# Build the test executable
make -j20

if [ $? -eq 0 ]; then
    echo "Build successful!"
    echo "Test executables created:"
    echo "  - build_test/test_llmengine (legacy Ollama tests)"
    echo "  - build_test/test_api (API integration tests)"
    echo ""
    echo "To run the tests:"
    echo "  cd build_test"
    echo "  ./test_llmengine    # Legacy Ollama tests"
    echo "  ./test_api          # API integration tests"
    echo ""
    echo "For API tests with Qwen:"
    echo "  export QWEN_API_KEY='your-api-key'"
    echo "  ./test_api"
    echo ""
    echo "Note: Legacy test requires Ollama server on localhost:11434"
else
    echo "Build failed!"
    exit 1
fi
