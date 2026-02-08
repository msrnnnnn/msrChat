#include "ConfigMgr.h"
#include <boost/property_tree/ini_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <filesystem>
#include <spdlog/spdlog.h>

ConfigMgr::ConfigMgr()
{
    std::filesystem::path current_path = std::filesystem::current_path();
    std::filesystem::path config_path = current_path / "config.ini";

    if (!std::filesystem::exists(config_path))
    {
        config_path = "/home/msr/msrChat/server/ChatServer/config.ini";
    }
    
    // Windows fallback for current environment
    if (!std::filesystem::exists(config_path))
    {
        config_path = "E:/Study/Project/Chat/msrchat/server/ChatServer/config.ini";
    }

    if (!std::filesystem::exists(config_path)) {
        spdlog::critical("Config.ini NOT FOUND at {}! Server cannot start.", config_path.string());
        throw std::runtime_error("Config not found");
    }

    spdlog::info("Loading Config from: {}", config_path.string());

    boost::property_tree::ptree pt;
    try
    {
        boost::property_tree::read_ini(config_path.string(), pt);
    }
    catch (std::exception &e)
    {
        spdlog::error("Config load failed: {}", e.what());
        return;
    }

    for (const auto &section_pair : pt)
    {
        const std::string &section_name = section_pair.first;
        const boost::property_tree::ptree &section_tree = section_pair.second;

        std::map<std::string, std::string> section_config;
        for (const auto &key_value_pair : section_tree)
        {
            const std::string &key = key_value_pair.first;
            const std::string &value = key_value_pair.second.get_value<std::string>();
            section_config[key] = value;
        }
        SectionInfo sectionInfo;
        sectionInfo._section_datas = section_config;
        _config_map[section_name] = sectionInfo;
    }
}
