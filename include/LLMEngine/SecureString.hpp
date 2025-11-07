// Copyright © 2025 Eser KUBALI <lxldev.contact@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file is part of LLMEngine and is licensed under
// the GNU General Public License v3.0 or later.
// See the LICENSE file in the project root for details.

#pragma once
#include "LLMEngine/LLMEngineExport.hpp"

#include <algorithm>
#include <cstring>
#include <string>
#include <string_view>

namespace LLMEngine {

/**
 * @brief Secure string wrapper that scrubs memory on destruction.
 *
 * This class provides a secure container for sensitive data (e.g., API keys)
 * that automatically overwrites the memory with zeros when destroyed, reducing
 * the window of exposure for secrets in memory.
 *
 * ## Security Considerations
 *
 * - **Memory scrubbing**: On destruction, the underlying memory is overwritten
 *   with zeros to reduce the risk of secrets persisting in memory dumps or
 *   swap files.
 * - **Limited lifetime**: Secrets should be stored in SecureString only for
 *   the minimum time necessary. Prefer passing secrets via environment variables
 *   or secure key management systems when possible.
 * - **Not foolproof**: Memory scrubbing does not guarantee complete security.
 *   The compiler may optimize away the scrubbing, or the memory may be copied
 *   elsewhere. This is a defense-in-depth measure, not a complete solution.
 *
 * ## Thread Safety
 *
 * SecureString is not thread-safe. Each instance should be used from a single
 * thread. If shared across threads, external synchronization is required.
 *
 * ## Usage
 *
 * ```cpp
 * SecureString api_key("sk-...");
 * // Use api_key.c_str() or api_key.view() as needed
 * // Memory is automatically scrubbed when api_key goes out of scope
 * ```
 */
class LLMENGINE_EXPORT SecureString {
  public:
    /**
     * @brief Construct from string_view.
     */
    explicit SecureString(std::string_view str) : data_(str) {}

    /**
     * @brief Construct from C-string.
     */
    explicit SecureString(const char* str) : data_(str ? str : "") {}

    /**
     * @brief Construct from std::string (moves if possible).
     */
    explicit SecureString(std::string str) : data_(std::move(str)) {}

    /**
     * @brief Copy constructor (creates a new secure copy).
     */
    SecureString(const SecureString& other) : data_(other.data_) {}

    /**
     * @brief Move constructor.
     */
    SecureString(SecureString&& other) noexcept : data_(std::move(other.data_)) {
        // Clear the moved-from object
        other.data_.clear();
    }

    /**
     * @brief Destructor - scrubs memory before destruction.
     */
    ~SecureString() { scrub(); }

    /**
     * @brief Copy assignment.
     */
    SecureString& operator=(const SecureString& other) {
        if (this != &other) {
            scrub();
            data_ = other.data_;
        }
        return *this;
    }

    /**
     * @brief Move assignment.
     */
    SecureString& operator=(SecureString&& other) noexcept {
        if (this != &other) {
            scrub();
            data_ = std::move(other.data_);
            other.data_.clear();
        }
        return *this;
    }

    /**
     * @brief Get a string_view of the data.
     */
    [[nodiscard]] std::string_view view() const noexcept { return data_; }

    /**
     * @brief Get a C-string pointer (null-terminated).
     */
    [[nodiscard]] const char* c_str() const noexcept { return data_.c_str(); }

    /**
     * @brief Get the underlying std::string (creates a copy).
     */
    [[nodiscard]] std::string str() const { return data_; }

    /**
     * @brief Check if the string is empty.
     */
    [[nodiscard]] bool empty() const noexcept { return data_.empty(); }

    /**
     * @brief Get the size of the string.
     */
    [[nodiscard]] size_t size() const noexcept { return data_.size(); }

    /**
     * @brief Explicitly scrub the memory (called automatically on destruction).
     */
    void scrub() noexcept {
        if (!data_.empty()) {
            // Overwrite memory with zeros
            // Use volatile to prevent compiler optimization
            volatile char* ptr = const_cast<volatile char*>(data_.data());
            std::fill_n(ptr, data_.size(), '\0');
            data_.clear();
        }
    }

    /**
     * @brief Clear and scrub the string immediately.
     */
    void clear() noexcept { scrub(); }

  private:
    std::string data_;
};

} // namespace LLMEngine

