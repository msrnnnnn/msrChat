#ifndef BASE_PROTOCOL_H
#define BASE_PROTOCOL_H

#include <cstdint>
#include <memory>
#include <vector>

class BaseProtocol
{
public:
    virtual ~BaseProtocol() = default;
    
    virtual bool Encode(uint16_t msg_id, const std::vector<char>& data, std::vector<char>& out_buffer) = 0;
    virtual bool Decode(std::vector<char>& buffer, uint16_t& msg_id, std::vector<char>& out_data) = 0;
    
    virtual int32_t GetHeadLen() const = 0;
};

class MsgNode
{
public:
    MsgNode(uint16_t msg_id, const char* data, uint32_t len)
        : _msg_id(msg_id), _data(data), _len(len) {}
    
    uint16_t _msg_id;
    const char* _data;
    uint32_t _len;
};

#endif
