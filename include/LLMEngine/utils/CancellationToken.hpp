// Copyright © 2026 Eser KUBALI <lxldev.contact@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once
#include <atomic>
#include <memory>

namespace LLMEngine {

/**
 * @brief Thread-safe token for signalling cancellation to long-running operations.
 */
struct CancellationToken {
    std::atomic<bool> cancelled{false};

    /**
     * @brief Request cancellation.
     *
     * This sets the cancelled flag to true. It is up to the operation being executed
     * to check this flag and terminate early.
     */
    void cancel() {
        cancelled.store(true, std::memory_order_relaxed);
    }

    /**
     * @brief Check if cancellation has been requested.
     */
    [[nodiscard]] bool isCancelled() const {
        return cancelled.load(std::memory_order_relaxed);
    }

    /**
     * @brief Create a new cancellation token.
     */
    static std::shared_ptr<CancellationToken> create() {
        return std::make_shared<CancellationToken>();
    }
};

} // namespace LLMEngine
