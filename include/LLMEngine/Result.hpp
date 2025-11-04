// Copyright © 2025 Eser KUBALI
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once
#include <utility>
#include <string>

// A tiny Expected-like result type for non-exception error propagation.
// Header-only; does not impose ABI changes on consumers.
template <typename T, typename E>
class Result {
public:
    static Result ok(T value) {
        return Result(std::in_place_index<0>, std::move(value));
    }
    static Result err(E error) {
        return Result(std::in_place_index<1>, std::move(error));
    }

    bool hasValue() const noexcept { return which_ == 0; }
    explicit operator bool() const noexcept { return hasValue(); }

    T& value() & { return v_.value; }
    const T& value() const & { return v_.value; }
    T&& value() && { return std::move(v_.value); }

    E& error() & { return v_.error; }
    const E& error() const & { return v_.error; }
    E&& error() && { return std::move(v_.error); }

private:
    template <std::size_t I>
    struct InPlaceTag { };

    template <std::size_t I, typename... Args>
    explicit Result(std::in_place_index_t<I>, Args&&... args) : which_(I) {
        if constexpr (I == 0) new (&v_.value) T(std::forward<Args>(args)...);
        else new (&v_.error) E(std::forward<Args>(args)...);
    }

    Result(const Result&) = delete;
    Result& operator=(const Result&) = delete;

    ~Result() {
        if (which_ == 0) {
            v_.value.~T();
        } else {
            v_.error.~E();
        }
    }

    int which_ {0};
    union Storage {
        Storage() {}
        ~Storage() {}
        T value;
        E error;
    } v_;
};


