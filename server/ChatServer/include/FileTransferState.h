/**
 * @file FileTransferState.h
 * @brief 会话级文件接收状态
 * @details 存储在 CSession 中，记录当前正在接收的文件元信息和已接收数据。
 *          仅在会话生命周期内有效，非线程安全（由 CSession::_file_mutex 保护）。
 */
#ifndef FILE_TRANSFER_STATE_H
#define FILE_TRANSFER_STATE_H

#include "FileDescriptor.h"
#include <cstdint>
#include <string>
#include <vector>

/**
 * @struct FileTransferState
 * @brief 文件接收中间状态
 * @details 在一次文件传输过程中，暂存文件元数据与已接收的二进制数据。
 */
struct FileTransferState
{
    int64_t task_id = 0;
    int from_uid = 0;
    int to_uid = 0;
    std::string filename;
    int64_t total_size = 0;
    int64_t received_size = 0;
    std::vector<char> data;
    bool transfer_ready = false;
};

#endif
