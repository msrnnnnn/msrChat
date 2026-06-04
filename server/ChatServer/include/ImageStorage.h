#ifndef IMAGE_STORAGE_H
#define IMAGE_STORAGE_H

#include "SQLiteMgr.h"
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

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

class ImageStorage
{
public:
    static ImageStorage &Instance();

    // 复用 SQLiteMgr 的连接池（避免双重连接同一 DB 文件的锁竞争）
    bool Init(std::shared_ptr<SQLiteConnectionPool> pool);

    bool Insert(const ImageRecord &rec);
    bool AppendChunk(const std::string &image_id, int64_t offset, const uint8_t *data, size_t len);
    bool MarkCompleted(const std::string &image_id);
    std::optional<ImageRecord> Get(const std::string &image_id);
    bool ReadRange(const std::string &image_id, int64_t offset, int64_t size,
                   std::vector<uint8_t> &out);
    bool MarkRecalled(const std::string &image_id);
    int DeleteExpired(int64_t now);

private:
    ImageStorage() = default;
    ~ImageStorage() = default;
    ImageStorage(const ImageStorage &) = delete;
    ImageStorage &operator=(const ImageStorage &) = delete;

    bool CreateTables();

    std::shared_ptr<SQLiteConnectionPool> _pool;
    std::mutex _mutex;
};

#endif