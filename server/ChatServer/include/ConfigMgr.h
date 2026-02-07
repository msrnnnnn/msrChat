#pragma once
#include <map>
#include <string>

struct SectionInfo
{
    SectionInfo() = default;
    ~SectionInfo() {}

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

class ConfigMgr
{
public:
    static ConfigMgr &GetInstance()
    {
        static ConfigMgr instance;
        return instance;
    }

    // Short alias for GetInstance() as used in user's snippet
    static ConfigMgr &Inst() {
        return GetInstance();
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
    ConfigMgr();
    std::map<std::string, SectionInfo> _config_map;
};
