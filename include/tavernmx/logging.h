#pragma once
#include <spdlog/spdlog.h>
#include <spdlog/fmt/fmt.h>


// TMX_INFO(fmt, args...) - Shorthand for info-level log, only enabled in debug builds
#ifndef NDEBUG
#define TMX_INFO(...) ::tavernmx::log_info(__VA_ARGS__)
#else
#define TMX_INFO(...)
#endif

// TMX_WARN(fmt, args...) - Shorthand for warn-level log
#define TMX_WARN(...) ::tavernmx::log_warn(__VA_ARGS__)

// TMX_ERR(fmt, args...) - Shorthand for error-level log
#define TMX_ERR(...) ::tavernmx::log_error(__VA_ARGS__)

namespace tavernmx {
    void configure_logging(spdlog::level::level_enum level, const std::optional<std::string>& log_file);

    std::vector<std::shared_ptr<spdlog::logger>>& get_loggers();

    /**
    * @brief Log a message with INFO level. Does nothing if a logger has not been configured.
    * Prefer the TMX_INFO() macro since it is removed in release builds.
    * @param msg is the message
    */
    inline void log_info(const char* msg) {
        for (auto& logger: get_loggers()) {
            logger->info(msg);
        }
    }

    /**
    * @brief Log a message with INFO level. Does nothing if a logger has not been configured.
    * Prefer the TMX_INFO() macro since it is removed in release builds.
    * @param fmt is the message
    * @param args are one or more arguments to interpolate into \p fmt
    */
    template<typename... Args>
    void log_info(fmt::format_string<Args...> fmt, Args&&... args) {
        for (auto& logger: get_loggers()) {
            logger->info(std::forward<fmt::format_string<Args...>>(fmt), std::forward<Args>(args)...);
        }
    }

    /**
    * @brief Log a message with WARN level. Does nothing if a logger has not been configured.
    * Prefer the TMX_WARN() macro.
    * @param msg is the message
    */
    inline void log_warn(const char* msg) {
        for (auto& logger: get_loggers()) {
            logger->warn(msg);
        }
    }

    /**
    * @brief Log a message with WARN level. Does nothing if a logger has not been configured.
    * Prefer the TMX_WARN() macro.
    * @param fmt is the message
    * @param args are one or more arguments to interpolate into \p fmt
    */
    template<typename... Args>
    void log_warn(fmt::format_string<Args...> fmt, Args&&... args) {
        for (auto& logger: get_loggers()) {
            logger->warn(std::forward<fmt::format_string<Args...>>(fmt), std::forward<Args>(args)...);
        }
    }

    /**
    * @brief Log a message with ERROR level. Does nothing if a logger has not been configured.
    * Prefer the TMX_ERR() macro.
    * @param msg is the message
    */
    inline void log_error(const char* msg) {
        for (auto& logger: get_loggers()) {
            logger->error(msg);
        }
    }

    /**
    * @brief Log a message with ERROR level. Does nothing if a logger has not been configured.
    * Prefer the TMX_ERR() macro.
    * @param fmt is the message
    * @param args are one or more arguments to interpolate into \p fmt
    */
    template<typename... Args>
    void log_error(fmt::format_string<Args...> fmt, Args&&... args) {
        for (auto& logger: get_loggers()) {
            logger->error(std::forward<fmt::format_string<Args...>>(fmt), std::forward<Args>(args)...);
        }
    }
}
