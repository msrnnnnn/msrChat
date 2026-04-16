#include "Logger.h"
#include <filesystem>
#include <iostream>

Logger::Logger()
{
    init();
}

Logger::~Logger()
{
    if (m_logger)
    {
        m_logger->flush();
    }
}

void Logger::init()
{
    std::call_once(
        m_initFlag,
        [this]()
        {
            try
            {
                std::vector<spdlog::sink_ptr> sinks;

                auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
                console_sink->set_level(spdlog::level::trace);
                console_sink->set_pattern("%^[%Y-%m-%d %H:%M:%S.%e] [%l] [%n] [%t] [%s:%#] %v%$");
                sinks.push_back(console_sink);

                namespace fs = std::filesystem;
                fs::path log_dir = fs::current_path() / "logs";
                if (!fs::exists(log_dir))
                {
                    fs::create_directories(log_dir);
                }

                auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
                    (log_dir / "app.log").string(), 10 * 1024 * 1024, 3);
                file_sink->set_level(spdlog::level::debug);
                file_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] [%t] [%s:%#] %v");
                sinks.push_back(file_sink);

                m_logger = std::make_shared<spdlog::logger>("QmsrChat", begin(sinks), end(sinks));
                m_logger->set_level(spdlog::level::trace);
                m_logger->flush_on(spdlog::level::warn);

                spdlog::register_logger(m_logger);
            }
            catch (const spdlog::spdlog_ex &ex)
            {
                std::cerr << "Logger initialization failed: " << ex.what() << std::endl;
            }
        });
}

Logger &Logger::getInstance()
{
    static Logger instance;
    return instance;
}

void Logger::setLevel(spdlog::level::level_enum level)
{
    if (m_logger)
    {
        m_logger->set_level(level);
    }
}

void Logger::flush()
{
    if (m_logger)
    {
        m_logger->flush();
    }
}
