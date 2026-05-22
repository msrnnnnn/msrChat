#include <gtest/gtest.h>
#include "Protocol/TLVProtocol.h"

#include <cstring>
#include <vector>

TEST(TLVProtocolTest, GetHeadLen)
{
    TLVProtocol proto;
    EXPECT_EQ(proto.GetHeadLen(), 8);
}

TEST(TLVProtocolTest, EncodeDecodeRoundtrip)
{
    TLVProtocol proto;
    std::vector<char> data = {'h', 'e', 'l', 'l', 'o'};
    std::vector<char> buffer;

    ASSERT_TRUE(proto.Encode(1006, data, buffer));
    EXPECT_EQ(buffer.size(), 8 + 5);

    uint16_t msg_id = 0;
    std::vector<char> out;
    ASSERT_TRUE(proto.Decode(buffer, msg_id, out));

    EXPECT_EQ(msg_id, 1006);
    EXPECT_EQ(out, data);
    EXPECT_TRUE(buffer.empty());
}

TEST(TLVProtocolTest, EncodeDecodeEmptyData)
{
    TLVProtocol proto;
    std::vector<char> data;
    std::vector<char> buffer;

    ASSERT_TRUE(proto.Encode(1000, data, buffer));
    EXPECT_EQ(buffer.size(), 8);

    uint16_t msg_id = 0;
    std::vector<char> out;
    ASSERT_TRUE(proto.Decode(buffer, msg_id, out));

    EXPECT_EQ(msg_id, 1000);
    EXPECT_TRUE(out.empty());
}

TEST(TLVProtocolTest, DecodeInsufficientBuffer)
{
    TLVProtocol proto;
    std::vector<char> buffer = {'a', 'b', 'c'};

    uint16_t msg_id = 0;
    std::vector<char> out;
    EXPECT_FALSE(proto.Decode(buffer, msg_id, out));
}

TEST(TLVProtocolTest, ExceedMaxMessageLength)
{
    TLVProtocol proto;
    std::vector<char> data(TLVProtocol::MAX_MSG_LEN + 1, 'x');
    std::vector<char> buffer;

    EXPECT_FALSE(proto.Encode(1006, data, buffer));
}

TEST(TLVProtocolTest, DecodeExceedsMaxLength)
{
    TLVProtocol proto;

    std::vector<char> buffer(8 + TLVProtocol::MAX_MSG_LEN + 1);
    uint32_t exceed_len = TLVProtocol::MAX_MSG_LEN + 1;
    uint16_t net_msg_id = htons(1006);
    uint32_t net_len = htonl(exceed_len);

    std::memcpy(buffer.data(), &net_msg_id, sizeof(uint16_t));
    std::memcpy(buffer.data() + 2, &net_len, sizeof(uint32_t));

    uint16_t msg_id = 0;
    std::vector<char> out;
    EXPECT_FALSE(proto.Decode(buffer, msg_id, out));
}

TEST(TLVProtocolTest, MultipleRoundtrips)
{
    TLVProtocol proto;

    for (uint16_t i = 1; i <= 100; ++i)
    {
        std::vector<char> data(i, static_cast<char>(i));
        std::vector<char> buffer;

        ASSERT_TRUE(proto.Encode(i, data, buffer));

        uint16_t msg_id = 0;
        std::vector<char> out;
        ASSERT_TRUE(proto.Decode(buffer, msg_id, out));

        EXPECT_EQ(msg_id, i);
        EXPECT_EQ(out, data);
    }
}

TEST(TLVProtocolTest, BufferWithTrailingData)
{
    TLVProtocol proto;
    std::vector<char> data = {'x', 'y'};
    std::vector<char> buffer;

    ASSERT_TRUE(proto.Encode(1006, data, buffer));

    buffer.push_back('e');
    buffer.push_back('x');
    buffer.push_back('t');
    buffer.push_back('r');
    buffer.push_back('a');

    uint16_t msg_id = 0;
    std::vector<char> out;
    ASSERT_TRUE(proto.Decode(buffer, msg_id, out));

    EXPECT_EQ(out, data);
    EXPECT_EQ(buffer.size(), 5);
    EXPECT_EQ(buffer[0], 'e');
}
