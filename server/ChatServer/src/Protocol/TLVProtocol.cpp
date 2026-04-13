#include "Protocol/TLVProtocol.h"
#include <cstring>

bool TLVProtocol::Encode(uint16_t msg_id, const std::vector<char>& data, std::vector<char>& out_buffer)
{
    uint32_t data_len = data.size();
    if (data_len > MAX_MSG_LEN) {
        return false;
    }
    
    out_buffer.resize(HEAD_LEN + data_len);
    
    uint16_t net_msg_id = htons(msg_id);
    uint32_t net_data_len = htonl(data_len);
    
    std::memcpy(out_buffer.data(), &net_msg_id, sizeof(uint16_t));
    std::memcpy(out_buffer.data() + 2, &net_data_len, sizeof(uint32_t));
    
    if (data_len > 0 && !data.empty()) {
        std::memcpy(out_buffer.data() + HEAD_LEN, data.data(), data_len);
    }
    
    return true;
}

bool TLVProtocol::Decode(std::vector<char>& buffer, uint16_t& msg_id, std::vector<char>& out_data)
{
    if (buffer.size() < HEAD_LEN) {
        return false;
    }
    
    uint16_t net_msg_id;
    uint32_t net_data_len;
    
    std::memcpy(&net_msg_id, buffer.data(), sizeof(uint16_t));
    std::memcpy(&net_data_len, buffer.data() + 2, sizeof(uint32_t));
    
    msg_id = ntohs(net_msg_id);
    uint32_t data_len = ntohl(net_data_len);
    
    if (data_len > MAX_MSG_LEN) {
        buffer.clear();
        return false;
    }
    
    uint32_t total_len = HEAD_LEN + data_len;
    if (buffer.size() < total_len) {
        return false;
    }
    
    if (data_len > 0) {
        out_data.resize(data_len);
        std::memcpy(out_data.data(), buffer.data() + HEAD_LEN, data_len);
    }
    
    buffer.erase(buffer.begin(), buffer.begin() + total_len);
    return true;
}

bool TLVProtocol::ParseHead(const char* data, uint32_t len, uint16_t& msg_id, uint32_t& data_len)
{
    if (len < HEAD_LEN) {
        return false;
    }
    
    uint16_t net_msg_id;
    uint32_t net_data_len;
    
    std::memcpy(&net_msg_id, data, sizeof(uint16_t));
    std::memcpy(&net_data_len, data + 2, sizeof(uint32_t));
    
    msg_id = ntohs(net_msg_id);
    data_len = ntohl(net_data_len);
    
    if (data_len > MAX_MSG_LEN) {
        return false;
    }
    
    return true;
}
