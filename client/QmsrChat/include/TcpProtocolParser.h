#pragma once
/**
 * @file    TcpProtocolParser.h
 * @brief   TCP 协议解析器
 * @details 根据请求类型将收到的协议包分发到对应的反序列化路径，
 *          分为登录类协议和聊天类协议两条主线。
 */
#ifndef TCP_PROTOCOL_PARSER_H
#define TCP_PROTOCOL_PARSER_H

#include "Global.h"
#include <QByteArray>

class TcpMgr;

/**
 * @brief TCP 协议包解析器
 * @details 持有 TcpMgr 引用，解析完成后直接调用 TcpMgr 的信号发送回 UI 层。
 */
class TcpProtocolParser
{
public:
    explicit TcpProtocolParser(TcpMgr &tcpMgr) : _tcpMgr(tcpMgr) {}

    /**
     * @brief 解析登录类协议包（登录、注册、验证码、重置密码等）
     * @param req_type 请求类型枚举
     * @param data 协议消息体
     */
    void parseLoginPacket(RequestType req_type, const QByteArray &data);
    /**
     * @brief 解析聊天类协议包（消息、确认、文件、图片、撤回、编辑等）
     * @param req_type 请求类型枚举
     * @param data 协议消息体
     */
    void parseChatPacket(RequestType req_type, const QByteArray &data);

private:
    TcpMgr &_tcpMgr;
};

#endif
