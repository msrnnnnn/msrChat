#pragma once
/**
 * @file OfflineSendState.h
 * @brief 离线消息发送状态 —— 记录某个用户的离线消息投递进度
 */
#ifndef OFFLINE_SEND_STATE_H
#define OFFLINE_SEND_STATE_H

#include <cstdint>

/**
 * @brief 离线消息发送状态
 * @details 用户上线后，系统按批次拉取离线消息并逐条推送。
 *          该结构记录当前批次的总量、已发送数和最后发送的消息 ID，
 *          用于断点续传和防止重复发送。
 */
struct OfflineSendState
{
    int uid = 0;                 ///< 目标用户 ID
    int64_t total_count = 0;     ///< 待发送总量
    int64_t sent_count = 0;      ///< 已发送数量
    int64_t last_sent_id = 0;    ///< 最后一条已发送消息的 ID（用于断点续传）
    bool sending = false;        ///< 是否正在发送中
};

#endif
