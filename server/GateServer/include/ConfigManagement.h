#pragma once
#include <map>
#include <string>

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

    // [Fix] 修复原教程漏写 return *this 的严重 Bug
    SectionInfo &operator=(const SectionInfo &src)
    {
        if (&src == this)
            return *this;
        this->_section_datas = src._section_datas;
        return *this; // 必须返回引用！
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

class ConfigManagement
{
public:
    // 使用单例模式，而不是全局变量
    static ConfigManagement &GetInstance()
    {
        static ConfigManagement instance;
        return instance;
    }

    SectionInfo operator[](const std::string &section)
    {
        if (_config_map.find(section) == _config_map.end())
        {
            return SectionInfo();
        }
        return _config_map[section];
    }

private:
    ConfigManagement();
    std::map<std::string, SectionInfo> _config_map;
};