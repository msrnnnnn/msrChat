/**
 * @file const.h
 * @brief 全局常量与错误码定义
 * @details 定义系统范围内的通用常量、错误代码枚举。
 * @version 1.0
 */

#pragma once

namespace ChatApp
{
/**
 * @enum ErrorCode
 * @brief 统一错误码定义 (Scoped Enum)
 * @note 使用 enum class 避免全局命名空间污染，并禁止隐式转换为 int。
 */
enum class ErrorCode
{
    Success = 0,       ///< 操作成功
    Error_Json = 1001, ///< JSON 解析失败 (格式错误或字段缺失)
    RPCFailed = 1002,  ///< gRPC 调用失败 (网络不可达或服务未启动)

    // @todo 建议预留更多错误段，例如:
    // 2000-2999: 数据库错误
    // 3000-3999: Redis 错误
    Error_VarifyExpired = 1003, ///< 验证码已过期
    Error_VarifyCodeErr = 1004, ///< 验证码不匹配
};
} // namespace ChatApp