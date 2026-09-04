#pragma once

#include <iostream>
#include <string>
#include <mutex>
#include <sstream>
#include <chrono>
#include <iomanip>

#ifdef ERROR
#undef ERROR
#endif

namespace pastebin::util {

enum class LogLevel {
    DEBUG_LVL,
    INFO_LVL,
    WARN_LVL,
    ERROR_LVL
};

class Logger {
public:
    static Logger& instance();

    void set_level(LogLevel level);
    LogLevel get_level() const;

    void log(LogLevel level, const std::string& message);
    void debug(const std::string& message);
    void info(const std::string& message);
    void warn(const std::string& message);
    void error(const std::string& message);

private:
    Logger() = default;
    LogLevel current_level_{LogLevel::INFO_LVL};
    std::mutex mutex_;

    static const char* level_to_string(LogLevel level);
    static std::string current_timestamp();
};

#define LOG_DEBUG(msg) ::pastebin::util::Logger::instance().debug(msg)
#define LOG_INFO(msg)  ::pastebin::util::Logger::instance().info(msg)
#define LOG_WARN(msg)  ::pastebin::util::Logger::instance().warn(msg)
#define LOG_ERROR(msg) ::pastebin::util::Logger::instance().error(msg)

} // namespace pastebin::util
