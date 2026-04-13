#ifndef TLV_PROTOCOL_H
#define TLV_PROTOCOL_H

#include "BaseProtocol.h"
#include <cstring>
#include <arpa/inet.h>

class TLVProtocol : public BaseProtocol
{
public:
    static constexpr uint32_t HEAD_LEN = 8;
    static constexpr uint32_t MAX_MSG_LEN = 1024 * 1024 * 10;
    
    TLVProtocol() = default;
    ~TLVProtocol() override = default;
    
    bool Encode(uint16_t msg_id, const std::vector<char>& data, std::vector<char>& out_buffer) override;
    bool Decode(std::vector<char>& buffer, uint16_t& msg_id, std::vector<char>& out_data) override;
    
    int32_t GetHeadLen() const override { return HEAD_LEN; }
    
private:
    bool ParseHead(const char* data, uint32_t len, uint16_t& msg_id, uint32_t& data_len);
};

#endif
