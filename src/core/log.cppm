module;

#include <chrono>
#include <format>
#include <print>
#include <source_location>
#include <string_view>
#include <functional>

export module core.log;

export namespace core::log {

enum class Level : uint8_t { trace, debug, info, warn, error, critical, off };

constexpr std::string_view to_string(const Level level) noexcept {
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

// Compile-time switch; set to Level::info (etc.) via a compile definition
#ifndef ATTO_LOG_LEVEL
    inline constexpr auto compile_time_level = Level::info;
#else
    inline constexpr Level compile_time_level = static_cast<Level>(ATTO_LOG_LEVEL);
#endif

// Sink: defaults to std::print with stderr, but can be replaced later by a log file later
inline std::function<void(std::string_view)> sink = [](std::string_view s) { std::print(stderr, "{}", s); };

template<Level level, typename... Args>
constexpr void log(
        const std::string_view fmt,
        Args&&... args,
        const std::source_location loc = std::source_location::current()
) {
    if constexpr (level < compile_time_level) { return; }   // compiled out

    using clock         = std::chrono::steady_clock;
    const auto now      = clock::now().time_since_epoch();
    const auto millis   = std::chrono::duration_cast<std::chrono::milliseconds>(now).count();

    auto header = std::format(
            "[{:>10} ms] [{:^7}] {:>20}:{:<3} ",
            millis,
            to_string(level),
            loc.file_name(), loc.line()
    );

    auto body = std::vformat(fmt, std::make_format_args(args...));
    sink(std::format("{}{}\n", header, body));
}

template<typename... A>
constexpr void trace(std::string_view f, A&&... a, std::source_location loc = std::source_location::current()) {
    log<Level::trace>(f, std::forward<A>(a)..., loc);
}

template<typename... A>
constexpr void debug(std::string_view f, A&&... a, std::source_location loc = std::source_location::current()) {
    log<Level::debug>(f, std::forward<A>(a)..., loc);
}

template<typename... A>
constexpr void info(std::string_view f, A&&... a, std::source_location loc = std::source_location::current()) {
    log<Level::info>(f, std::forward<A>(a)..., loc);
}

template<typename... A>
constexpr void warn(std::string_view f, A&&... a, std::source_location loc = std::source_location::current()) {
    log<Level::warn>(f, std::forward<A>(a)..., loc);
}

template<typename... A>
constexpr void error(std::string_view f, A&&... a, std::source_location loc = std::source_location::current()) {
    log<Level::error>(f, std::forward<A>(a)..., loc);
}

template<typename... A>
constexpr void critical(std::string_view f, A&&... a, std::source_location loc = std::source_location::current()) {
    log<Level::critical>(f, std::forward<A>(a)..., loc);
}

} // namespace core::log
