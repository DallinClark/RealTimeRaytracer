module;

#include <chrono>
#include <format>
#include <print>
#include <source_location>
#include <string_view>

export module core.log;

export namespace core::log {

// ─────────────────── enums & helpers ───────────────────
enum class Level : std::uint8_t { trace, debug, info, warn, error, critical, off };

[[nodiscard]] consteval std::string_view to_string(const Level level) noexcept {
    switch (level) {
        case Level::trace:    return "TRACE";
        case Level::debug:    return "DEBUG";
        case Level::info:     return "INFO";
        case Level::warn:     return "WARN";
        case Level::error:    return "ERROR";
        case Level::critical: return "CRIT";
        default:              return "UNKNOWN";
    }
}

// ──────────── compile-time verbosity switch ───────────
#ifndef LOG_LEVEL
    inline constexpr auto compile_time_level = Level::info;
#else
    inline constexpr auto compile_time_level = static_cast<Level>(ATTO_LOG_LEVEL);
#endif

// ────── output sink (replace with file possible) ──────
using Sink = void(*)(std::string_view);
inline Sink sink = +[](std::string_view s){ std::print(stderr, "{}", s); };

// ─────────────────── core formatter ───────────────────
template<Level level, class... Args>
constexpr void log(
        std::format_string<Args...> fmt,
        Args&&...                   args
) {
    if constexpr (level < compile_time_level) return;   // fully removed at compile time if unneeded

    const std::source_location  loc = std::source_location::current();

    using clock = std::chrono::steady_clock;
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(clock::now().time_since_epoch()).count();

    auto header = std::format(
            "[{:>10} ms] [{:^7}] {:>20}:{:<3} ",
            ms,
            to_string(level),
            loc.file_name(),
            loc.line()
    );

    auto body = std::format(fmt, std::forward<Args>(args)...);
    sink(std::format("{}{}\n", header, body));
}

// ──────────────── convenience wrappers ────────────────
template<class... A>
constexpr void trace   (std::format_string<A...> f, A&&... a)
{ log<Level::trace   >(f, std::forward<A>(a)...); }

template<class... A>
constexpr void debug   (std::format_string<A...> f, A&&... a)
{ log<Level::debug   >(f, std::forward<A>(a)...); }

template<class... A>
constexpr void info    (std::format_string<A...> f, A&&... a)
{ log<Level::info    >(f, std::forward<A>(a)...); }

template<class... A>
constexpr void warn    (std::format_string<A...> f, A&&... a)
{ log<Level::warn    >(f, std::forward<A>(a)...); }

template<class... A>
constexpr void error   (std::format_string<A...> f, A&&... a)
{ log<Level::error   >(f, std::forward<A>(a)...); }

template<class... A>
constexpr void critical(std::format_string<A...> f, A&&... a)
{ log<Level::critical>(f, std::forward<A>(a)...); }

} // namespace core::log
