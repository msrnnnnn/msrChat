/**
 * @file ImageStorage.h
 * @brief 图片持久化存储（SQLite）
 * @details 将 protobuf 图片消息的二进制数据分片写入 SQLite。
 *          复用 SQLiteMgr 连接池，使用互斥锁保证写操作串行化。
 */
#ifndef IMAGE_STORAGE_H
#define IMAGE_STORAGE_H

#include "SQLiteMgr.h"
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

/**
 * @struct ImageRecord
 * @brief 图片元数据记录
 */
struct ImageRecord
{
    std::string image_id;
    int from_uid = 0;
    int to_uid = 0;
    std::string ext;
    int64_t size = 0;
    std::string md5;
    int width = 0;
    int height = 0;
    int64_t created_at = 0;
    int64_t expires_at = 0;
    bool recalled = false;
};

/**
 * @class ImageStorage
 * @brief 图片 SQLite 存储单例
 * @details 分片存储图片数据：AppendChunk 写入分片，MarkCompleted 标记完成。
 *          过期图片通过 DeleteExpired 批量清理（7 天过期）。
 */
class ImageStorage
{
public:
    /**
     * @brief 获取单例实例
     */
    static ImageStorage &Instance();

    /**
     * @brief 初始化存储，复用外部 SQLite 连接池并创建必要的表
     * @param pool SQLite 连接池
     * @return 是否初始化成功
     */
    bool Init(std::shared_ptr<SQLiteConnectionPool> pool);

    /**
     * @brief 插入图片元数据记录
     * @param rec 图片元数据
     * @return 是否插入成功
     */
    bool Insert(const ImageRecord &rec);

    /**
     * @brief 追加图片数据分片
     * @param image_id 图片唯一 ID
     * @param offset 写入偏移量
     * @param data 数据指针
     * @param len 数据长度
     * @return 是否写入成功
     */
    bool AppendChunk(const std::string &image_id, int64_t offset, const uint8_t *data, size_t len);

    /**
     * @brief 标记图片传输完成，更新 expires_at 为 7 天后
     * @param image_id 图片唯一 ID
     * @return 是否更新成功
     */
    bool MarkCompleted(const std::string &image_id);

    /**
     * @brief 查询图片元数据
     * @param image_id 图片唯一 ID
     * @return 图片元数据，不存在时返回 std::nullopt
     */
    std::optional<ImageRecord> Get(const std::string &image_id);

    /**
     * @brief 按偏移和长度读取图片二进制数据
     * @param image_id 图片唯一 ID
     * @param offset 读取起始偏移
     * @param size 读取长度
     * @param out 输出缓冲区
     * @return 是否读取成功
     */
    bool ReadRange(const std::string &image_id, int64_t offset, int64_t size,
                   std::vector<uint8_t> &out);

    /**
     * @brief 标记图片已撤回
     * @param image_id 图片唯一 ID
     * @return 是否更新成功
     */
    bool MarkRecalled(const std::string &image_id);

    /**
     * @brief 删除所有已过期的图片及其数据
     * @param now 当前时间戳（秒）
     * @return 删除的图片数量
     */
    int DeleteExpired(int64_t now);

private:
    ImageStorage() = default;
    ~ImageStorage() = default;
    ImageStorage(const ImageStorage &) = delete;
    ImageStorage &operator=(const ImageStorage &) = delete;

    /**
     * @brief 创建图片存储所需的 SQLite 表
     * @return 是否创建成功
     */
    bool CreateTables();

    std::shared_ptr<SQLiteConnectionPool> _pool; ///< SQLite 连接池（外部传入，不持有所有权）
    std::mutex _mutex;                            ///< 保护写操作的互斥锁
};

#endif