// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include <atomic>
#include <chrono>
#include <thread>

namespace til
{
    // Similar to std::atomic<T>::wait, but slightly faster and with the ability to specify a timeout.
    // Returns false on failure, which is pretty much always a timeout. (We prevent invalid arguments by taking references.)
    // Linux port: implemented on top of C++20 std::atomic::wait/notify, which
    // uses a futex underneath and thus behaves like the original WaitOnAddress.
    template<typename T>
    bool atomic_wait(const std::atomic<T>& atomic, const T& current, DWORD waitMilliseconds = INFINITE) noexcept
    {
        static_assert(sizeof(atomic) == sizeof(current));

        if (waitMilliseconds == INFINITE)
        {
            atomic.wait(current);
            return true;
        }

        const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(waitMilliseconds);
        do
        {
            if (atomic.load() != current)
            {
                return true;
            }
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        } while (std::chrono::steady_clock::now() < deadline);

        return atomic.load() != current;
    }

    // Wakes at most one of the threads waiting on the atomic via atomic_wait().
    // Don't use this with std::atomic<T>::wait, because it's not guaranteed to work in the future.
    template<typename T>
    void atomic_notify_one(const std::atomic<T>& atomic) noexcept
    {
        const_cast<std::atomic<T>*>(&atomic)->notify_one();
    }

    // Wakes all threads waiting on the atomic via atomic_wait().
    // Don't use this with std::atomic<T>::wait, because it's not guaranteed to work in the future.
    template<typename T>
    void atomic_notify_all(const std::atomic<T>& atomic) noexcept
    {
        const_cast<std::atomic<T>*>(&atomic)->notify_all();
    }
}
