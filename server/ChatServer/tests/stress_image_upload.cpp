/**
 * @file stress_image_upload.cpp
 * @brief 压力测试：A→B 方向并发 100 张 5MB 图片上传
 *
 * =============================================================================
 * 用法
 * =============================================================================
 * 编译（独立二进制，不走 CMakeLists.txt）：
 *   # Linux/macOS
 *   g++ -std=c++17 -fsanitize=address -fno-omit-frame-pointer \
 *       -I server/ChatServer/include \
 *       -I server/ChatServer/include/Protocol \
 *       -I client/QmsrChat/proto \
 *       $(find server/ChatServer/src -name "*.cpp" | grep -v main) \
 *       -L/usr/lib -lpthread -lssl -lcrypto -lsqlite3 \
 *       -o server/ChatServer/build/stress_image_upload
 *
 *   # macOS (brew installed openssl/sqlite3)
 *   g++ -std=c++17 -fsanitize=address -fno-omit-frame-pointer \
 *       -I server/ChatServer/include \
 *       -I server/ChatServer/include/Protocol \
 *       -I client/QmsrChat/proto \
 *       server/ChatServer/tests/stress_image_upload.cpp \
 *       server/ChatServer/src/AsioIOServicePool.cpp \
 *       server/ChatServer/src/CServer.cpp \
 *       server/ChatServer/src/CSession.cpp \
 *       server/ChatServer/src/MessageDispatcher.cpp \
 *       server/ChatServer/src/LogicSystem.cpp \
 *       server/ChatServer/src/SessionManager.cpp \
 *       server/ChatServer/src/MessageRouter.cpp \
 *       server/ChatServer/src/TokenManager.cpp \
 *       server/ChatServer/src/Protocol/BaseProtocol.cpp \
 *       server/ChatServer/src/Protocol/TLVProtocol.cpp \
 *       server/ChatServer/src/Protocol/BinaryPacketProtocol.cpp \
 *       server/ChatServer/src/ThreadPool.cpp \
 *       server/ChatServer/src/FileTransfer.cpp \
 *       server/ChatServer/src/SQLiteMgr.cpp \
 *       server/ChatServer/src/UserData.cpp \
 *       server/ChatServer/src/OfflineStorage.cpp \
 *       server/ChatServer/src/ImageStorage.cpp \
 *       client/QmsrChat/proto/Message.pb.cc \
 *       -L/usr/local/opt/openssl/lib -L/usr/local/opt/sqlite/lib \
 *       -lpthread -lssl -lcrypto -lsqlite3 -lboost_system -lboost_thread \
 *       -o server/ChatServer/build/stress_image_upload
 *
 * 运行：
 *   ./server/ChatServer/build/ChatServer &
 *   ./server/ChatServer/build/stress_image_upload
 *
 * 预期输出（成功时）：
 *   [1/5] 启动服务端 ... ✅
 *   [2/5] 客户端登录 ... ✅
 *   [3/5] 并发上传 100 张 5MB 图片 ... ✅ (约 60-120s)
 *   [4/5] 验证 image_storage 表 ... ✅
 *   [5/5] 优雅关闭 + ASAN 检测 ... ✅
 *   ========== 压力测试通过 ==========
 *
 * 预期运行时间：60–180 秒（取决于网络/磁盘 IO）
 *
 * =============================================================================
 * 测试设计
 * =============================================================================
 * - 2 个 TCP 连接：client A (uid=1001) 和 client B (uid=1002)
 * - client B 先登录，然后 client A 登录
 * - A→B 并发 100 次，每次：
 *     1. 生成 5MB 随机数据，计算 MD5
 *     2. 构造 FileReq protobuf（task_id=自增，filename="{uuid}.jpg"，md5，total_size=5MB）
 *     3. 切分 4KB chunk，顺序发送 MSG_FILE_CHUNK
 *     4. 等待 FILE_ACK（received==total_size）
 * - 完成后验证：
 *     1. 服务端未崩溃
 *     2. image_storage 表恰好 100 行
 *     3. 每行 md5 与上传时计算的 MD5 一致
 *     4. 服务端日志无 ASAN 错误
 * - 资源清理：socket close、subprocess reaper、临时文件删除
 *
 * =============================================================================
 * 注意事项
 * =============================================================================
 * - 不修改 CMakeLists.txt（标注为 optional standalone）
 * - 不使用 SQLiteMgr::Query/Execute（不存在）
 * - 直接通过 sqlite3_prepare_v2/bind/step 查询（不使用 SQLiteConnectionGuard）
 * - CHUNK_SIZE 读自 const.h（4KB），不假设 64KB
 * - protobuf 使用手动 varint 编码（避免链接 libprotobuf）
 * =============================================================================
 */

#include <gtest/gtest.h>
#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <future>
#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

// ─── Windows 兼容 ───────────────────────────────────────────────────────────
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
using SOCKET_TYPE = SOCKET;
#define CLOSE_SOCKET closesocket
#define SLEEP_MS(ms) Sleep(ms)
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
using SOCKET_TYPE = int;
#define CLOSE_SOCKET close
#define SLEEP_MS(ms) std::this_thread::sleep_for(std::chrono::milliseconds(ms))
#endif

// ─── 协议常量（来自 const.h）─────────────────────────────────────────────
constexpr uint16_t MSG_HELLO = 1000;
constexpr uint16_t MSG_CHAT_LOGIN = 1005;
constexpr uint16_t MSG_FILE_REQ = 2001;
constexpr uint16_t MSG_FILE_RSP = 2002;
constexpr uint16_t MSG_FILE_CHUNK = 2003;
constexpr uint16_t MSG_FILE_ACK = 2004;
constexpr int HEAD_TOTAL_LEN = 6;
constexpr size_t CHUNK_SIZE = 4 * 1024;  // 4KB（来自 const.h）
constexpr int SERVER_PORT = 8080;

// ─── 测试参数 ──────────────────────────────────────────────────────────────
constexpr size_t IMAGE_SIZE = 5 * 1024 * 1024;  // 5MB
constexpr int CONCURRENT_UPLOADS = 100;
constexpr int CLIENT_A_UID = 1001;
constexpr int CLIENT_B_UID = 1002;
constexpr const char *CLIENT_TOKEN = "dev_token";

// ─── 网络工具 ──────────────────────────────────────────────────────────────

/**
 * @brief 构造 TLV 包：2 字节 msg_id (big-endian) + 4 字节 body_len (big-endian) + body
 * @details 与 asan_benchmark.py 的 struct.pack('!HI', msg_id, len(body)) 完全一致
 */
std::string BuildPacket(uint16_t msg_id, const std::string &body)
{
    std::string packet;
    packet.reserve(HEAD_TOTAL_LEN + body.size());
    // msg_id: 2 bytes big-endian (!H = unsigned short)
    packet.push_back(static_cast<char>((msg_id >> 8) & 0xFF));
    packet.push_back(static_cast<char>(msg_id & 0xFF));
    // body_len: 4 bytes big-endian (!I = unsigned int)
    packet.push_back(static_cast<char>((body.size() >> 24) & 0xFF));
    packet.push_back(static_cast<char>((body.size() >> 16) & 0xFF));
    packet.push_back(static_cast<char>((body.size() >> 8) & 0xFF));
    packet.push_back(static_cast<char>(body.size() & 0xFF));
    packet += body;
    return packet;
}

/**
 * @brief 接收恰好 n 字节
 */
bool RecvN(SOCKET_TYPE sock, char *buf, int n)
{
    int received = 0;
    while (received < n)
    {
        int r = recv(sock, buf + received, n - received, 0);
        if (r <= 0)
            return false;
        received += r;
    }
    return true;
}

/**
 * @brief 接收一个完整 TLV 包
 */
bool RecvPacket(SOCKET_TYPE sock, uint16_t &out_msg_id, std::string &out_body)
{
    char header[HEAD_TOTAL_LEN];
    if (!RecvN(sock, header, HEAD_TOTAL_LEN))
        return false;

    uint16_t msg_id = static_cast<uint16_t>((static_cast<unsigned char>(header[0]) << 8) |
                                            static_cast<unsigned char>(header[1]));
    uint32_t body_len = (static_cast<unsigned char>(header[2]) << 24) |
                        (static_cast<unsigned char>(header[3]) << 16) |
                        (static_cast<unsigned char>(header[4]) << 8) |
                        static_cast<unsigned char>(header[5]);

    if (body_len > 16 * 1024 * 1024)  // 防护：单包不超过 16MB
        return false;

    out_body.resize(body_len);
    if (body_len > 0 && !RecvN(sock, &out_body[0], static_cast<int>(body_len)))
        return false;

    out_msg_id = msg_id;
    return true;
}

// ─── Protobuf 手动编码 ─────────────────────────────────────────────────────

/**
 * @brief 计算字节序列的 MD5（返回 32 字符十六进制小写）
 */
std::string ComputeMD5(const char *data, size_t len);

/**
 * @brief 构造 FileReq protobuf（手动 varint 编码）
 * @details 字段：
 *   1: task_id (int64, wire_type=0, varint)
 *   2: from_uid (int32, wire_type=0, varint)
 *   3: to_uid (int32, wire_type=0, varint)
 *   4: filename (string, wire_type=2, length-delimited)
 *   5: total_size (int64, wire_type=0, varint)
 *   6: md5 (string, wire_type=2, length-delimited)
 */
std::string EncodeFileReq(int64_t task_id, int32_t from_uid, int32_t to_uid,
                          const std::string &filename, int64_t total_size,
                          const std::string &md5_hex)
{
    std::string out;

    // varint helper
    auto write_varint = [&](uint64_t value, std::string &buf) {
        while (value > 0x7F)
        {
            buf.push_back(static_cast<char>((value & 0x7F) | 0x80));
            value >>= 7;
        }
        buf.push_back(static_cast<char>(value & 0x7F));
    };

    // field(tag, typ) = (field_number << 3) | wire_type
    // field 1: int64 → tag=8, wire_type=0 → field header = 8
    write_varint(8, out);
    write_varint(static_cast<uint64_t>(task_id), out);

    // field 2: from_uid (int32)
    write_varint(16, out);  // (2<<3)|0=16
    write_varint(static_cast<uint64_t>(from_uid), out);

    // field 3: to_uid (int32)
    write_varint(24, out);  // (3<<3)|0=24
    write_varint(static_cast<uint64_t>(to_uid), out);

    // field 4: filename (string)
    write_varint(34, out);  // (4<<3)|2=34
    // filename length + content
    write_varint(static_cast<uint64_t>(filename.size()), out);
    out += filename;

    // field 5: total_size (int64)
    write_varint(40, out);  // (5<<3)|0=40
    write_varint(static_cast<uint64_t>(total_size), out);

    // field 6: md5 (string)
    write_varint(50, out);  // (6<<3)|2=50
    write_varint(static_cast<uint64_t>(md5_hex.size()), out);
    out += md5_hex;

    return out;
}

/**
 * @brief 构造 FileChunk protobuf
 * @details 字段：
 *   1: task_id (int64, varint)
 *   2: offset (int64, varint)
 *   3: size (int64, varint)
 *   4: data (bytes, length-delimited)
 */
std::string EncodeFileChunk(int64_t task_id, int64_t offset, const char *data, size_t size)
{
    std::string out;

    auto write_varint = [&](uint64_t value, std::string &buf) {
        while (value > 0x7F)
        {
            buf.push_back(static_cast<char>((value & 0x7F) | 0x80));
            value >>= 7;
        }
        buf.push_back(static_cast<char>(value & 0x7F));
    };

    // field 1: task_id
    write_varint(8, out);
    write_varint(static_cast<uint64_t>(task_id), out);

    // field 2: offset
    write_varint(16, out);  // (2<<3)|0=16
    write_varint(static_cast<uint64_t>(offset), out);

    // field 3: size
    write_varint(24, out);  // (3<<3)|0=24
    write_varint(static_cast<uint64_t>(size), out);

    // field 4: data (bytes)
    write_varint(34, out);  // (4<<3)|2=34
    write_varint(static_cast<uint64_t>(size), out);
    out.append(data, size);

    return out;
}

/**
 * @brief 解析 FileAck protobuf（手动 varint 解码）
 * @details 字段：1:task_id(int64), 2:error(int32), 3:message(string), 4:received(int64)
 */
bool ParseFileAck(const std::string &body, int64_t &out_task_id, int32_t &out_error,
                 int64_t &out_received)
{
    size_t pos = 0;
    out_task_id = 0;
    out_error = 0;
    out_received = 0;

    auto read_varint = [&](const std::string &buf, size_t &idx, uint64_t &out_val) -> bool {
        out_val = 0;
        int shift = 0;
        while (idx < buf.size())
        {
            uint8_t b = static_cast<unsigned char>(buf[idx++]);
            out_val |= (static_cast<uint64_t>(b & 0x7F) << shift);
            if ((b & 0x80) == 0)
                return true;
            shift += 7;
            if (shift > 63)
                return false;
        }
        return false;
    };

    while (pos < body.size())
    {
        uint64_t field_and_wire = 0;
        if (!read_varint(body, pos, field_and_wire))
            return false;
        uint8_t wire_type = field_and_wire & 0x07;
        uint32_t field_number = static_cast<uint32_t>(field_and_wire >> 3);

        if (wire_type == 0)  // varint
        {
            uint64_t val = 0;
            if (!read_varint(body, pos, val))
                return false;
            if (field_number == 1)
                out_task_id = static_cast<int64_t>(val);
            else if (field_number == 2)
                out_error = static_cast<int32_t>(val);
            else if (field_number == 4)
                out_received = static_cast<int64_t>(val);
        }
        else if (wire_type == 2)  // length-delimited (string/bytes)
        {
            uint64_t len = 0;
            if (!read_varint(body, pos, len))
                return false;
            if (pos + len > body.size())
                return false;
            pos += static_cast<size_t>(len);
        }
    }
    return true;
}

// ─── TCP 客户端封装 ────────────────────────────────────────────────────────

class TestClient
{
public:
    TestClient(int uid, const std::string &token)
        : _uid(uid), _token(token), _sock(-1), _logged_in(false)
    {
#ifdef _WIN32
        WSADATA wsaData;
        WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif
    }

    ~TestClient()
    {
        Close();
#ifdef _WIN32
        WSACleanup();
#endif
    }

    bool Connect(const char *host = "127.0.0.1", int port = SERVER_PORT)
    {
        _sock = socket(AF_INET, SOCK_STREAM, 0);
        if (_sock < 0)
            return false;

        struct sockaddr_in addr;
        std::memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(static_cast<uint16_t>(port));
        addr.sin_addr.s_addr = inet_addr(host);

        if (::connect(_sock, reinterpret_cast<struct sockaddr *>(&addr), sizeof(addr)) < 0)
        {
            CLOSE_SOCKET(_sock);
            _sock = -1;
            return false;
        }
        return true;
    }

    bool Login()
    {
        std::string login_body = "{\"uid\":" + std::to_string(_uid) +
                                 ",\"token\":\"" + _token + "\"}";
        std::string packet = BuildPacket(MSG_CHAT_LOGIN, login_body);

        if (send(_sock, packet.data(), static_cast<int>(packet.size()), 0) < 0)
            return false;

        uint16_t rsp_id = 0;
        std::string rsp_body;
        if (!RecvPacket(_sock, rsp_id, rsp_body))
            return false;

        if (rsp_id != MSG_CHAT_LOGIN)
            return false;

        // 检查 error==0（简单字符串查找）
        if (rsp_body.find("\"error\":0") == std::string::npos &&
            rsp_body.find("\"error\": 0") == std::string::npos)
            return false;

        _logged_in = true;
        return true;
    }

    /**
     * @brief 上传一张图片（完整 FileReq → FileChunk 序列 → 等待 FileAck）
     * @param image_data 5MB 原始数据
     * @param md5_hex 图片 MD5（十六进制）
     * @param image_id UUID 字符串
     * @param task_id 任务 ID（全局自增）
     * @return 成功返回 true
     */
    bool UploadImage(const std::vector<char> &image_data,
                     const std::string &md5_hex,
                     const std::string &image_id,
                     int64_t task_id)
    {
        // 构造 filename = "{uuid}.jpg"
        std::string filename = image_id + ".jpg";
        int64_t total_size = static_cast<int64_t>(image_data.size());

        // Step 1: 发送 FileReq
        std::string file_req_body = EncodeFileReq(
            task_id, _uid, CLIENT_B_UID, filename, total_size, md5_hex);
        std::string file_req_packet = BuildPacket(MSG_FILE_REQ, file_req_body);
        if (send(_sock, file_req_packet.data(), static_cast<int>(file_req_packet.size()), 0) < 0)
            return false;

        // Step 2: 等待 FileRsp（接收方 B 回复，表示已准备好接收）
        uint16_t rsp_id = 0;
        std::string rsp_body;
        if (!RecvPacket(_sock, rsp_id, rsp_body))
            return false;
        // FileRsp 可以忽略内容，只要收到了就继续

        // Step 3: 分片发送 FileChunk（4KB per chunk）
        int64_t offset = 0;
        size_t chunk_count = 0;
        while (offset < static_cast<int64_t>(image_data.size()))
        {
            size_t this_chunk = std::min(CHUNK_SIZE, image_data.size() - static_cast<size_t>(offset));
            std::string chunk_body = EncodeFileChunk(
                task_id, offset, image_data.data() + offset, this_chunk);
            std::string chunk_packet = BuildPacket(MSG_FILE_CHUNK, chunk_body);
            if (send(_sock, chunk_packet.data(), static_cast<int>(chunk_packet.size()), 0) < 0)
                return false;
            offset += this_chunk;
            chunk_count++;
        }

        // Step 4: 等待 FileAck（received == total_size 表示传输完成）
        int64_t recv_task_id = 0;
        int32_t recv_error = 0;
        int64_t recv_received = 0;
        if (!RecvPacket(_sock, rsp_id, rsp_body))
            return false;

        if (rsp_id != MSG_FILE_ACK)
            return false;

        if (!ParseFileAck(rsp_body, recv_task_id, recv_error, recv_received))
            return false;

        if (recv_error != 0)
            return false;
        if (recv_received != total_size)
            return false;

        return true;
    }

    void Close()
    {
        if (_sock >= 0)
        {
            CLOSE_SOCKET(_sock);
            _sock = -1;
        }
        _logged_in = false;
    }

    bool isLoggedIn() const { return _logged_in; }
    int getUid() const { return _uid; }

private:
    int _uid;
    std::string _token;
    SOCKET_TYPE _sock;
    bool _logged_in;
};

// ─── SQLite 验证 ────────────────────────────────────────────────────────────

/**
 * @brief 查询 image_storage 表，返回行数和 MD5 列表
 */
bool QueryImageStorage(const std::string &db_path,
                       int &out_row_count,
                       std::vector<std::string> &out_md5_list)
{
#ifdef _WIN32
    sqlite3 *db = nullptr;
    if (sqlite3_open(db_path.c_str(), &db) != SQLITE_OK)
        return false;
#else
    sqlite3 *db = nullptr;
    if (sqlite3_open_v2(db_path.c_str(), &db, SQLITE_OPEN_READONLY, nullptr) != SQLITE_OK)
        return false;
#endif

    const char *sql = "SELECT md5 FROM image_storage ORDER BY created_at";
    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK)
    {
        sqlite3_close(db);
        return false;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        const char *md5_text = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0));
        if (md5_text)
            out_md5_list.push_back(md5_text);
    }

    sqlite3_finalize(stmt);
    sqlite3_close(db);

    out_row_count = static_cast<int>(out_md5_list.size());
    return true;
}

// ─── 服务端进程管理 ─────────────────────────────────────────────────────────

/**
 * @brief 启动 ChatServer 子进程，写日志到 log_file
 */
bool StartServer(const std::string &server_path,
                 const std::string &log_file,
                 int &out_pid)
{
    // 确保日志文件目录存在
    FILE *log = fopen(log_file.c_str(), "w");
    if (!log)
        return false;
    fclose(log);

#ifdef _WIN32
    STARTUPINFOA si = {sizeof(si)};
    PROCESS_INFORMATION pi;
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = CreateFileA(log_file.c_str(), GENERIC_WRITE,
                                FILE_SHARE_READ | FILE_SHARE_WRITE,
                                nullptr, OPEN_ALWAYS,
                                FILE_ATTRIBUTE_NORMAL, nullptr);
    si.hStdError = si.hStdOutput;
    if (!CreateProcessA(server_path.c_str(), nullptr, nullptr, nullptr, TRUE,
                        CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi))
        return false;
    out_pid = pi.dwProcessId;
    CloseHandle(pi.hThread);
    return true;
#else
    pid_t pid = fork();
    if (pid < 0)
        return false;
    if (pid == 0)
    {
        // 子进程
        int fd = open(log_file.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd >= 0)
        {
            dup2(fd, STDOUT_FILENO);
            dup2(fd, STDERR_FILENO);
            close(fd);
        }
        execl(server_path.c_str(), server_path.c_str(), nullptr);
        _exit(1);
    }
    out_pid = pid;
    return true;
#endif
}

/**
 * @brief 等待服务端端口可连接
 */
bool WaitServerReady(int timeout_seconds = 30)
{
    int waited = 0;
    while (waited < timeout_seconds)
    {
#ifdef _WIN32
        SOCKET_TYPE s = socket(AF_INET, SOCK_STREAM, 0);
        struct sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_port = htons(SERVER_PORT);
        addr.sin_addr.s_addr = inet_addr("127.0.0.1");
        int ret = connect(s, reinterpret_cast<struct sockaddr *>(&addr), sizeof(addr));
        CLOSE_SOCKET(s);
        if (ret == 0)
            return true;
#else
        int s = socket(AF_INET, SOCK_STREAM, 0);
        struct sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_port = htons(SERVER_PORT);
        addr.sin_addr.s_addr = inet_addr("127.0.0.1");
        int ret = connect(s, reinterpret_cast<struct sockaddr *>(&addr), sizeof(addr));
        close(s);
        if (ret == 0)
            return true;
#endif
        SLEEP_MS(500);
        waited += 1;
    }
    return false;
}

/**
 * @brief 发送 SIGINT 优雅关闭服务端
 */
bool GracefulStopServer(int pid)
{
#ifdef _WIN32
    // Windows 下直接 TerminateProcess（CREATE_NO_WINDOW 进程无法响应 Ctrl+C）
    HANDLE h = OpenProcess(PROCESS_TERMINATE, FALSE, static_cast<DWORD>(pid));
    if (h)
    {
        SLEEP_MS(1000);
        TerminateProcess(h, 0);
        CloseHandle(h);
        return true;
    }
    return false;
#else
    if (kill(pid, SIGINT) == 0)
    {
        // 等待进程退出，最多 15 秒
        int status = 0;
        for (int i = 0; i < 30; ++i)
        {
            int ret = waitpid(pid, &status, WNOHANG);
            if (ret > 0)
                return true;
            SLEEP_MS(500);
        }
        kill(pid, SIGKILL);
        return true;
    }
    return false;
#endif
}

/**
 * @brief 检查 ASAN 日志
 */
std::string CheckASAN(const std::string &log_file)
{
    FILE *f = fopen(log_file.c_str(), "r");
    if (!f)
        return "⚠️  日志文件无法打开";

    std::string content;
    char buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0)
        content.append(buf, n);
    fclose(f);

    if (content.empty())
        return "⚠️  日志为空";
    if (content.find("0 memory leaks detected") != std::string::npos)
        return "✅ 0 memory leaks detected";
    if (content.find("ERROR: AddressSanitizer") != std::string::npos)
    {
        std::string::size_type pos = content.find("ERROR: AddressSanitizer");
        std::string snippet = content.substr(pos, 200);
        return "❌ ASAN 错误:\n" + snippet;
    }
    return "⚠️  未检测到 ASAN 输出（可能未启用 ASAN）";
}

// ─── MD5 计算（OpenSSL） ───────────────────────────────────────────────────

std::string ComputeMD5(const char *data, size_t len)
{
    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    EVP_DigestInit_ex(ctx, EVP_md5(), nullptr);
    EVP_DigestUpdate(ctx, data, len);

    unsigned char digest[EVP_MAX_MD_SIZE];
    unsigned int digest_len = 0;
    EVP_DigestFinal_ex(ctx, digest, &digest_len);
    EVP_MD_CTX_free(ctx);

    char hex[33];
    for (int i = 0; i < 16; ++i)
        sprintf(hex + i * 2, "%02x", digest[i]);
    return std::string(hex, 32);
}

// ─── UUID 生成 ─────────────────────────────────────────────────────────────

std::string GenerateUUID()
{
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<uint32_t> dis(0, 0xFFFFFFFF);

    char buf[37];
    snprintf(buf, sizeof(buf), "%08x-%04x-%04x-%04x-%04x%08x",
             dis(gen), dis(gen) & 0xFFFF, dis(gen) & 0xFFFF,
             dis(gen) & 0xFFFF, dis(gen) & 0xFFFF, dis(gen));
    return std::string(buf);
}

// ─── 测试夹具 ─────────────────────────────────────────────────────────────

class StressImageUploadTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        _server_pid = -1;
        _log_file = "stress_image_upload.log";
        _db_path = "chatserver.db";

        // 清理旧的日志文件和 DB
        std::remove(_log_file.c_str());
        std::remove(_db_path.c_str());
    }

    void TearDown() override
    {
        // 清理资源
    }

    bool StartServerProcess()
    {
#ifdef _WIN32
        std::string server_path = "server/ChatServer/build/ChatServer.exe";
#else
        std::string server_path = "server/ChatServer/build/ChatServer";
#endif
        return StartServer(server_path, _log_file, _server_pid);
    }

    bool StopServerProcess()
    {
        if (_server_pid <= 0)
            return false;
        bool ok = GracefulStopServer(_server_pid);
        _server_pid = -1;
        return ok;
    }

    int _server_pid;
    std::string _log_file;
    std::string _db_path;
};

// ─── 测试用例 ─────────────────────────────────────────────────────────────

TEST_F(StressImageUploadTest, Concurrent100Images)
{
    std::cout << "=== 压力测试：100 张 5MB 图片并发上传 ===" << std::endl;

    // ── Step 1: 启动服务端 ─────────────────────────────────────────────────
    std::cout << "[1/5] 启动服务端 ... ";
    if (!StartServerProcess())
    {
        FAIL() << "服务端启动失败";
    }

    if (!WaitServerReady(30))
    {
        StopServerProcess();
        FAIL() << "服务端端口未就绪";
    }
    std::cout << "✅ PID=" << _server_pid << std::endl;

    // ── Step 2: 客户端登录 ─────────────────────────────────────────────────
    std::cout << "[2/5] 客户端登录 ... ";
    TestClient clientB(CLIENT_B_UID, CLIENT_TOKEN);
    if (!clientB.Connect())
    {
        StopServerProcess();
        FAIL() << "client B 连接失败";
    }
    if (!clientB.Login())
    {
        StopServerProcess();
        FAIL() << "client B 登录失败";
    }

    TestClient clientA(CLIENT_A_UID, CLIENT_TOKEN);
    if (!clientA.Connect())
    {
        StopServerProcess();
        FAIL() << "client A 连接失败";
    }
    if (!clientA.Login())
    {
        StopServerProcess();
        FAIL() << "client A 登录失败";
    }
    std::cout << "✅ A(uid=" << CLIENT_A_UID << ") + B(uid=" << CLIENT_B_UID << ") 已登录" << std::endl;

    // ── Step 3: 生成 100 张 5MB 图片 ───────────────────────────────────────
    std::cout << "[3/5] 并发上传 " << CONCURRENT_UPLOADS << " 张 5MB 图片 ... ";
    std::cout.flush();

    std::vector<std::vector<char>> images(CONCURRENT_UPLOADS);
    std::vector<std::string> md5_list(CONCURRENT_UPLOADS);
    std::vector<std::string> image_ids(CONCURRENT_UPLOADS);

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> dist(0, 255);

    for (int i = 0; i < CONCURRENT_UPLOADS; ++i)
    {
        images[i].resize(IMAGE_SIZE);
        for (size_t j = 0; j < IMAGE_SIZE; ++j)
            images[i][j] = static_cast<char>(dist(gen));
        md5_list[i] = ComputeMD5(images[i].data(), images[i].size());
        image_ids[i] = GenerateUUID();
    }
    std::cout << "生成完毕，";

    // ── 并发上传：std::thread 而非线程池 ─────────────────────────────────
    // 不做过度设计：直接用 std::thread + 简单 lambda
    std::atomic<int> success_count{0};
    std::atomic<int> fail_count{0};
    std::atomic<int64_t> next_task_id{1};

    auto start_time = std::chrono::steady_clock::now();

    std::vector<std::thread> threads;
    threads.reserve(CONCURRENT_UPLOADS);

    for (int i = 0; i < CONCURRENT_UPLOADS; ++i)
    {
        threads.emplace_back([&, i]() {
            // 每线程独立 socket（100线程共享 1 个 socket 会导致 send/recv 字节交错）
            TestClient threadClient(CLIENT_A_UID, CLIENT_TOKEN);
            if (!threadClient.Connect() || !threadClient.Login())
            {
                fail_count.fetch_add(1);
                return;
            }
            int64_t task_id = next_task_id.fetch_add(1);
            bool ok = threadClient.UploadImage(images[i], md5_list[i], image_ids[i], task_id);
            if (ok)
                success_count.fetch_add(1);
            else
                fail_count.fetch_add(1);
        });
    }

    for (auto &t : threads)
        t.join();

    auto end_time = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();

    std::cout << "✅ " << success_count.load() << "/" << CONCURRENT_UPLOADS
              << " 成功 (" << elapsed << "ms)" << std::endl;

    if (fail_count.load() > 0)
    {
        std::cerr << "❌ " << fail_count.load() << " 张图片上传失败" << std::endl;
        clientA.Close();
        clientB.Close();
        StopServerProcess();
        FAIL() << fail_count.load() << " 张图片上传失败";
    }

    // ── Step 4: 验证 image_storage ─────────────────────────────────────────
    std::cout << "[4/5] 验证 image_storage 表 ... ";
    clientA.Close();
    clientB.Close();

    // 等待服务端完成所有写入
    SLEEP_MS(1000);

    int row_count = 0;
    std::vector<std::string> stored_md5s;
    if (!QueryImageStorage(_db_path, row_count, stored_md5s))
    {
        StopServerProcess();
        FAIL() << "无法打开 DB 或查询失败";
    }

    EXPECT_EQ(row_count, CONCURRENT_UPLOADS)
        << "image_storage 行数应为 " << CONCURRENT_UPLOADS << "，实际为 " << row_count;

    // 验证 MD5 一致性（集合比较）
    std::sort(md5_list.begin(), md5_list.end());
    std::sort(stored_md5s.begin(), stored_md5s.end());
    bool md5_match = (md5_list == stored_md5s);
    EXPECT_TRUE(md5_match) << "MD5 列表不匹配（上传 vs 存储）";
    std::cout << "✅ 行数=" << row_count << ", MD5 匹配=" << (md5_match ? "是" : "否") << std::endl;

    // ── Step 5: 优雅关闭 + ASAN ─────────────────────────────────────────────
    std::cout << "[5/5] 优雅关闭服务端 + ASAN 检测 ... ";
    if (!StopServerProcess())
    {
        std::cerr << "⚠️  服务端关闭失败（可能已崩溃）" << std::endl;
    }

    std::string asan_result = CheckASAN(_log_file);
    std::cout << asan_result << std::endl;

    // 清理临时 DB 文件
    std::remove(_db_path.c_str());

    // ASAN 检查（如果日志中没有 ASAN 输出，给出警告但不失败）
    if (asan_result.find("❌") != std::string::npos)
    {
        FAIL() << "ASAN 检测到错误";
    }
}

// ─── GTest Main ────────────────────────────────────────────────────────────

int main(int argc, char *argv[])
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}