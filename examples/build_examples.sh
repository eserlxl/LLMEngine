#!/bin/bash

# LLMEngine Examples Build Script
# This script builds all example applications

set -e  # Exit on any error

echo "🔨 Building LLMEngine Examples"
echo "=============================="

# Check if we're in the right directory
if [ ! -f "CMakeLists.txt" ]; then
    echo "❌ Error: CMakeLists.txt not found. Please run this script from the examples directory."
    exit 1
fi

# Check if main library is built
if [ ! -f "../build/libLLMEngine.a" ]; then
    echo "❌ Error: Main library not found. Please build the main library first:"
    echo "   cd .. && mkdir -p build && cd build && cmake .. && make -j20"
    exit 1
fi

# Create build directory
echo "📁 Creating build directory..."
mkdir -p build_examples
cd build_examples

# Configure with CMake
echo "⚙️  Configuring with CMake..."
cmake ..

# Build all examples
echo "🔨 Building examples..."
make -j20

echo ""
echo "✅ Build completed successfully!"
echo ""
echo "📋 Available examples:"
echo "  ./chatbot          - Interactive chatbot"
echo "  ./code_analyzer    - Code analysis and review tool"
echo "  ./text_processor   - Text processing and summarization"
echo "  ./file_analyzer    - File analysis tool"
echo "  ./translator       - Translation tool"
echo ""
echo "💡 Usage examples:"
echo "  ./chatbot"
echo "  ./code_analyzer main.cpp"
echo "  ./text_processor -s \"Your text here\""
echo "  ./file_analyzer document.txt"
echo "  ./translator \"Hello world\" Spanish"
echo ""
echo "🔑 Don't forget to set your API key:"
echo "  export QWEN_API_KEY=\"your-key\""
echo "  export OPENAI_API_KEY=\"your-key\""
echo "  export ANTHROPIC_API_KEY=\"your-key\""
echo ""
echo "📖 For more information, see README.md"
