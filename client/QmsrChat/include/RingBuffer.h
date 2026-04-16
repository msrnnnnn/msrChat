#ifndef RINGBUFFER_H
#define RINGBUFFER_H

#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>

class RingBuffer
{
public:
    static constexpr std::size_t kDefaultCapacity = 64 * 1024;
    static constexpr std::size_t kMaxCapacity = 4 * 1024 * 1024;

    explicit RingBuffer(std::size_t capacity = kDefaultCapacity)
        : _capacity(capacity),
          _buffer(new char[capacity]),
          _read_pos(0),
          _write_pos(0)
    {
    }

    ~RingBuffer()
    {
        delete[] _buffer;
    }

    RingBuffer(const RingBuffer &) = delete;
    RingBuffer &operator=(const RingBuffer &) = delete;

    RingBuffer(RingBuffer &&other) noexcept
        : _capacity(other._capacity),
          _buffer(other._buffer),
          _read_pos(other._read_pos),
          _write_pos(other._write_pos)
    {
        other._buffer = nullptr;
        other._capacity = 0;
        other._read_pos = 0;
        other._write_pos = 0;
    }

    RingBuffer &operator=(RingBuffer &&) = delete;

    bool Write(const char *data, std::size_t len)
    {
        if (len == 0)
        {
            return true;
        }

        if (len > kMaxCapacity)
        {
            return false;
        }

        while (len > GetSpaceAvailable(_write_pos))
        {
            if (!Expand())
            {
                return false;
            }
        }

        std::size_t write_pos = _write_pos;
        std::size_t available = GetSpaceAvailable(write_pos);

        if (available >= len)
        {
            CopyToBuffer(data, len, write_pos);
            _write_pos = CalculateNextPos(write_pos, len);
        }
        else
        {
            std::size_t to_end = _capacity - write_pos;
            if (len <= to_end)
            {
                std::memcpy(_buffer + write_pos, data, len);
                _write_pos = (write_pos + len) % _capacity;
            }
            else
            {
                std::memcpy(_buffer + write_pos, data, to_end);
                std::memcpy(_buffer, data + to_end, len - to_end);
                _write_pos = len - to_end;
            }
        }

        return true;
    }

    bool Reserve(std::size_t capacity)
    {
        if (capacity <= _capacity)
        {
            return true;
        }

        std::size_t new_capacity = _capacity;
        while (new_capacity < capacity && new_capacity < kMaxCapacity)
        {
            new_capacity *= 2;
        }

        if (new_capacity > kMaxCapacity)
        {
            new_capacity = kMaxCapacity;
        }

        if (capacity > new_capacity)
        {
            return false;
        }

        char *new_buffer = new char[new_capacity];
        std::size_t available = Available();
        Read(new_buffer, available);

        delete[] _buffer;
        _buffer = new_buffer;
        _capacity = new_capacity;
        _read_pos = 0;
        _write_pos = available;

        return true;
    }

    std::size_t SpaceRemaining() const
    {
        return GetSpaceAvailable(_write_pos);
    }

    bool Peek(std::size_t offset, char *dest, std::size_t len) const
    {
        std::size_t current_read = _read_pos;
        std::size_t current_write = _write_pos;
        std::size_t available = GetDataAvailable(current_read, current_write);

        if (offset + len > available)
        {
            return false;
        }

        std::size_t read_pos = (current_read + offset) % _capacity;

        if (read_pos + len <= _capacity)
        {
            std::memcpy(dest, _buffer + read_pos, len);
        }
        else
        {
            std::size_t to_end = _capacity - read_pos;
            std::memcpy(dest, _buffer + read_pos, to_end);
            std::memcpy(dest + to_end, _buffer, len - to_end);
        }

        return true;
    }

    std::size_t Read(char *dest, std::size_t len)
    {
        std::size_t current_read = _read_pos;
        std::size_t current_write = _write_pos;
        std::size_t available = GetDataAvailable(current_read, current_write);

        if (available == 0 || len == 0)
        {
            return 0;
        }

        std::size_t to_read = (std::min)(len, available);

        if (current_read + to_read <= _capacity)
        {
            std::memcpy(dest, _buffer + current_read, to_read);
            _read_pos = (current_read + to_read) % _capacity;
        }
        else
        {
            std::size_t to_end = _capacity - current_read;
            std::memcpy(dest, _buffer + current_read, to_end);
            std::memcpy(dest + to_end, _buffer, to_read - to_end);
            _read_pos = to_read - to_end;
        }

        return to_read;
    }

    void Consume(std::size_t len)
    {
        std::size_t current_read = _read_pos;
        std::size_t current_write = _write_pos;
        std::size_t available = GetDataAvailable(current_read, current_write);

        if (len > available)
        {
            len = available;
        }

        if (len > 0)
        {
            _read_pos = (current_read + len) % _capacity;
        }
    }

    std::size_t Available() const
    {
        return GetDataAvailable(_read_pos, _write_pos);
    }

    std::size_t Capacity() const
    {
        return _capacity;
    }

    void Clear()
    {
        _read_pos = 0;
        _write_pos = 0;
    }

private:
    std::size_t GetDataAvailable(std::size_t read_pos, std::size_t write_pos) const
    {
        if (write_pos >= read_pos)
        {
            return write_pos - read_pos;
        }
        return _capacity - read_pos + write_pos;
    }

    std::size_t GetSpaceAvailable(std::size_t write_pos) const
    {
        std::size_t current_read = _read_pos;
        if (write_pos >= current_read)
        {
            return _capacity - (write_pos - current_read) - 1;
        }
        return current_read - write_pos - 1;
    }

    bool Expand()
    {
        if (_capacity >= kMaxCapacity)
        {
            return false;
        }

        std::size_t new_capacity = _capacity * 2;
        if (new_capacity > kMaxCapacity)
        {
            new_capacity = kMaxCapacity;
        }

        char *new_buffer = new char[new_capacity];
        std::size_t available = Available();
        Read(new_buffer, available);

        delete[] _buffer;
        _buffer = new_buffer;
        _capacity = new_capacity;
        _read_pos = 0;
        _write_pos = available;

        return true;
    }

    std::size_t CalculateNextPos(std::size_t pos, std::size_t len) const
    {
        return (pos + len) % _capacity;
    }

    void CopyToBuffer(const char *data, std::size_t len, std::size_t write_pos)
    {
        if (write_pos + len <= _capacity)
        {
            std::memcpy(_buffer + write_pos, data, len);
        }
        else
        {
            std::size_t to_end = _capacity - write_pos;
            std::memcpy(_buffer + write_pos, data, to_end);
            std::memcpy(_buffer, data + to_end, len - to_end);
        }
    }

    std::size_t _capacity;
    char *_buffer;
    std::size_t _read_pos;
    std::size_t _write_pos;
};

#endif
