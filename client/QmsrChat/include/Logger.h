#ifndef LOGGER_H
#define LOGGER_H

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <memory>
#include <mutex>

class Logger {
public:
    static Logger& getInstance();

    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    spdlog::logger* getLogger() const { return m_logger.get(); }

    void setLevel(spdlog::level::level_enum level);
    void flush();

private:
    Logger();
    ~Logger();

    void init();

    std::shared_ptr<spdlog::logger> m_logger;
    std::once_flag m_initFlag;
};

#endif // LOGGER_H
