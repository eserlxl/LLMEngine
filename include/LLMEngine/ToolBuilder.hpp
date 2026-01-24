// Copyright © 2026 Eser KUBALI
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once
#include "LLMEngine/LLMEngineExport.hpp"
#include <nlohmann/json.hpp>
#include <string>
#include <string_view>
#include <vector>

namespace LLMEngine {

/**
 * @brief Builder for creating type-safe Function Tool definitions.
 *
 * Follows the OpenAI function calling JSON schema format.
 */
class LLMENGINE_EXPORT ToolBuilder {
public:
    /**
     * @brief Start building a function tool.
     * @param name Function name (e.g., "get_weather").
     * @param description Description of what the function does.
     */
    /**
     * @brief Start building a function tool.
     * @param name Function name (e.g., "get_weather").
     * @param description Description of what the function does.
     */
    static ToolBuilder createFunction(std::string_view name, std::string_view description);

    /**
     * @brief Create a generic Object Schema builder (no function wrapper).
     * Useful for building nested objects or response_format schemas.
     */
    static ToolBuilder createSchema(std::string_view description = "");


    /**
     * @brief Add a string property to the function parameters.
     * @param name Property name.
     * @param description Description of the property.
     * @param required Whether this property is mandatory.
     */
    ToolBuilder& addStringProperty(std::string_view name, std::string_view description, bool required = false);

    /**
     * @brief Add a number/integer property.
     * @param name Property name.
     * @param description Description.
     * @param required Mandatory status.
     */
    ToolBuilder& addNumberProperty(std::string_view name, std::string_view description, bool required = false);
    ToolBuilder& addIntegerProperty(std::string_view name, std::string_view description, bool required = false);

    /**
     * @brief Add a boolean property.
     */
    ToolBuilder& addBooleanProperty(std::string_view name, std::string_view description, bool required = false);

    /**
     * @brief Add an enumeration property (string with allowed values).
     */
    ToolBuilder& addEnumProperty(std::string_view name, const std::vector<std::string>& values, std::string_view description, bool required = false);

    /**
     * @brief Add a nested object property.
     * @param name Property name.
     * @param schema A ToolBuilder configured as the schema for this object.
     */
    ToolBuilder& addObjectProperty(std::string_view name, const ToolBuilder& schema, std::string_view description, bool required = false);

    /**
     * @brief Add an array property.
     * @param name Property name.
     * @param items_schema JSON schema for items.
     */
    ToolBuilder& addArrayProperty(std::string_view name, const nlohmann::json& items_schema, std::string_view description, bool required = false);

    /**
     * @brief Enable Strict Mode (Structured Outputs).
     * Adds "strict": true and "additionalProperties": false.
     */
    ToolBuilder& setStrict(bool strict);


    /**
     * @brief Finalize and return the JSON tool object.
     */
    [[nodiscard]] nlohmann::json build() const;

private:
    ToolBuilder(std::string_view name, std::string_view description);

    std::string name_;
    std::string description_;
    nlohmann::json properties_;
    std::vector<std::string> required_;
    bool strict_ = false;
    bool is_schema_only_ = false; // If true, build() returns the schema directly, not wrapped in "function"


};

} // namespace LLMEngine
