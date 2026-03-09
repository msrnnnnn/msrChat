/**
 * @file    ConfigMgr.cpp
 * @brief   配置管理器实现
 */
#include "ConfigMgr.h"
#include <boost/property_tree/ini_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <filesystem>
#include <iostream>

/**
 * @brief 构造函数
 * @details 自动查找并加载 config.ini 文件。
 */
ConfigMgr::ConfigMgr()
{
    // 获取当前工作目录
    std::filesystem::path current_path = std::filesystem::current_path();
    // 拼接配置文件路径
    std::filesystem::path config_path = current_path / "config.ini";

    if (!std::filesystem::exists(config_path))
    {
        // 如果当前目录找不到，尝试使用默认开发路径
        config_path = "e:/Study/Project/Chat/msrchat/server/GateServer/config.ini";
    }

    std::cout << "Loading Config from: " << config_path << std::endl;

    boost::property_tree::ptree pt;
    try
    {
        boost::property_tree::read_ini(config_path.string(), pt);
    }
    catch (std::exception &e)
    {
        std::cerr << "Config load failed: " << e.what() << std::endl;
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
