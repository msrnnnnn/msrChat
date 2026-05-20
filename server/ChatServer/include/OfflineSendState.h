#ifndef OFFLINE_SEND_STATE_H
#define OFFLINE_SEND_STATE_H

#include <cstdint>

struct OfflineSendState
{
    int uid = 0;
    int64_t total_count = 0;
    int64_t sent_count = 0;
    int64_t last_sent_id = 0;
    bool sending = false;
};

#endif
