#ifndef FILE_DESCRIPTOR_H
#define FILE_DESCRIPTOR_H

#ifdef __unix__
#include <fcntl.h>
#include <unistd.h>
#endif

class FileDescriptor
{
public:
    explicit FileDescriptor(int fd = -1)
        : _fd(fd)
    {
    }

    FileDescriptor(const FileDescriptor &) = delete;
    FileDescriptor &operator=(const FileDescriptor &) = delete;

    FileDescriptor(FileDescriptor &&other) noexcept
        : _fd(other._fd)
    {
        other._fd = -1;
    }

    FileDescriptor &operator=(FileDescriptor &&other) noexcept
    {
        if (this != &other)
        {
            if (_fd >= 0)
            {
                close(_fd);
            }
            _fd = other._fd;
            other._fd = -1;
        }
        return *this;
    }

    ~FileDescriptor()
    {
        if (_fd >= 0)
        {
            close(_fd);
            _fd = -1;
        }
    }

    int Get() const
    {
        return _fd;
    }

    int Release()
    {
        int fd = _fd;
        _fd = -1;
        return fd;
    }

    FileDescriptor &operator=(int fd)
    {
        if (_fd >= 0)
        {
            close(_fd);
        }
        _fd = fd;
        return *this;
    }

    void Reset(int fd = -1)
    {
        if (_fd >= 0)
        {
            close(_fd);
        }
        _fd = fd;
    }

    bool IsValid() const
    {
        return _fd >= 0;
    }

    explicit operator bool() const
    {
        return _fd >= 0;
    }

    operator int() const
    {
        return _fd;
    }

private:
    int _fd;
};

#endif
