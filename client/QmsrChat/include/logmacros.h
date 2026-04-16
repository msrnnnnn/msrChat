#ifndef LOGMACROS_H
#define LOGMACROS_H

#include "Logger.h"

#define LOG_TRACE(...) SPDLOG_LOGGER_CALL(Logger::getInstance().getLogger(), spdlog::level::trace, __VA_ARGS__)
#define LOG_DEBUG(...) SPDLOG_LOGGER_CALL(Logger::getInstance().getLogger(), spdlog::level::debug, __VA_ARGS__)
#define LOG_INFO(...)  SPDLOG_LOGGER_CALL(Logger::getInstance().getLogger(), spdlog::level::info, __VA_ARGS__)
#define LOG_WARN(...)  SPDLOG_LOGGER_CALL(Logger::getInstance().getLogger(), spdlog::level::warn, __VA_ARGS__)
#define LOG_ERROR(...) SPDLOG_LOGGER_CALL(Logger::getInstance().getLogger(), spdlog::level::err, __VA_ARGS__)
#define LOG_CRITICAL(...) SPDLOG_LOGGER_CALL(Logger::getInstance().getLogger(), spdlog::level::critical, __VA_ARGS__)

#endif // LOGMACROS_H
