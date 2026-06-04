#include "ImageStorage.h"
#include <spdlog/spdlog.h>
#include <ctime>
#include <cstring>

ImageStorage &ImageStorage::Instance()
{
    static ImageStorage inst;
    return inst;
}

bool ImageStorage::Init(std::shared_ptr<SQLiteConnectionPool> pool)
{
    std::lock_guard<std::mutex> lock(_mutex);
    _pool = pool;
    if (!_pool) return false;
    return CreateTables();
}

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

// Stub — Task 1.4 will implement
bool ImageStorage::Insert(const ImageRecord &) { return false; }
bool ImageStorage::AppendChunk(const std::string &, int64_t, const uint8_t *, size_t) { return false; }
bool ImageStorage::MarkCompleted(const std::string &) { return false; }
std::optional<ImageRecord> ImageStorage::Get(const std::string &) { return std::nullopt; }
bool ImageStorage::ReadRange(const std::string &, int64_t, int64_t, std::vector<uint8_t> &) { return false; }
bool ImageStorage::MarkRecalled(const std::string &) { return false; }
int ImageStorage::DeleteExpired(int64_t) { return 0; }