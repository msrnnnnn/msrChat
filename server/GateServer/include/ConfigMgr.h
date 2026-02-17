/**
 * @file    ConfigMgr.h
 * @brief   配置管理类
 * @details 负责读取 INI 配置文件并提供配置项查询功能。
 * @author  msr
 */

#pragma once

#include <map>
#include <string>

/**
 * @struct  SectionInfo
 * @brief   配置段信息结构体
 * @details 存储 INI 文件中一个 Section 下的所有 Key-Value 对。
 */
struct SectionInfo
{
    SectionInfo() = default;
    ~SectionInfo()
    {
        _section_datas.clear();
    }

    SectionInfo(const SectionInfo &src)
    {
        _section_datas = src._section_datas;
    }

    SectionInfo &operator=(const SectionInfo &src)
    {
        if (&src == this)
        {
            return *this;
        }
        this->_section_datas = src._section_datas;
        return *this;
    }

    std::map<std::string, std::string> _section_datas;

    std::string operator[](const std::string &key)
    {
        if (_section_datas.find(key) == _section_datas.end())
        {
            return "";
        }
        return _section_datas[key];
    }
};

/**
 * @class   ConfigMgr
 * @brief   配置管理器 (单例)
 * @details 读取 config.ini 并缓存所有配置项。
 */
class ConfigMgr
{
public:
    /**
     * @brief 获取单例实例
     * @return ConfigMgr& 单例引用
     */
    static ConfigMgr &GetInstance()
    {
        static ConfigMgr instance;
        return instance;
    }

    /**
     * @brief 获取指定 Section 的配置信息
     * @param section Section 名称
     * @return SectionInfo 配置段信息
     */
    SectionInfo operator[](const std::string &section)
    {
        if (_config_map.find(section) == _config_map.end())
        {
            return SectionInfo();
        }
        return _config_map[section];
    }

private:
    /**
     * @brief 私有构造函数 (加载配置)
     */
    ConfigMgr();

    std::map<std::string, SectionInfo> _config_map; ///< 配置映射表
};
