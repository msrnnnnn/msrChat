#ifndef FILE_TRANSFER_STATE_H
#define FILE_TRANSFER_STATE_H

#include "FileDescriptor.h"
#include <cstdint>
#include <string>
#include <vector>

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

struct FileSendState
{
    int64_t task_id = 0;
    FileDescriptor fd;
    int64_t total_size = 0;
    int64_t sent_size = 0;
    std::string filename;
    bool sending = false;
};

#endif
