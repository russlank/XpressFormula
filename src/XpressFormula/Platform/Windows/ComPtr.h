// SPDX-License-Identifier: MIT
// ComPtr.h - Minimal COM pointer RAII helper for Windows platform services.
#pragma once

#include <utility>

namespace XpressFormula::Platform::Windows {

template <typename T>
class ComPtr {
public:
    ComPtr() noexcept = default;
    explicit ComPtr(T* ptr) noexcept
        : m_ptr(ptr) {
    }

    ComPtr(const ComPtr&) = delete;
    ComPtr& operator=(const ComPtr&) = delete;

    ComPtr(ComPtr&& other) noexcept
        : m_ptr(std::exchange(other.m_ptr, nullptr)) {
    }

    ComPtr& operator=(ComPtr&& other) noexcept {
        if (this != &other) {
            reset(std::exchange(other.m_ptr, nullptr));
        }
        return *this;
    }

    ~ComPtr() {
        reset();
    }

    [[nodiscard]] T* get() const noexcept {
        return m_ptr;
    }

    [[nodiscard]] T** put() noexcept {
        reset();
        return &m_ptr;
    }

    [[nodiscard]] T* detach() noexcept {
        return std::exchange(m_ptr, nullptr);
    }

    void reset(T* ptr = nullptr) noexcept {
        if (m_ptr) {
            m_ptr->Release();
        }
        m_ptr = ptr;
    }

    [[nodiscard]] explicit operator bool() const noexcept {
        return m_ptr != nullptr;
    }

    [[nodiscard]] T* operator->() const noexcept {
        return m_ptr;
    }

private:
    T* m_ptr = nullptr;
};

} // namespace XpressFormula::Platform::Windows

