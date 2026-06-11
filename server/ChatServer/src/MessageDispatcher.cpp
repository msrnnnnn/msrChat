/**
 * @file    MessageDispatcher.cpp
 * @brief   消息分发器实现 —— 纯路由层
 * @details 仅负责将消息类型映射到对应 Service 的处理器函数，
 *          所有业务逻辑已拆分到 services/ 目录下的 Service 类中。
 */

#include "MessageDispatcher.h"
#include "services/AuthService.h"
#include "services/ChatService.h"
#include "services/FileService.h"
#include "services/ImageService.h"

void MessageDispatcher::RegisterDefaultHandlers()
{
    // ── 心跳 ──
    RegisterHandler(MSG_HELLO,
        [](CSession &session, const std::string &body_data) -> bool {
            session.Send(body_data, MSG_HELLO);
            session.ContinueReading();
            return true;
        }, false);

    // ── AuthService：认证相关 ──
    RegisterHandler(MSG_CHAT_LOGIN,      AuthService::HandleLoginRequest,         false);
    RegisterHandler(ID_REGISTER_USER,    AuthService::HandleRegisterRequest,       false);
    RegisterHandler(ID_LOGIN_USER,       AuthService::HandleLoginAuthRequest,      false);
    RegisterHandler(ID_GET_VERIFY_CODE,  AuthService::HandleGetVerifyCodeRequest,  false);
    RegisterHandler(ID_RESET_PWD,        AuthService::HandleResetPwdRequest,       false);

    // ── ChatService：聊天文本 + 离线 ──
    RegisterHandler(MSG_CHAT_TEXT,       ChatService::HandleChatText,              true);
    RegisterHandler(MSG_OFFLINE_ACK,     ChatService::HandleOfflineAck,            true);

    // ── FileService：文件传输 ──
    RegisterHandler(MSG_FILE_REQ,        FileService::HandleFileReq,               true);
    RegisterHandler(MSG_FILE_RSP,        FileService::HandleFileRsp,               true);
    RegisterHandler(MSG_FILE_CHUNK,
        [](CSession &session, const std::string &body_data) -> bool {
            return FileService::HandleFileChunk(session, body_data);
        }, true);
    RegisterHandler(MSG_FILE_ACK,        FileService::HandleFileAck,               true);

    // ── ImageService：图片 + 撤回/编辑 ──
    RegisterHandler(MSG_CHAT_IMAGE,          ImageService::HandleChatImage,          true);
    RegisterHandler(MSG_IMAGE_DOWNLOAD_REQ,  ImageService::HandleImageDownloadReq,   true);
    RegisterHandler(MSG_CHAT_RECALL,         ImageService::HandleChatRecall,          true);
    RegisterHandler(MSG_CHAT_EDIT,           ImageService::HandleChatEdit,            true);
}
