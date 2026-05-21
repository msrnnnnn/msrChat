#ifndef PACKET_NODE_H
#define PACKET_NODE_H

#include "ObjectPool.h"
#include "const.h"
#include <boost/asio.hpp>
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

class RecvNode
{
public:
    uint16_t _msg_id;
    uint32_t _total_len;
    char *_data;
    std::vector<char> _buffer;

    RecvNode()
        : _msg_id(0),
          _total_len(0),
          _data(nullptr),
          _buffer(HEAD_TOTAL_LEN + 1)
    {
        _data = _buffer.data();
    }

    void SetPool(ObjectPool<RecvNode> *pool) noexcept
    {
        _pool = pool;
    }

    void SetPool(std::nullptr_t) noexcept
    {
        _pool = nullptr;
    }

    void Reset() noexcept
    {
        _msg_id = 0;
        _total_len = 0;
        _data = _buffer.data();
        if (!_buffer.empty())
        {
            _buffer[0] = '\0';
        }
    }

    void Reset(uint32_t max_len, uint16_t msg_id)
    {
        _msg_id = msg_id;
        _total_len = max_len;
        if (_buffer.size() < static_cast<std::size_t>(_total_len) + 1)
        {
            _buffer.resize(static_cast<std::size_t>(_total_len) + 1);
        }
        _data = _buffer.data();
        _data[_total_len] = '\0';
    }

    void Clear() noexcept
    {
        if (!_buffer.empty())
        {
            ::memset(_buffer.data(), 0, _buffer.size());
            _data = _buffer.data();
        }
    }

private:
    ObjectPool<RecvNode> *_pool = nullptr;
};

class SendNode
{
public:
    uint16_t _msg_id;
    uint32_t _total_len;
    char *_data;
    std::vector<char> _buffer;

    SendNode()
        : _msg_id(0),
          _total_len(0),
          _data(nullptr),
          _buffer(HEAD_TOTAL_LEN)
    {
        _data = _buffer.data();
    }

    void SetPool(ObjectPool<SendNode> *pool) noexcept
    {
        _pool = pool;
    }

    void SetPool(std::nullptr_t) noexcept
    {
        _pool = nullptr;
    }

    void Reset() noexcept
    {
        _msg_id = 0;
        _total_len = 0;
        _data = _buffer.data();
    }

    void Reset(const std::string &msg, uint16_t msg_id)
    {
        _msg_id = msg_id;
        _total_len = static_cast<uint32_t>(msg.length());
        if (_buffer.size() < static_cast<std::size_t>(_total_len) + HEAD_TOTAL_LEN)
        {
            _buffer.resize(static_cast<std::size_t>(_total_len) + HEAD_TOTAL_LEN);
        }
        _data = _buffer.data();
        uint16_t net_msg_id = boost::asio::detail::socket_ops::host_to_network_short(msg_id);
        memcpy(_data, &net_msg_id, 2);
        uint32_t net_len =
            boost::asio::detail::socket_ops::host_to_network_long(static_cast<unsigned long>(_total_len));
        memcpy(_data + 2, &net_len, 4);
        if (_total_len > 0)
        {
            memcpy(_data + 6, msg.data(), _total_len);
        }
    }

private:
    ObjectPool<SendNode> *_pool = nullptr;
};

#endif
