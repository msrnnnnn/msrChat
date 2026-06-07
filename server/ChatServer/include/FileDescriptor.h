/**
 * @file FileDescriptor.h
 * @brief RAII 文件描述符封装
 * @details 自动管理文件描述符生命周期，支持移动语义，禁止拷贝
 */
#ifndef FILE_DESCRIPTOR_H
#define FILE_DESCRIPTOR_H

#ifdef __unix__
#include <fcntl.h>
#include <unistd.h>
#endif

/**
 * @class FileDescriptor
 * @brief 文件描述符 RAII 包装类
 * @details 构造时接管文件描述符所有权，析构时自动 close。
 *          仅支持移动语义，禁止拷贝，避免描述符重复关闭。
 */
class FileDescriptor
{
public:
    explicit FileDescriptor(int fd = -1)
        : _fd(fd)
    {
    }

    FileDescriptor(const FileDescriptor &) = delete;
    FileDescriptor &operator=(const FileDescriptor &) = delete;

    /**
     * @brief 移动构造，接管 other 的文件描述符所有权
     */
    FileDescriptor(FileDescriptor &&other) noexcept
        : _fd(other._fd)
    {
        other._fd = -1;
    }

    /**
     * @brief 移动赋值，先关闭当前描述符再接管 other 的所有权
     */
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

    /**
     * @brief 析构时自动关闭持有的文件描述符
     */
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

    /**
     * @brief 接管新的文件描述符（先关闭旧描述符）
     */
    FileDescriptor &operator=(int fd)
    {
        if (_fd >= 0)
        {
            close(_fd);
        }
        _fd = fd;
        return *this;
    }

    /**
     * @brief 接管新描述符，先关闭当前持有的描述符
     * @param fd 新的文件描述符，默认为 -1
     */
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
