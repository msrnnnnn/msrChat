#ifndef BINARY_PACKET_PROTOCOL_H
#define BINARY_PACKET_PROTOCOL_H

#include "BaseProtocol.h"
#include <arpa/inet.h>
#include <cstring>
#include <string_view>

class BinaryPacketProtocol : public BaseProtocol
{
public:
    static constexpr uint32_t HEAD_LEN = 10;
    static constexpr uint32_t MAX_MSG_LEN = 1024 * 1024 * 10;

    BinaryPacketProtocol() = default;
    ~BinaryPacketProtocol() override = default;

    bool Encode(
        uint16_t msg_id, const std::string &json_data, const std::vector<char> &binary_data,
        std::vector<char> &out_buffer);
    bool Encode(
        uint16_t msg_id, const std::vector<char> &json_data, const std::vector<char> &binary_data,
        std::vector<char> &out_buffer);
    bool Decode(std::vector<char> &buffer, uint16_t &msg_id, std::string &out_json, std::vector<char> &out_binary);
    bool Decode(std::vector<char> &buffer, uint16_t &msg_id, std::string_view &out_json_view, std::string_view &out_binary_view);

    int32_t GetHeadLen() const override
    {
        return HEAD_LEN;
    }

private:
    bool ParseHead(const char *data, uint32_t len, uint16_t &msg_id, uint32_t &total_len, uint32_t &json_len);
};

#endif
