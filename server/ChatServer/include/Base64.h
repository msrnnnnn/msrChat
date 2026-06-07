/**
 * @file Base64.h
 * @brief 轻量级 Base64 编解码工具（header-only）
 * @details 用于将 protobuf 二进制数据编码为可安全存储的文本
 */
#ifndef BASE64_H
#define BASE64_H

#include <string>
#include <cstdint>

namespace base64
{

/**
 * @brief 将二进制数据编码为 Base64 文本
 * @param input 原始二进制数据
 * @return Base64 编码后的字符串
 * @details 每 3 字节编码为 4 个字符，不足 3 字节以 '=' 填充
 */
inline std::string encode(const std::string &input)
{
    static const char table[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    out.reserve(((input.size() + 2) / 3) * 4);

    for (size_t i = 0; i < input.size(); i += 3)
    {
        uint32_t n = static_cast<uint8_t>(input[i]) << 16;
        if (i + 1 < input.size()) n |= static_cast<uint8_t>(input[i + 1]) << 8;
        if (i + 2 < input.size()) n |= static_cast<uint8_t>(input[i + 2]);

        out.push_back(table[(n >> 18) & 0x3F]);
        out.push_back(table[(n >> 12) & 0x3F]);
        out.push_back((i + 1 < input.size()) ? table[(n >> 6) & 0x3F] : '=');
        out.push_back((i + 2 < input.size()) ? table[n & 0x3F] : '=');
    }
    return out;
}

/**
 * @brief 将 Base64 文本解码为原始二进制数据
 * @param input Base64 编码的字符串
 * @return 解码后的原始二进制数据
 * @details 自动跳过无效字符，支持标准 Base64 格式（含 '=' 填充）
 */
inline std::string decode(const std::string &input)
{
    static const int table[] = {
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,62,-1,-1,-1,63,
        52,53,54,55,56,57,58,59,60,61,-1,-1,-1,-1,-1,-1,
        -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,
        15,16,17,18,19,20,21,22,23,24,25,-1,-1,-1,-1,-1,
        -1,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,
        41,42,43,44,45,46,47,48,49,50,51,-1,-1,-1,-1,-1
    };

    std::string out;
    out.reserve((input.size() / 4) * 3);

    for (size_t i = 0; i < input.size(); i += 4)
    {
        uint32_t n = 0;
        int pad = 0;
        for (int j = 0; j < 4 && (i + j) < input.size(); ++j)
        {
            unsigned char c = static_cast<unsigned char>(input[i + j]);
            if (c == '=') { ++pad; continue; }
            if (c >= 128 || table[c] == -1) continue;
            n = (n << 6) | static_cast<uint32_t>(table[c]);
        }
        int bytes = 3 - pad;
        for (int j = bytes - 1; j >= 0; --j)
        {
            out.push_back(static_cast<char>((n >> (j * 8)) & 0xFF));
        }
    }
    return out;
}

} // namespace base64

#endif // BASE64_H
