#!/bin/bash

# Copyright © 2026 Eser KUBALI <lxldev.contact@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later
#
# This script builds all LLMEngine example applications and is licensed under
# the GNU General Public License v3.0 or later.
# See the LICENSE file in the project root for details.

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

# Prefer consuming the exported package from build-tree or install
if [ ! -d "../build" ]; then
    echo "ℹ️  Build directory not found. Creating build directory and configuring root first."
    (cd ..; mkdir -p build; cd build; cmake ..)
fi

# Create build directory
echo "📁 Creating build directory..."
mkdir -p build_examples
cd build_examples

# Copy config file for examples that need it
echo "📋 Copying config file..."
mkdir -p config
cp ../../config/api_config.json config/

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
echo "  ./code_analyzer     - Code analysis and review tool"
echo "  ./text_processor   - Text processing and summarization"
echo "  ./file_analyzer    - File analysis tool"
echo "  ./translator       - Translation tool"
echo "  ./logo_generator   - AI-powered logo generation"
echo "  ./image_generator  - AI-powered image generation"
echo "  ./readme_chatbot   - README-based project chatbot"
echo "  ./multi_bot_analyzer - Multi-expert problem analyzer with democratic voting"
echo ""
echo "💡 Usage examples:"
echo "  ./chatbot"
echo "  ./code_analyzer main.cpp"
echo "  ./text_processor -s \"Your text here\""
echo "  ./file_analyzer document.txt"
echo "  ./translator \"Hello world\" Spanish"
echo "  ./logo_generator \"Modern tech logo\" png"
echo "  ./image_generator \"Beautiful sunset\" artwork png"
echo "  ./readme_chatbot https://github.com/user/repo"
echo "  ./multi_bot_analyzer \"How to optimize database\" 3 ollama qwen3:1.7b"
echo ""
echo "🔑 Don't forget to set your API key:"
echo "  export QWEN_API_KEY=\"your-key\""
echo "  export OPENAI_API_KEY=\"your-key\""
echo "  export ANTHROPIC_API_KEY=\"your-key\""
echo ""
echo "📖 For more information, see README.md"
