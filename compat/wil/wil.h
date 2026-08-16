/* Minimal WIL (Windows Implementation Libraries) compatibility for the Linux port.
   Implements just the handful of wil:: facilities used by the ported engine. */
#pragma once

#include <exception>
#include <functional>
#include <cstdarg>
#include <cstdlib>
#include <cwchar>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <unistd.h>

#include <windows.h>

namespace wil
{
    // ---- scope_exit -------------------------------------------------------
    template<typename Callable>
    class scope_exit
    {
    public:
        explicit scope_exit(Callable&& fn) noexcept :
            _fn(std::forward<Callable>(fn))
        {
        }

        scope_exit(scope_exit&& other) noexcept :
            _fn(std::move(other._fn)),
            _engaged(other._engaged)
        {
            other.release();
        }

        ~scope_exit()
        {
            if (_engaged)
            {
                _fn();
            }
        }

        void release() noexcept
        {
            _engaged = false;
        }

        scope_exit(const scope_exit&) = delete;
        scope_exit& operator=(const scope_exit&) = delete;

    private:
        Callable _fn;
        bool _engaged = true;
    };

    // ---- str_printf -------------------------------------------------------
    template<typename StringT = std::wstring>
    StringT str_printf(_In_z_ const wchar_t* format, ...)
    {
        va_list args;
        va_start(args, format);
        const auto needed = std::vswprintf(nullptr, 0, format, args);
        va_end(args);
        StringT result(static_cast<size_t>(needed), L'\0');
        va_start(args, format);
        std::vswprintf(result.data(), result.size() + 1, format, args);
        va_end(args);
        return result;
    }

    // ---- zwstring_view ----------------------------------------------------
    using zwstring_view = std::wstring_view;

    // ---- ResultFromCaughtException ---------------------------------------
    inline HRESULT ResultFromCaughtException() noexcept
    {
        try
        {
            throw;
        }
        catch (const std::exception&)
        {
            return E_FAIL;
        }
        catch (...)
        {
            return E_FAIL;
        }
    }

    // ---- unique handles ---------------------------------------------------

    // On Linux, a HANDLE is a raw file descriptor (stored as a void*), so the
    // deletes must close() instead of delete. Address and ownership are kept by
    // a small wrapper class rather than std::unique_ptr.
    class handle_deleter
    {
    public:
        void operator()(HANDLE h) const noexcept
        {
            if (h != nullptr && h != INVALID_HANDLE_VALUE)
            {
                ::close(reinterpret_cast<std::intptr_t>(h));
            }
        }
    };

    class unique_hfile_t
    {
    public:
        unique_hfile_t() noexcept = default;
        explicit unique_hfile_t(HANDLE h) noexcept :
            _h(h)
        {
        }
        ~unique_hfile_t() noexcept
        {
            reset();
        }
        unique_hfile_t(unique_hfile_t&& other) noexcept :
            _h(other.release())
        {
        }
        unique_hfile_t& operator=(unique_hfile_t&& other) noexcept
        {
            if (this != &other)
            {
                reset(other.release());
            }
            return *this;
        }
        unique_hfile_t(const unique_hfile_t&) = delete;
        unique_hfile_t& operator=(const unique_hfile_t&) = delete;

        void reset(HANDLE h = nullptr) noexcept
        {
            if (_h != nullptr && _h != INVALID_HANDLE_VALUE)
            {
                ::close(reinterpret_cast<std::intptr_t>(_h));
            }
            _h = h;
        }
        HANDLE release() noexcept
        {
            const auto h = _h;
            _h = nullptr;
            return h;
        }
        HANDLE* addressof() noexcept
        {
            return &_h;
        }
        HANDLE get() const noexcept
        {
            return _h;
        }
        HANDLE* put() noexcept
        {
            reset();
            return &_h;
        }
        explicit operator bool() const noexcept
        {
            return _h != nullptr && _h != INVALID_HANDLE_VALUE;
        }

    private:
        HANDLE _h = nullptr;
    };

    using unique_hfile = unique_hfile_t;
    using unique_handle = unique_hfile_t;

    // On Windows this RAII type wraps a VirtualAlloc'd buffer. Here it wraps a
    // heap-allocated array instead, which is what the ROW/TextBuffer arena uses.
    template<typename T>
    class unique_virtualalloc_ptr
    {
    public:
        unique_virtualalloc_ptr() = default;
        explicit unique_virtualalloc_ptr(T* p) noexcept :
            _p(p)
        {
        }
        ~unique_virtualalloc_ptr() noexcept
        {
            reset();
        }
        unique_virtualalloc_ptr(unique_virtualalloc_ptr&& other) noexcept :
            _p(other.release())
        {
        }
        unique_virtualalloc_ptr& operator=(unique_virtualalloc_ptr&& other) noexcept
        {
            if (this != &other)
            {
                reset(other.release());
            }
            return *this;
        }
        unique_virtualalloc_ptr(const unique_virtualalloc_ptr&) = delete;
        unique_virtualalloc_ptr& operator=(const unique_virtualalloc_ptr&) = delete;

        T* get() const noexcept
        {
            return _p;
        }
        T** put() noexcept
        {
            reset();
            return &_p;
        }
        T& operator[](ptrdiff_t i) noexcept
        {
            return _p[i];
        }
        const T& operator[](ptrdiff_t i) const noexcept
        {
            return _p[i];
        }
        T& operator*() noexcept
        {
            return *_p;
        }
        T* operator->() noexcept
        {
            return _p;
        }
        T* release() noexcept
        {
            auto p = _p;
            _p = nullptr;
            return p;
        }
        void reset(T* p = nullptr) noexcept
        {
            std::free(_p);
            _p = p;
        }
        explicit operator bool() const noexcept
        {
            return _p != nullptr;
        }

    private:
        T* _p = nullptr;
    };
}

// ---- return / throw helpers used by the engine ---------------------------
namespace wil
{
    class ResultException
    {
    public:
        explicit ResultException(const HRESULT hr) noexcept :
            _hr(hr)
        {
        }
        HRESULT GetErrorCode() const noexcept
        {
            return _hr;
        }

    private:
        HRESULT _hr;
    };
}

#define THROW_HR(hr) throw ::wil::ResultException(static_cast<HRESULT>(hr))
#define THROW_HR_IF(hr, cond) \
    do                        \
    {                         \
        if (cond)             \
        {                     \
            THROW_HR(hr);     \
        }                     \
    } while (0, 0)
#define THROW_WIN32(err) THROW_HR(HRESULT_FROM_WIN32(err))
#define THROW_LAST_ERROR() THROW_WIN32(::GetLastError())
// WIL's _IF_NULL variants evaluate to their argument so they can be used as a
// value expression (e.g. `THROW_LAST_ERROR_IF_NULL(VirtualAlloc(...))`).
#define THROW_LAST_ERROR_IF_NULL(ptr) \
    ((ptr) ? (ptr) : (THROW_LAST_ERROR(), (ptr)))
#define THROW_HR_IF_NULL(hr, ptr) \
    ((ptr) ? (ptr) : (THROW_HR(hr), (ptr)))
#define THROW_IF_WIN32_BOOL_FALSE(expr) \
    do                                  \
    {                                   \
        if (!(expr))                    \
        {                               \
            THROW_LAST_ERROR();         \
        }                               \
    } while (0, 0)
#define THROW_WIN32_IF_MSG(err, cond, ...) \
    do                                     \
    {                                      \
        if (cond)                          \
        {                                  \
            THROW_WIN32(err);              \
        }                                  \
    } while (0, 0)
#define THROW_IF_NTSTATUS_FAILED(nt) \
    do                               \
    {                                \
        if (!NT_SUCCESS(nt))         \
        {                            \
            THROW_HR(E_FAIL);        \
        }                            \
    } while (0, 0)

#define RETURN_IF_FAILED(hr)            \
    do                                  \
    {                                   \
        const auto _hrRet = (hr);       \
        if (FAILED(_hrRet))             \
        {                               \
            return _hrRet;              \
        }                               \
    } while (0, 0)
#define RETURN_HR_IF(hr, cond) \
    do                        \
    {                         \
        if (cond)             \
        {                     \
            return (hr);      \
        }                     \
    } while (0, 0)
#define RETURN_HR(hr) return (hr)
#define RETURN_WIN32(err) return HRESULT_FROM_WIN32(err)
#define RETURN_IF_WIN32_BOOL_FALSE(cond) \
    do                                   \
    {                                    \
        if (!(cond))                     \
        {                                \
            return E_FAIL;               \
        }                                \
    } while (0, 0)

// grc:: no-op macro used in headers (e.g. ConhostInternalGetSet sites)
#define LOG_IF_FAILED(hr)
#define LOG_HR(hr) (hr)
#define LOG_CAUGHT_EXCEPTION()
#define LOG_IF_WIN32_ERROR(err) (err)
#define RETURN_IF_WIN32_ERROR(err) \
    do                             \
    {                              \
        const auto _e = (err);     \
        if (_e != 0)               \
        {                          \
            return _e;             \
        }                          \
    } while (0, 0)

#define CATCH_RETURN()                    \
    catch (...)                           \
    {                                     \
        return wil::ResultFromCaughtException(); \
    }                                     \
    static_assert(true, "")
#define CATCH_RETURN_HR_IF(hr, cond) \
    catch (...)                      \
    {                                \
        if (cond)                    \
        {                            \
            return (hr);             \
        }                            \
        return wil::ResultFromCaughtException(); \
    }                                \
    static_assert(true, "")

#define CATCH_FAIL_FAST() \
    catch (...)           \
    {                     \
        std::abort();     \
    }                     \
    ;

#define THROW_IF_FAILED(hr)        \
    do                             \
    {                              \
        const auto _hr = (hr);     \
        if (FAILED(_hr))           \
        {                          \
            throw ::wil::ResultException(_hr); \
        }                          \
    } while (0, 0)
#define THROW_HR_IF_NULL(hr, p)        \
    do                                 \
    {                                  \
        if ((p) == nullptr)            \
        {                              \
            throw ::wil::ResultException(hr); \
        }                              \
    } while (0, 0)

#define LOG_HR_IF(hr, cond) (hr)
#define SUCCEEDED_LOG(hr) (SUCCEEDED(hr))

// --- wil flag / enum helpers -------------------------------------------------

namespace wil
{
    inline constexpr bool FAIL_FAST_ENABLED = true;

    template<typename T>
    constexpr auto _wi_cast(T v) noexcept
    {
        if constexpr (std::is_enum_v<T>)
        {
            return static_cast<std::underlying_type_t<T>>(v);
        }
        else
        {
            return v;
        }
    }
}

#define WI_EnumValue(v) static_cast<std::underlying_type_t<std::remove_reference_t<decltype(v)>>>(v)

template<typename T1, typename T2>
constexpr bool WI_IsFlagSet(T1 value, T2 flag) noexcept
{
    return (static_cast<unsigned long long>(wil::_wi_cast(value)) & static_cast<unsigned long long>(wil::_wi_cast(flag))) != 0;
}

template<typename T1, typename T2>
constexpr bool WI_IsFlagClear(T1 value, T2 flag) noexcept
{
    return (static_cast<unsigned long long>(wil::_wi_cast(value)) & static_cast<unsigned long long>(wil::_wi_cast(flag))) == 0;
}

template<typename T1, typename T2>
constexpr bool WI_IsAnyFlagSet(T1 value, T2 flags) noexcept
{
    return (static_cast<unsigned long long>(wil::_wi_cast(value)) & static_cast<unsigned long long>(wil::_wi_cast(flags))) != 0;
}

template<typename T1, typename T2>
constexpr bool WI_AreAllFlagsSet(T1 value, T2 flags) noexcept
{
    return (static_cast<unsigned long long>(wil::_wi_cast(value)) & static_cast<unsigned long long>(wil::_wi_cast(flags))) == static_cast<unsigned long long>(wil::_wi_cast(flags));
}

template<typename T1, typename T2>
constexpr bool WI_AreAllFlagsClear(T1 value, T2 flags) noexcept
{
    return (static_cast<unsigned long long>(wil::_wi_cast(value)) & static_cast<unsigned long long>(wil::_wi_cast(flags))) == 0;
}

template<typename T1, typename T2>
constexpr T1 WI_SetFlag(T1& value, T2 flag) noexcept
{
    const auto updated = static_cast<unsigned long long>(wil::_wi_cast(value)) | static_cast<unsigned long long>(wil::_wi_cast(flag));
    value = static_cast<T1>(updated);
    return value;
}

template<typename T1, typename T2>
constexpr T1 WI_ClearFlag(T1& value, T2 flag) noexcept
{
    const auto updated = static_cast<unsigned long long>(wil::_wi_cast(value)) & ~static_cast<unsigned long long>(wil::_wi_cast(flag));
    value = static_cast<T1>(updated);
    return value;
}

template<typename T1, typename T2>
constexpr T1 WI_ToggleFlag(T1& value, T2 flag) noexcept
{
    const auto updated = static_cast<unsigned long long>(wil::_wi_cast(value)) ^ static_cast<unsigned long long>(wil::_wi_cast(flag));
    value = static_cast<T1>(updated);
    return value;
}

template<typename T1, typename T2>
constexpr T1 WI_UpdateFlag(T1& value, T2 flag, bool set) noexcept
{
    const auto flagBits = static_cast<unsigned long long>(wil::_wi_cast(flag));
    const auto valueBits = static_cast<unsigned long long>(wil::_wi_cast(value));
    value = static_cast<T1>(set ? (valueBits | flagBits) : (valueBits & ~flagBits));
    return value;
}

template<typename T1, typename T2>
constexpr void WI_SetFlagIf(T1& value, T2 flag, bool set) noexcept
{
    value = set ? value | flag : value & ~flag;
}

template<typename T1, typename T2>
constexpr void WI_ClearFlagIf(T1& value, T2 flag, bool clear) noexcept
{
    if (clear)
    {
        WI_ClearFlag(value, flag);
    }
}

template<typename T1>
constexpr T1 WI_SetAllFlags(T1& value) noexcept
{
    value = static_cast<T1>(~static_cast<unsigned long long>(0));
    return value;
}

template<typename T1>
constexpr void WI_ClearAllFlags(T1& value) noexcept
{
    value = static_cast<T1>(0);
}

template<typename T1, typename T2>
constexpr void WI_ClearAllFlags(T1& value, const T2 flags) noexcept
{
    value = static_cast<T1>(value & static_cast<T1>(~flags));
}

#ifdef WI_NOEXCEPT
#undef WI_NOEXCEPT
#endif
#define WI_NOEXCEPT noexcept

#ifdef WI_ASSERT
#undef WI_ASSERT
#endif
#define WI_ASSERT(cond) assert(cond)

// --- enum flag operators used by DEFINE_ENUM_FLAG_OPERATORS -------------------

#define DEFINE_ENUM_FLAG_OPERATORS(T)                                    \
    constexpr T operator|(T a, T b) noexcept                             \
    {                                                                    \
        return static_cast<T>(wil::_wi_cast(a) | wil::_wi_cast(b));       \
    }                                                                    \
    constexpr T operator&(T a, T b) noexcept                             \
    {                                                                    \
        return static_cast<T>(wil::_wi_cast(a) & wil::_wi_cast(b));       \
    }                                                                    \
    constexpr T operator^(T a, T b) noexcept                             \
    {                                                                    \
        return static_cast<T>(wil::_wi_cast(a) ^ wil::_wi_cast(b));       \
    }                                                                    \
    constexpr T operator~(T a) noexcept                                  \
    {                                                                    \
        return static_cast<T>(~wil::_wi_cast(a));                         \
    }                                                                    \
    constexpr T& operator|=(T& a, T b) noexcept                          \
    {                                                                    \
        a = a | b;                                                       \
        return a;                                                        \
    }                                                                    \
    constexpr T& operator&=(T& a, T b) noexcept                          \
    {                                                                    \
        a = a & b;                                                       \
        return a;                                                        \
    }                                                                    \
    constexpr T& operator^=(T& a, T b) noexcept                          \
    {                                                                    \
        a = a ^ b;                                                       \
        return a;                                                        \
    }

// --- fail-fast helpers ------------------------------------------------------

#include <cstdlib>
#include <cstdio>

#define FAIL_FAST()                              \
    do                                           \
    {                                            \
        std::abort();                            \
    } while (0, 0)

#define FAIL_FAST_IF(cond)                       \
    do                                           \
    {                                            \
        if (cond)                                \
        {                                        \
            std::fprintf(stderr, "FAIL_FAST_IF: %s\n", #cond); \
            std::abort();                        \
        }                                        \
    } while (0, 0)

#define FAIL_FAST_HR(hr)                         \
    do                                           \
    {                                            \
        std::fprintf(stderr, "FAIL_FAST_HR: 0x%08lx\n", static_cast<unsigned long>(hr)); \
        std::abort();                            \
    } while (0, 0)

#define FAIL_FAST_LAST_ERROR_IF(cond)            \
    do                                           \
    {                                            \
        if (cond)                                \
        {                                        \
            std::abort();                        \
        }                                        \
    } while (0, 0)


// --- function_deleter -------------------------------------------------------
namespace wil
{
    // Calls the given free function to release a pointer, for std::unique_ptr.
    template<typename Callable, Callable function>
    struct function_deleter
    {
        template<typename T>
        void operator()(T* ptr) const noexcept
        {
            function(ptr);
        }
    };
}

// --- unique_struct ----------------------------------------------------------
// RAII for opaque C structs that are closed by a free function, matching
// wil::unique_struct. The engine code writes fields straight through the
// wrapper (`ut.context = ...`), so we inherit from T to expose them directly.
namespace wil
{
    template<typename T, typename CloseFnPtr, CloseFnPtr CloseFn>
    class unique_struct : public T
    {
    public:
        unique_struct() = default;
        explicit unique_struct(const T& value) noexcept :
            T(value),
            _owned{ true }
        {
        }
        unique_struct(unique_struct&& other) noexcept :
            T(other),
            _owned{ true }
        {
            other._owned = false;
        }
        unique_struct& operator=(unique_struct&& other) noexcept
        {
            if (this != &other)
            {
                reset();
                static_cast<T&>(*this) = static_cast<T&>(other);
                _owned = other._owned;
                other._owned = false;
            }
            return *this;
        }
        unique_struct(const unique_struct&) = delete;
        unique_struct& operator=(const unique_struct&) = delete;

        ~unique_struct() noexcept
        {
            reset();
        }

        void reset() noexcept
        {
            if (_owned)
            {
                CloseFn(static_cast<T*>(this));
                _owned = false;
            }
        }

        T* get() noexcept
        {
            return static_cast<T*>(this);
        }
        const T* get() const noexcept
        {
            return static_cast<const T*>(this);
        }

    private:
        bool _owned{ true };
    };
}
