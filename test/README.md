# LLMEngine Test Suite

This directory contains a comprehensive test suite for the LLMEngine library.

## Files

- `test.cpp` - Main test file that demonstrates LLMEngine usage
- `CMakeLists.txt` - CMake configuration for building the test
- `build_test.sh` - Build script for easy compilation
- `README.md` - This file

## Prerequisites

1. **Ollama Server**: A local Ollama instance must be running
   ```bash
   # Install Ollama (if not already installed)
   curl -fsSL https://ollama.ai/install.sh | sh
   
   # Start Ollama server
   ollama serve
   
   # Pull a model (e.g., llama3.2)
   ollama pull llama3.2
   ```

2. **Dependencies**: The following libraries must be installed:
   - nlohmann/json
   - cpr (C++ Requests library)
   - OpenSSL

## Building and Running

### Option 1: Using the build script (Recommended)
```bash
cd test
./build_test.sh
cd build_test
./test_llmengine
```

### Option 2: Manual build
```bash
cd test
mkdir build_test
cd build_test
cmake ..
make -j20
./test_llmengine
```

## Test Coverage

Coverage can be enabled locally without CI.

1. Configure a Debug build with coverage flags:
   ```bash
   cd ..
   mkdir -p build
   cd build
   cmake -DENABLE_COVERAGE=ON -DCMAKE_BUILD_TYPE=Debug ..
   make -j20
   ```

2. Build tests and run them. Temporary files are written under `/tmp` only.

3. Generate reports into `/tmp/llmengine-coverage`:
   ```bash
   cd ../test
   ./coverage.sh
   ```

The test suite covers:

1. **LLMEngine Initialization**: Tests basic constructor functionality
2. **Analysis Function**: Tests the main `analyze()` method with sample data
3. **Response Parsing**: Tests `LLMEngine::ResponseParser` for `<think>` extraction
4. **Utils Functions**: Tests utility functions like markdown stripping

## Configuration

Before running the test, the following may need to be modified in `test.cpp`:

- `model`: Default is "llama3.2" (change to your available model)
- `temperature`: Default is 0.7
- `debug`: Set to true for verbose output

Note: Ollama URL is configured via `config/api_config.json` or defaults to "http://localhost:11434"

## Expected Output

The test will:
1. Initialize the LLMEngine
2. Send a test analysis request to Ollama
3. Parse the response using `ResponseParser`
4. Test utility functions
5. Display results and statistics

## Troubleshooting

- **Connection Error**: Ensure Ollama server is running and accessible
- **Model Not Found**: Pull the required model with `ollama pull <model_name>`
- **Build Errors**: Check that all dependencies are installed
- **Permission Errors**: Ensure the build script is executable (`chmod +x build_test.sh`)

## Notes

- The test creates temporary files in the `/tmp` directory
- Debug mode is enabled by default for detailed output
- The test limits token usage to 100 for faster execution

## See also

- Root documentation: `../README.md`
- Quick start: `../QUICKSTART.md`
- Configuration guide: `../docs/CONFIGURATION.md`
