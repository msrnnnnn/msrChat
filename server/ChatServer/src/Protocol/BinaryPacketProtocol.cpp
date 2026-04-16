#include "Protocol/BinaryPacketProtocol.h"
#include <string>
#include <string_view>
#include <vector>

bool BinaryPacketProtocol::Encode(
    uint16_t msg_id, const std::string &json_data, const std::vector<char> &binary_data, std::vector<char> &out_buffer)
{
    uint32_t json_len = json_data.size();
    uint32_t binary_len = binary_data.size();
    uint32_t total_len = json_len + binary_len;

    if (total_len > MAX_MSG_LEN)
    {
        return false;
    }

    out_buffer.resize(HEAD_LEN + total_len);

    uint16_t net_msg_id = htons(msg_id);
    uint32_t net_total_len = htonl(total_len);
    uint32_t net_json_len = htonl(json_len);

    std::memcpy(out_buffer.data(), &net_msg_id, sizeof(uint16_t));
    std::memcpy(out_buffer.data() + 2, &net_total_len, sizeof(uint32_t));
    std::memcpy(out_buffer.data() + 6, &net_json_len, sizeof(uint32_t));

    if (json_len > 0)
    {
        std::memcpy(out_buffer.data() + HEAD_LEN, json_data.data(), json_len);
    }

    if (binary_len > 0)
    {
        std::memcpy(out_buffer.data() + HEAD_LEN + json_len, binary_data.data(), binary_len);
    }

    return true;
}

bool BinaryPacketProtocol::Encode(
    uint16_t msg_id, const std::vector<char> &json_data, const std::vector<char> &binary_data,
    std::vector<char> &out_buffer)
{
    return Encode(msg_id, std::string(json_data.begin(), json_data.end()), binary_data, out_buffer);
}

bool BinaryPacketProtocol::Decode(
    std::vector<char> &buffer, uint16_t &msg_id, std::string &out_json, std::vector<char> &out_binary)
{
    if (buffer.size() < HEAD_LEN)
    {
        return false;
    }

    uint16_t net_msg_id;
    uint32_t net_total_len;
    uint32_t net_json_len;

    std::memcpy(&net_msg_id, buffer.data(), sizeof(uint16_t));
    std::memcpy(&net_total_len, buffer.data() + 2, sizeof(uint32_t));
    std::memcpy(&net_json_len, buffer.data() + 6, sizeof(uint32_t));

    msg_id = ntohs(net_msg_id);
    uint32_t total_len = ntohl(net_total_len);
    uint32_t json_len = ntohl(net_json_len);

    if (total_len > MAX_MSG_LEN)
    {
        buffer.clear();
        return false;
    }

    uint32_t full_len = HEAD_LEN + total_len;
    if (buffer.size() < full_len)
    {
        return false;
    }

    if (json_len > 0)
    {
        out_json.assign(buffer.data() + HEAD_LEN, buffer.data() + HEAD_LEN + json_len);
    }
    else
    {
        out_json.clear();
    }

    uint32_t binary_len = total_len - json_len;
    if (binary_len > 0)
    {
        out_binary.resize(binary_len);
        std::memcpy(out_binary.data(), buffer.data() + HEAD_LEN + json_len, binary_len);
    }
    else
    {
        out_binary.clear();
    }

    buffer.erase(buffer.begin(), buffer.begin() + full_len);
    return true;
}

bool BinaryPacketProtocol::Decode(
    std::vector<char> &buffer, uint16_t &msg_id, std::string_view &out_json_view, std::string_view &out_binary_view)
{
    if (buffer.size() < HEAD_LEN)
    {
        return false;
    }

    uint16_t net_msg_id;
    uint32_t net_total_len;
    uint32_t net_json_len;

    std::memcpy(&net_msg_id, buffer.data(), sizeof(uint16_t));
    std::memcpy(&net_total_len, buffer.data() + 2, sizeof(uint32_t));
    std::memcpy(&net_json_len, buffer.data() + 6, sizeof(uint32_t));

    msg_id = ntohs(net_msg_id);
    uint32_t total_len = ntohl(net_total_len);
    uint32_t json_len = ntohl(net_json_len);

    if (total_len > MAX_MSG_LEN)
    {
        buffer.clear();
        return false;
    }

    uint32_t full_len = HEAD_LEN + total_len;
    if (buffer.size() < full_len)
    {
        return false;
    }

    if (json_len > 0)
    {
        out_json_view = std::string_view(buffer.data() + HEAD_LEN, json_len);
    }
    else
    {
        out_json_view = std::string_view();
    }

    uint32_t binary_len = total_len - json_len;
    if (binary_len > 0)
    {
        out_binary_view = std::string_view(buffer.data() + HEAD_LEN + json_len, binary_len);
    }
    else
    {
        out_binary_view = std::string_view();
    }

    buffer.erase(buffer.begin(), buffer.begin() + full_len);
    return true;
}

bool BinaryPacketProtocol::ParseHead(
    const char *data, uint32_t len, uint16_t &msg_id, uint32_t &total_len, uint32_t &json_len)
{
    if (len < HEAD_LEN)
    {
        return false;
    }

    uint16_t net_msg_id;
    uint32_t net_total_len;
    uint32_t net_json_len;

    std::memcpy(&net_msg_id, data, sizeof(uint16_t));
    std::memcpy(&net_total_len, data + 2, sizeof(uint32_t));
    std::memcpy(&net_json_len, data + 6, sizeof(uint32_t));

    msg_id = ntohs(net_msg_id);
    total_len = ntohl(net_total_len);
    json_len = ntohl(net_json_len);

    if (total_len > MAX_MSG_LEN)
    {
        return false;
    }

    return true;
}
