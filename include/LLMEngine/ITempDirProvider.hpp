// Copyright © 2025 Eser KUBALI <lxldev.contact@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file is part of LLMEngine and is licensed under
// the GNU General Public License v3.0 or later.
// See the LICENSE file in the project root for details.

#pragma once
#include <string>
#include "LLMEngine/LLMEngineExport.hpp"

namespace LLMEngine {

/**
 * @brief Interface for providing temporary directory paths.
 * 
 * This interface allows injection of temporary directory providers for
 * thread-safety and testability. Implementations must be thread-safe.
 */
class LLMENGINE_EXPORT ITempDirProvider {
public:
    virtual ~ITempDirProvider() = default;
    
    /**
     * @brief Get the base temporary directory path.
     * @return The base temporary directory path (e.g., "/tmp/llmengine")
     * 
     * This method must be thread-safe and can be called concurrently
     * from multiple threads.
     */
    [[nodiscard]] virtual std::string getTempDir() const = 0;
};

/**
 * @brief Default implementation that returns a constant path.
 * 
 * ## Thread Safety
 * 
 * **DefaultTempDirProvider is thread-safe.** Uses a constexpr string_view
 * (or stored string), so no synchronization is needed. Multiple threads can
 * call getTempDir() concurrently without issues.
 * 
 * ## Ownership
 * 
 * Can be created on the stack or heap. Typically used as a default when
 * no custom provider is specified.
 */
class LLMENGINE_EXPORT DefaultTempDirProvider : public ITempDirProvider {
public:
    DefaultTempDirProvider() = default;
    explicit DefaultTempDirProvider(std::string_view base_path) : base_path_(base_path) {}
    
    [[nodiscard]] std::string getTempDir() const override {
        return std::string(base_path_);
    }

private:
    std::string_view base_path_ = "/tmp/llmengine";
};

} // namespace LLMEngine

