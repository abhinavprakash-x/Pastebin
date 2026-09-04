#include "logger.hpp"

namespace pastebin::util {

Logger& Logger::instance() {
    static Logger inst;
    return inst;
}

void Logger::set_level(LogLevel level) {
    std::lock_guard<std::mutex> lock(mutex_);
    current_level_ = level;
}

LogLevel Logger::get_level() const {
    return current_level_;
}

void Logger::log(LogLevel level, const std::string& message) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (static_cast<int>(level) < static_cast<int>(current_level_)) {
        return;
    }

    std::ostream& out = (level == LogLevel::ERROR_LVL) ? std::cerr : std::cout;
    out << "[" << current_timestamp() << "] ["
        << level_to_string(level) << "] "
        << message << std::endl;
}

void Logger::debug(const std::string& message) {
    log(LogLevel::DEBUG_LVL, message);
}

void Logger::info(const std::string& message) {
    log(LogLevel::INFO_LVL, message);
}

void Logger::warn(const std::string& message) {
    log(LogLevel::WARN_LVL, message);
}

void Logger::error(const std::string& message) {
    log(LogLevel::ERROR_LVL, message);
}

const char* Logger::level_to_string(LogLevel level) {
    switch (level) {
        case LogLevel::DEBUG_LVL: return "DEBUG";
        case LogLevel::INFO_LVL:  return "INFO ";
        case LogLevel::WARN_LVL:  return "WARN ";
        case LogLevel::ERROR_LVL: return "ERROR";
        default:                  return "UNKN ";
    }
}

std::string Logger::current_timestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

    std::tm tm_buf{};
#if defined(_WIN32) || defined(_WIN64)
    localtime_s(&tm_buf, &time_t_now);
#else
    localtime_r(&time_t_now, &tm_buf);
#endif

    std::ostringstream ss;
    ss << std::put_time(&tm_buf, "%Y-%m-%d %H:%M:%S")
       << '.' << std::setfill('0') << std::setw(3) << ms.count();
    return ss.str();
}

} // namespace pastebin::util
