// Copyright © 2025 Eser KUBALI <lxldev.contact@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file is part of LLMEngine and is licensed under
// the GNU General Public License v3.0 or later.
// See the LICENSE file in the project root for details.

#pragma once
#include "LLMEngine/LLMEngineExport.hpp"

#include <cstdlib>
#include <filesystem>
#include <stdexcept>
#include <string>

namespace LLMEngine {

/**
 * @brief Interface for providing temporary directory paths.
 *
 * This interface allows injection of temporary directory providers for
 * thread-safety and testability.
 *
 * ## Thread Safety Requirements
 *
 * **Implementations MUST be thread-safe.** The getTempDir() method may be
 * called concurrently from multiple threads.
 *
 * ## Ownership
 *
 * Implementations are typically owned via shared_ptr to allow sharing across
 * multiple LLMEngine instances. The lifetime is managed by the owner(s).
 *
 * ## Use Cases
 *
 * - Dependency injection for testing (mock temp directories)
 * - Per-tenant isolation in multi-tenant systems
 * - Custom cleanup policies
 */
class LLMENGINE_EXPORT ITempDirProvider {
  public:
    virtual ~ITempDirProvider() = default;

    /**
     * @brief Get the base temporary directory path.
     *
     * **Thread Safety:** This method MUST be thread-safe and can be called
     * concurrently from multiple threads.
     *
     * @return The base temporary directory path (e.g., "/tmp/llmengine")
     */
    [[nodiscard]] virtual std::string getTempDir() const = 0;
};

/**
 * @brief Default implementation that returns a constant path.
 *
 * ## Thread Safety
 *
 * **DefaultTempDirProvider is thread-safe.** Uses an owning std::string,
 * so no synchronization is needed. Multiple threads can call getTempDir()
 * concurrently without issues.
 *
 * ## Ownership
 *
 * Can be created on the stack or heap. Typically used as a default when
 * no custom provider is specified.
 */
class LLMENGINE_EXPORT DefaultTempDirProvider : public ITempDirProvider {
  public:
    DefaultTempDirProvider() {
        std::error_code ec;
        const auto base = std::filesystem::temp_directory_path(ec);
        if (ec) {
// Platform-specific fallback for temp directory
#ifdef _WIN32
            // Windows: Use %TEMP% or %TMP% environment variable, fallback to current directory
            const char* temp_env = std::getenv("TEMP");
            if (!temp_env)
                temp_env = std::getenv("TMP");
            if (temp_env) {
                base_path_ = std::filesystem::path(temp_env) / "llmengine";
            } else {
                // Last resort: use current directory (not ideal but better than /tmp on Windows)
                base_path_ = std::filesystem::current_path(ec) / "llmengine";
                if (ec) {
                    throw std::runtime_error("Failed to determine temporary directory: no system "
                                             "temp path and current directory unavailable");
                }
            }
#else
            // POSIX: Fallback to /tmp (exists on most Unix-like systems)
            base_path_ = std::filesystem::path{"/tmp"} / "llmengine";
#endif
        } else {
            base_path_ = base / "llmengine";
        }
        base_path_ = std::filesystem::weakly_canonical(base_path_, ec);
        if (ec) {
            // If canonicalization fails, fall back to generic form
            base_path_ = base_path_.lexically_normal();
        }
        base_path_str_ = base_path_.string();
    }
    explicit DefaultTempDirProvider(std::string_view base_path) {
        std::error_code ec;
        base_path_ = std::filesystem::path(base_path).lexically_normal();
        base_path_ = std::filesystem::weakly_canonical(base_path_, ec);
        if (ec) {
            base_path_ = base_path_.lexically_normal();
        }
        base_path_str_ = base_path_.string();
    }

    [[nodiscard]] std::string getTempDir() const override {
        return base_path_str_;
    }

  private:
    std::filesystem::path base_path_;
    std::string base_path_str_;
};

} // namespace LLMEngine
