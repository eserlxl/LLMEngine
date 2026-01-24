// Copyright © 2026 Eser KUBALI
// SPDX-License-Identifier: GPL-3.0-or-later

#include "LLMEngine/ToolBuilder.hpp"
#include <stdexcept>

namespace LLMEngine {

ToolBuilder::ToolBuilder(std::string_view name, std::string_view description)
    : name_(name), description_(description) {
// Name can be empty for schema-only objects
}

ToolBuilder ToolBuilder::createFunction(std::string_view name, std::string_view description) {
    return ToolBuilder(name, description);
}

ToolBuilder ToolBuilder::createSchema(std::string_view description) {
    ToolBuilder builder("", description);
    builder.is_schema_only_ = true;
    return builder;
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



ToolBuilder& ToolBuilder::addObjectProperty(std::string_view name, const ToolBuilder& schema, std::string_view description, bool required) {
    nlohmann::json nested_json = schema.build();
    
    // Extract parameters if it's wrapped in function structure
    if (nested_json.contains("function")) {
        if (nested_json["function"].contains("parameters")) {
            nested_json = nested_json["function"]["parameters"];
        }
    } else if (nested_json.contains("type") && nested_json["type"] == "function") {
         if (nested_json.contains("function") && nested_json["function"].contains("parameters")) {
             nested_json = nested_json["function"]["parameters"];
         }
    }
    
    if (!description.empty()) {
        nested_json["description"] = description;
    }
    
    properties_[name] = nested_json;
    
    if (required) {
        required_.push_back(std::string(name));
    }
    return *this;
}

ToolBuilder& ToolBuilder::addArrayProperty(std::string_view name, const nlohmann::json& items_schema, std::string_view description, bool required) {
    properties_[name] = {
        {"type", "array"},
        {"items", items_schema},
        {"description", description}
    };
    if (required) {
        required_.push_back(std::string(name));
    }
    return *this;
}

ToolBuilder& ToolBuilder::setStrict(bool strict) {
    strict_ = strict;
    return *this;
}

nlohmann::json ToolBuilder::build() const {

    std::vector<std::string> effective_required = required_;

    if (strict_) {
        // In strict mode, all properties must be required.
        for (auto& [key, val] : properties_.items()) {
             bool found = false;
             for (const auto& req : effective_required) {
                 if (req == key) {
                     found = true;
                     break;
                 }
             }
             if (!found) {
                 effective_required.push_back(std::string(key));
             }
        }
    }

    nlohmann::json parameters = {
        {"type", "object"},
        {"properties", properties_},
        {"required", effective_required}
    };
    
    if (strict_) {
        parameters["additionalProperties"] = false;
    }

    if (is_schema_only_) {
        // Return just the schema object
        return parameters;
    }

    nlohmann::json tool = {
        {"type", "function"},
        {"function", {
            {"name", name_},
            {"description", description_},
            {"parameters", parameters}
        }}
    };
    
    // Validate name for tools
    if (name_.empty()) {
        throw std::invalid_argument("Function name cannot be empty for tools");
    }

    if (strict_) {
        tool["function"]["strict"] = true;
    }
    
    return tool;
}


} // namespace LLMEngine
