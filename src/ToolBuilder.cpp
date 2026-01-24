// Copyright © 2026 Eser KUBALI
// SPDX-License-Identifier: GPL-3.0-or-later

#include "LLMEngine/ToolBuilder.hpp"
#include <stdexcept>

namespace LLMEngine {

ToolBuilder::ToolBuilder(std::string_view name, std::string_view description)
    : name_(name), description_(description) {
    if (name.empty()) {
        throw std::invalid_argument("Function name cannot be empty");
    }
}

ToolBuilder ToolBuilder::createFunction(std::string_view name, std::string_view description) {
    return ToolBuilder(name, description);
}

ToolBuilder& ToolBuilder::addStringProperty(std::string_view name, std::string_view description, bool required) {
    properties_[name] = {
        {"type", "string"},
        {"description", description}
    };
    if (required) {
        required_.push_back(std::string(name));
    }
    return *this;
}

ToolBuilder& ToolBuilder::addNumberProperty(std::string_view name, std::string_view description, bool required) {
    properties_[name] = {
        {"type", "number"},
        {"description", description}
    };
    if (required) {
        required_.push_back(std::string(name));
    }
    return *this;
}

ToolBuilder& ToolBuilder::addIntegerProperty(std::string_view name, std::string_view description, bool required) {
    properties_[name] = {
        {"type", "integer"},
        {"description", description}
    };
    if (required) {
        required_.push_back(std::string(name));
    }
    return *this;
}

ToolBuilder& ToolBuilder::addBooleanProperty(std::string_view name, std::string_view description, bool required) {
    properties_[name] = {
        {"type", "boolean"},
        {"description", description}
    };
    if (required) {
        required_.push_back(std::string(name));
    }
    return *this;
}

ToolBuilder& ToolBuilder::addEnumProperty(std::string_view name, const std::vector<std::string>& values, std::string_view description, bool required) {
    properties_[name] = {
        {"type", "string"},
        {"description", description},
        {"enum", values}
    };
    if (required) {
        required_.push_back(std::string(name));
    }
    return *this;
}

nlohmann::json ToolBuilder::build() const {
    nlohmann::json parameters = {
        {"type", "object"},
        {"properties", properties_},
        {"required", required_}
    };
    
    // Strict mode is often useful but optional; OpenAI supports "strict": true
    // adding it implicitly if needed or via method (omitted for now)

    return {
        {"type", "function"},
        {"function", {
            {"name", name_},
            {"description", description_},
            {"parameters", parameters}
        }}
    };
}

} // namespace LLMEngine
