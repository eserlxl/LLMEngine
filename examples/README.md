# LLMEngine Examples

This folder contains practical applications that demonstrate the capabilities of the LLMEngine library. Unlike unit tests, these examples are designed to solve real-world problems and showcase different use cases.

## 🚀 Quick Start

### Prerequisites

1. **Build the main library** (if not already done):
   ```bash
   cd ..
   mkdir -p build && cd build
   cmake ..
   make -j20
   cd ../examples
   ```

2. **Set up API key** (choose one):
   ```bash
   export QWEN_API_KEY="your-qwen-key"
   export OPENAI_API_KEY="your-openai-key"
   export ANTHROPIC_API_KEY="your-anthropic-key"
   ```

3. **Build examples**:
   ```bash
   ./build_examples.sh
   ```

4. **Run examples**:
   ```bash
   cd build_examples
   ./chatbot                    # Interactive chatbot
   ./logo_generator -i          # Interactive logo generator
   ./code_analyzer main.cpp     # Analyze code
   ```

## 📋 Available Examples (6 total)

### 1. 🤖 Chatbot (`chatbot.cpp`)
Interactive conversational AI application with multiple features.

**Features:**
- Multi-provider support (Qwen, OpenAI, Anthropic, Ollama)
- Conversation history management
- Built-in commands (help, clear, save, status)
- Automatic conversation logging
- Debug mode support

**Usage:**
```bash
./chatbot                    # Use default provider (Qwen)
./chatbot qwen              # Use Qwen provider
./chatbot openai            # Use OpenAI provider
./chatbot ollama llama2     # Use local Ollama with llama2 model
```

**Commands:**
- `help` - Show available commands
- `clear` - Clear conversation history
- `save` - Save conversation to file
- `status` - Show bot status and provider info
- `quit/exit/bye` - End conversation

### 2. 🔍 Code Analyzer (`code_analyzer.cpp`)
Comprehensive code analysis and review tool.

**Features:**
- Multi-language support (C++, Python, JavaScript, Java, Go, Rust, etc.)
- Different analysis types (comprehensive, security, performance, style, bugs)
- Directory analysis for multiple files
- File comparison functionality
- Detailed analysis reports

**Usage:**
```bash
./code_analyzer main.cpp                    # Analyze single file
./code_analyzer main.cpp security           # Security analysis
./code_analyzer -d src/                    # Analyze directory
./code_analyzer -c old.cpp new.cpp         # Compare files
```

**Analysis Types:**
- `comprehensive` - Full code review (default)
- `security` - Security vulnerability analysis
- `performance` - Performance optimization analysis
- `style` - Code style and best practices
- `bugs` - Bug detection and potential issues

### 3. 📝 Text Processor (`text_processor.cpp`)
Advanced text processing and analysis tool.

**Features:**
- Text summarization
- Keyword extraction
- Sentiment analysis
- Question generation
- Translation support
- Batch processing for directories

**Usage:**
```bash
./text_processor -s "Your text here" summary.txt    # Summarize text
./text_processor -f document.txt                    # Summarize file
./text_processor -k "Text to analyze..." 15        # Extract keywords
./text_processor -t "Hello world" Spanish          # Translate text
./text_processor -a "I love this product!"         # Analyze sentiment
./text_processor -q "Article content..." 10         # Generate questions
./text_processor -b ./documents summarize           # Batch process
```

**Operations:**
- `-s, --summarize` - Summarize text
- `-f, --file` - Summarize file
- `-k, --keywords` - Extract keywords
- `-t, --translate` - Translate text
- `-a, --analyze` - Analyze sentiment
- `-q, --questions` - Generate questions
- `-b, --batch` - Batch process directory

### 4. 📁 File Analyzer (`file_analyzer.cpp`)
Comprehensive file and directory analysis tool.

**Features:**
- File type detection and analysis
- Directory statistics and insights
- Duplicate file detection
- File size analysis
- Permission analysis
- Detailed reporting

**Usage:**
```bash
./file_analyzer document.txt                    # Analyze single file
./file_analyzer -d ./src/                      # Analyze directory
./file_analyzer -duplicates ./downloads/        # Find duplicates
./file_analyzer -report ./project/ report.txt   # Generate report
```

**Options:**
- `-d, --directory` - Analyze directory
- `-duplicates` - Find duplicate files
- `-report` - Generate detailed report

### 5. 🌍 Translator (`translator.cpp`)
Multi-language translation tool with advanced features.

**Features:**
- Support for 100+ languages
- File and directory translation
- Language detection
- Interactive mode
- Batch processing

**Usage:**
```bash
./translator "Hello world" Spanish              # Translate text
./translator -f document.txt French             # Translate file
./translator -b ./texts/ German                 # Batch translate
./translator -d "Bonjour le monde"              # Detect language
./translator -i                                  # Interactive mode
./translator -l                                  # Show languages
```

**Options:**
- `-f, --file` - Translate file
- `-b, --batch` - Batch translate directory
- `-d, --detect` - Detect language
- `-i, --interactive` - Interactive mode
- `-l, --languages` - Show supported languages

### 6. 🎨 Logo Generator (`logo_generator.cpp`)
AI-powered logo generation tool that creates professional logos from text descriptions.

**Features:**
- Generate logos from natural language descriptions
- Support for PNG and JPEG output formats
- Interactive mode for iterative design
- Batch logo generation
- SVG generation with automatic raster conversion
- Professional design specifications

**Usage:**
```bash
./logo_generator "Modern tech startup logo" png    # Generate single logo
./logo_generator -i                                 # Interactive mode
./logo_generator -m "desc1" "desc2" "desc3"        # Multiple logos
./logo_generator ollama llama2                     # Use local Ollama
```

**Examples:**
```bash
./logo_generator "Minimalist coffee shop logo with warm colors" png
./logo_generator "Tech company logo with geometric shapes and blue gradient" jpeg
./logo_generator "Vintage restaurant logo with elegant typography" png
```

**Interactive Commands:**
- `help` - Show available commands
- `list` - List all generated logos
- `clear` - Clear output directory
- `quit/exit/bye` - End session

**Output:**
- Logos saved to `generated_logos/` directory
- SVG files for vector graphics
- PNG/JPEG files for raster graphics (requires ImageMagick)

## 🔧 Building Examples

### Automatic Build
```bash
./build_examples.sh
```

### Manual Build
```bash
mkdir -p build_examples
cd build_examples
cmake ..
make -j20
```

### Individual Build
```bash
# Build specific example
g++ -std=c++17 ../chatbot.cpp -o chatbot \
    -I../../src \
    -L../../build -lLLMEngine \
    $(pkg-config --cflags --libs libcpr nlohmann_json)
```

## 🔑 API Key Setup

### Qwen (DashScope) - Recommended
1. Visit: https://dashscope.console.aliyuncs.com/
2. Sign up and create API key
3. Set environment variable:
   ```bash
   export QWEN_API_KEY="sk-your-api-key-here"
   ```

### OpenAI
```bash
export OPENAI_API_KEY="sk-your-openai-key"
```

### Anthropic
```bash
export ANTHROPIC_API_KEY="sk-ant-your-anthropic-key"
```

### Ollama (Local)
No API key needed. Just ensure Ollama is running:
```bash
ollama serve
```

## 📊 Performance Tips

1. **Use appropriate models:**
   - `qwen-flash` - Fastest, most cost-effective
   - `qwen-plus` - Balanced performance
   - `qwen-max` - Most capable for complex tasks

2. **Optimize for your use case:**
   - Code analysis: Use `qwen-max` for best results
   - Chatbot: Use `qwen-flash` for speed
   - Translation: Use `qwen-plus` for quality

3. **Batch processing:**
   - Process multiple files together
   - Use directory analysis for efficiency

## 🛠️ Customization

### Adding New Examples

1. **Create new C++ file** in examples directory
2. **Add to CMakeLists.txt**:
   ```cmake
   add_executable(your_example your_example.cpp)
   target_link_libraries(your_example LLMEngine ${CPR_LIBRARIES} ${NLOHMANN_JSON_LIBRARIES} ssl crypto pthread)
   ```
3. **Follow the pattern** of existing examples
4. **Rebuild** with `./build_examples.sh`

### Example Template

```cpp
#include "LLMEngine.hpp"
#include <iostream>

int main(int argc, char* argv[]) {
    // Get API key
    const char* api_key = std::getenv("QWEN_API_KEY");
    if (!api_key) {
        std::cerr << "❌ No API key found!" << std::endl;
        return 1;
    }
    
    try {
        // Create engine
        LLMEngine engine("qwen", api_key, "qwen-flash");
        
        // Your application logic here
        std::string prompt = "Your prompt here";
        nlohmann::json input = {};
        auto result = engine.analyze(prompt, input, "your_analysis_type");
        
        std::cout << "Result: " << result[1] << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "❌ Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
```

## 🐛 Troubleshooting

### Common Issues

1. **"No API key found"**
   - Set environment variable: `export QWEN_API_KEY="your-key"`
   - Check if variable is set: `echo $QWEN_API_KEY`

2. **"Main library not found"**
   - Build main library first: `cd .. && mkdir -p build && cd build && cmake .. && make -j20`

3. **"Network error"**
   - Check internet connection
   - Verify API key is valid
   - Try with debug mode enabled

4. **"Build errors"**
   - Install dependencies: `sudo apt install nlohmann-json3-dev libcpr-dev`
   - Clear build cache: `rm -rf build_examples && mkdir build_examples`

### Debug Mode

Enable debug mode for detailed logging:
```cpp
LLMEngine engine("qwen", api_key, "qwen-flash", {}, 24, true);
```

Debug files created:
- `api_response.json` - Full API response
- `api_response_error.json` - Error details
- `response_full.txt` - Complete response text

## 📚 Learning Resources

- **Main Library**: See `../README.md` for detailed documentation
- **API Integration**: See `../API_INTEGRATION_SUMMARY.md`
- **Configuration**: See `../config/README.md`
- **Quick Start**: See `../QUICKSTART.md`

## 🤝 Contributing

1. **Fork the repository**
2. **Create feature branch**: `git checkout -b feature-name`
3. **Add your example** following the existing patterns
4. **Test thoroughly** with different providers
5. **Update documentation** if needed
6. **Submit pull request**

## 📄 License

This project is licensed under the **GNU General Public License v3.0** - see the [LICENSE](../LICENSE) file for details.

---

**Ready to explore?** Set your API key and start building! 🚀
