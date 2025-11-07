// Copyright © 2025 Eser KUBALI
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once
#include <stdexcept>
#include <string>
#include <utility>

namespace LLMEngine {

// A tiny Expected-like result type for non-exception error propagation.
// Header-only; does not impose ABI changes on consumers.
template <typename T, typename E> class Result {
  public:
    static Result ok(T value) {
        return Result(std::in_place_index<0>, std::move(value));
    }
    static Result err(E error) {
        return Result(std::in_place_index<1>, std::move(error));
    }

    bool hasValue() const noexcept {
        return which_ == 0;
    }
    bool hasError() const noexcept {
        return which_ == 1;
    }
    explicit operator bool() const noexcept {
        return hasValue();
    }

    T& value() & {
        if (which_ != 0) {
            throw std::runtime_error("Result::value() called on error result");
        }
        return v_.value;
    }
    const T& value() const& {
        if (which_ != 0) {
            throw std::runtime_error("Result::value() called on error result");
        }
        return v_.value;
    }
    T&& value() && {
        if (which_ != 0) {
            throw std::runtime_error("Result::value() called on error result");
        }
        return std::move(v_.value);
    }

    E& error() & {
        if (which_ != 1) {
            throw std::runtime_error("Result::error() called on success result");
        }
        return v_.error;
    }
    const E& error() const& {
        if (which_ != 1) {
            throw std::runtime_error("Result::error() called on success result");
        }
        return v_.error;
    }
    E&& error() && {
        if (which_ != 1) {
            throw std::runtime_error("Result::error() called on success result");
        }
        return std::move(v_.error);
    }

    /**
     * @brief Map the value to a new type if the result is successful.
     *
     * @param f Function to apply to the value
     * @return Result with mapped value, or original error
     */
    template <typename U, typename F> Result<U, E> map(F&& f) const& {
        if (hasValue()) {
            return Result<U, E>::ok(f(value()));
        }
        return Result<U, E>::err(error());
    }

    template <typename U, typename F> Result<U, E> map(F&& f) && {
        if (hasValue()) {
            return Result<U, E>::ok(f(std::move(value())));
        }
        return Result<U, E>::err(std::move(error()));
    }

    /**
     * @brief Chain operations that return Result.
     *
     * @param f Function that takes value and returns Result<U, E>
     * @return Result from f, or original error
     */
    template <typename U, typename F> Result<U, E> andThen(F&& f) const& {
        if (hasValue()) {
            return f(value());
        }
        return Result<U, E>::err(error());
    }

    template <typename U, typename F> Result<U, E> andThen(F&& f) && {
        if (hasValue()) {
            return f(std::move(value()));
        }
        return Result<U, E>::err(std::move(error()));
    }

    /**
     * @brief Get value or return default if error.
     */
    T valueOr(T default_value) const& {
        return hasValue() ? value() : default_value;
    }

    T valueOr(T default_value) && {
        return hasValue() ? std::move(value()) : default_value;
    }

  private:
    template <std::size_t I> struct InPlaceTag {};

    template <std::size_t I, typename... Args>
    explicit Result(std::in_place_index_t<I>, Args&&... args) : which_(I) {
        if constexpr (I == 0)
            new (&v_.value) T(std::forward<Args>(args)...);
        else
            new (&v_.error) E(std::forward<Args>(args)...);
    }

    Result(const Result&) = delete;
    Result& operator=(const Result&) = delete;

    // Move constructor
    Result(Result&& other) noexcept : which_(other.which_) {
        if (which_ == 0) {
            new (&v_.value) T(std::move(other.v_.value));
            other.v_.value.~T();
        } else {
            new (&v_.error) E(std::move(other.v_.error));
            other.v_.error.~E();
        }
        other.which_ = -1; // Mark as moved-from
    }

    // Move assignment operator
    Result& operator=(Result&& other) noexcept {
        if (this != &other) {
            // Destroy current value
            if (which_ == 0) {
                v_.value.~T();
            } else if (which_ == 1) {
                v_.error.~E();
            }

            // Move from other
            which_ = other.which_;
            if (which_ == 0) {
                new (&v_.value) T(std::move(other.v_.value));
                other.v_.value.~T();
            } else {
                new (&v_.error) E(std::move(other.v_.error));
                other.v_.error.~E();
            }
            other.which_ = -1; // Mark as moved-from
        }
        return *this;
    }

  public:
    ~Result() {
        if (which_ == 0) {
            v_.value.~T();
        } else if (which_ == 1) {
            v_.error.~E();
        }
        // which_ == -1 means moved-from, no cleanup needed
    }

    int which_{0};
    union Storage {
        Storage() {}
        ~Storage() {}
        T value;
        E error;
    } v_;
};

} // namespace LLMEngine
