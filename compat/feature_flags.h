// Linux port: feature flags from src/features.xml. Stages "AlwaysEnabled" are
// modeled as compile-time-constant classes exposing IsEnabled(), matching the
// upstream generated header's interface (FEATURE_ALWAYS_ENABLED produces
// `Feature_X::IsEnabled() constexpr true`). "AlwaysDisabled" would be the same
// sealed struct returning false.
#pragma once

namespace
{
    struct Feature_KeypadModeEnabled
    {
        static constexpr bool IsEnabled() noexcept
        {
            return true;
        }
    };
    struct Feature_ScrollbarMarks
    {
        static constexpr bool IsEnabled() noexcept
        {
            return true;
        }
    };
    struct Feature_ShellCompletions
    {
        static constexpr bool IsEnabled() noexcept
        {
            return true;
        }
    };
    struct Feature_VtChecksumReport
    {
        static constexpr bool IsEnabled() noexcept
        {
            return true;
        }
    };
    struct Feature_AdjustIndistinguishableText
    {
        static constexpr bool IsEnabled() noexcept
        {
            return true;
        }
    };
}