/**
 * @file ImageStorage.cpp
 * @brief 图片存储引擎实现
 * @details 基于 SQLite 的图片 Blob 存储，支持分片追加、范围读取、过期清理。
 */
#include "ImageStorage.h"
#include <spdlog/spdlog.h>
#include <ctime>

/**
 * @brief 获取单例实例
 * @return ImageStorage& 全局唯一实例
 */
ImageStorage &ImageStorage::Instance()
{
    static ImageStorage inst;
    return inst;
}

/**
 * @brief 初始化存储引擎
 * @param pool SQLite 连接池
 * @return 初始化成功返回 true
 * @details 线程安全初始化，建表失败返回 false
 */
bool ImageStorage::Init(std::shared_ptr<SQLiteConnectionPool> pool)
{
    std::lock_guard<std::mutex> lock(_mutex);
    _pool = pool;
    if (!_pool) return false;
    return CreateTables();
}

/**
 * @brief 创建图片存储表与索引
 * @return 建表成功返回 true
 * @details image_storage 表用 image_id 作为主键，blob 字段存完整图片数据
 */
bool ImageStorage::CreateTables()
{
    SQLiteConnectionGuard guard(_pool);
    if (!guard) return false;
    sqlite3 *db = guard.Get();
    const char *sql = R"SQL(
        CREATE TABLE IF NOT EXISTS image_storage (
            image_id   TEXT PRIMARY KEY,
            from_uid   INTEGER NOT NULL,
            to_uid     INTEGER NOT NULL,
            ext        TEXT NOT NULL,
            size       INTEGER NOT NULL,
            md5        TEXT NOT NULL,
            width      INTEGER NOT NULL,
            height     INTEGER NOT NULL,
            created_at INTEGER NOT NULL,
            expires_at INTEGER NOT NULL,
            recalled   INTEGER DEFAULT 0,
            blob       BLOB NOT NULL
        );
        CREATE INDEX IF NOT EXISTS idx_image_expires ON image_storage(expires_at);
    )SQL";
    char *err = nullptr;
    int rc = sqlite3_exec(db, sql, nullptr, nullptr, &err);
    if (rc != SQLITE_OK) {
        spdlog::error("ImageStorage::CreateTables: {}", err ? err : "");
        if (err) sqlite3_free(err);
        return false;
    }
    return true;
}

/**
 * @brief 插入图片元数据行
 * @param rec 图片记录（不含 blob 数据，blob 初始为空）
 * @return 插入成功返回 true
 * @details 使用 INSERT OR REPLACE 避免 image_id 冲突
 */
bool ImageStorage::Insert(const ImageRecord &rec)
{
    SQLiteConnectionGuard guard(_pool);
    if (!guard) return false;
    sqlite3 *db = guard.Get();
    const char *sql = R"SQL(
        INSERT OR REPLACE INTO image_storage
            (image_id, from_uid, to_uid, ext, size, md5, width, height,
             created_at, expires_at, recalled, blob)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, 0, X'');
    )SQL";
    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
    sqlite3_bind_text(stmt, 1, rec.image_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 2, rec.from_uid);
    sqlite3_bind_int(stmt, 3, rec.to_uid);
    sqlite3_bind_text(stmt, 4, rec.ext.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 5, rec.size);
    sqlite3_bind_text(stmt, 6, rec.md5.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 7, rec.width);
    sqlite3_bind_int(stmt, 8, rec.height);
    sqlite3_bind_int64(stmt, 9, rec.created_at);
    sqlite3_bind_int64(stmt, 10, rec.expires_at);
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return rc == SQLITE_DONE;
}

/**
 * @brief 分片追加图片二进制数据
 * @param image_id 图片唯一 ID
 * @param offset 写入偏移量（字节）
 * @param data 数据指针
 * @param len 数据长度
 * @return 追加成功返回 true
 * @details 先读取已有 blob，在内存中追加分片后写回；
 *          若 offset 大于已有大小，中间部分填零补齐
 */
bool ImageStorage::AppendChunk(const std::string &image_id, int64_t offset,
                               const uint8_t *data, size_t len)
{
    if (!data || len == 0) return false;
    SQLiteConnectionGuard guard(_pool);
    if (!guard) return false;
    sqlite3 *db = guard.Get();

    // 1. 读取当前 blob
    const char *select_sql = "SELECT blob FROM image_storage WHERE image_id = ?;";
    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(db, select_sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
    sqlite3_bind_text(stmt, 1, image_id.c_str(), -1, SQLITE_TRANSIENT);

    std::vector<uint8_t> full_blob;
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        const void *blob = sqlite3_column_blob(stmt, 0);
        int blob_size = sqlite3_column_bytes(stmt, 0);
        if (blob && blob_size > 0)
        {
            full_blob.assign(static_cast<const uint8_t *>(blob),
                             static_cast<const uint8_t *>(blob) + blob_size);
        }
    }
    sqlite3_finalize(stmt);

    // 2. 在内存中追加数据
    if (static_cast<int64_t>(full_blob.size()) < offset)
    {
        full_blob.resize(static_cast<size_t>(offset), 0);  // zero-fill gap
    }
    full_blob.insert(full_blob.begin() + offset, data, data + len);

    // 3. 写回完整 blob
    const char *update_sql = "UPDATE image_storage SET blob = ? WHERE image_id = ?;";
    if (sqlite3_prepare_v2(db, update_sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
    sqlite3_bind_blob(stmt, 1, full_blob.data(), static_cast<int>(full_blob.size()), SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, image_id.c_str(), -1, SQLITE_TRANSIENT);

    int rc = sqlite3_step(stmt);
    int changes = sqlite3_changes(db);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE || changes == 0)
    {
        spdlog::error("[ImageStorage] AppendChunk: write-back failed rc={} changes={} image_id={}",
                      rc, changes, image_id);
        return false;
    }
    spdlog::info("[ImageStorage] AppendChunk: ok image_id={} offset={} len={} blob_total={}",
                 image_id, offset, len, full_blob.size());
    return true;
}

/**
 * @brief 标记图片传输完成，设置过期时间
 * @param image_id 图片唯一 ID
 * @return 更新成功返回 true
 * @details 将 expires_at 设为当前时间 + 7 天
 */
bool ImageStorage::MarkCompleted(const std::string &image_id)
{
    SQLiteConnectionGuard guard(_pool);
    if (!guard) return false;
    sqlite3 *db = guard.Get();
    const char *sql = "UPDATE image_storage SET expires_at = ? WHERE image_id = ?;";
    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
    sqlite3_bind_int64(stmt, 1, std::time(nullptr) + 7 * 24 * 3600);
    sqlite3_bind_text(stmt, 2, image_id.c_str(), -1, SQLITE_TRANSIENT);
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return rc == SQLITE_DONE;
}

/**
 * @brief 查询图片元数据（不含 blob）
 * @param image_id 图片唯一 ID
 * @return 存在则返回 ImageRecord，否则返回 std::nullopt
 */
std::optional<ImageRecord> ImageStorage::Get(const std::string &image_id)
{
    SQLiteConnectionGuard guard(_pool);
    if (!guard) return std::nullopt;
    sqlite3 *db = guard.Get();
    const char *sql =
        "SELECT image_id, from_uid, to_uid, ext, size, md5, width, height, "
        "created_at, expires_at, recalled FROM image_storage WHERE image_id = ?;";
    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return std::nullopt;
    sqlite3_bind_text(stmt, 1, image_id.c_str(), -1, SQLITE_TRANSIENT);
    std::optional<ImageRecord> result;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        ImageRecord rec;
        rec.image_id    = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        rec.from_uid   = sqlite3_column_int(stmt, 1);
        rec.to_uid     = sqlite3_column_int(stmt, 2);
        rec.ext        = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        rec.size       = sqlite3_column_int64(stmt, 4);
        rec.md5        = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
        rec.width      = sqlite3_column_int(stmt, 6);
        rec.height     = sqlite3_column_int(stmt, 7);
        rec.created_at = sqlite3_column_int64(stmt, 8);
        rec.expires_at = sqlite3_column_int64(stmt, 9);
        rec.recalled   = sqlite3_column_int(stmt, 10) != 0;
        result = rec;
    }
    sqlite3_finalize(stmt);
    return result;
}

/**
 * @brief 按偏移量读取图片 blob 片段
 * @param image_id 图片唯一 ID
 * @param offset 读取起始偏移（字节）
 * @param size 读取长度
 * @param out 输出缓冲区
 * @return 读取成功返回 true
 * @details 边界检查：offset + size 不能超过 blob 实际大小
 */
bool ImageStorage::ReadRange(const std::string &image_id, int64_t offset, int64_t size,
                              std::vector<uint8_t> &out)
{
    SQLiteConnectionGuard guard(_pool);
    if (!guard) return false;
    sqlite3 *db = guard.Get();
    const char *sql = "SELECT blob FROM image_storage WHERE image_id = ?;";
    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
    sqlite3_bind_text(stmt, 1, image_id.c_str(), -1, SQLITE_TRANSIENT);

    bool ok = false;
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        const void *blob = sqlite3_column_blob(stmt, 0);
        int blob_size = sqlite3_column_bytes(stmt, 0);
        spdlog::info("[ImageStorage] ReadRange: image_id={}, offset={}, size={}, blob_size={}",
                     image_id, offset, size, blob_size);
        if (blob && blob_size > 0 && offset >= 0 && offset + size <= blob_size)
        {
            const auto *base = static_cast<const uint8_t *>(blob);
            out.assign(base + offset, base + offset + size);
            ok = true;
        }
        else
        {
            spdlog::error("[ImageStorage] ReadRange: invalid range blob_size={} offset={} size={}",
                          blob_size, offset, size);
        }
    }
    else
    {
        spdlog::error("[ImageStorage] ReadRange: no row found for {}", image_id);
    }
    sqlite3_finalize(stmt);
    return ok;
}

/**
 * @brief 标记图片已被撤回
 * @param image_id 图片唯一 ID
 * @return 更新成功返回 true
 */
bool ImageStorage::MarkRecalled(const std::string &image_id)
{
    SQLiteConnectionGuard guard(_pool);
    if (!guard) return false;
    sqlite3 *db = guard.Get();
    const char *sql = "UPDATE image_storage SET recalled = 1 WHERE image_id = ?;";
    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
    sqlite3_bind_text(stmt, 1, image_id.c_str(), -1, SQLITE_TRANSIENT);
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return rc == SQLITE_DONE;
}

/**
 * @brief 清理已过期且已撤回的图片记录
 * @param now 当前时间戳（秒）
 * @return 删除的记录数
 */
int ImageStorage::DeleteExpired(int64_t now)
{
    SQLiteConnectionGuard guard(_pool);
    if (!guard) return 0;
    sqlite3 *db = guard.Get();
    const char *sql = "DELETE FROM image_storage WHERE expires_at < ? AND recalled = 1;";
    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return 0;
    sqlite3_bind_int64(stmt, 1, now);
    sqlite3_step(stmt);
    int changes = sqlite3_changes(db);
    sqlite3_finalize(stmt);
    return changes;
}