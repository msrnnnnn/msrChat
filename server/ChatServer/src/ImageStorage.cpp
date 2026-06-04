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

bool ImageStorage::Insert(const ImageRecord &rec)
{
    SQLiteConnectionGuard guard(_pool);
    if (!guard) return false;
    sqlite3 *db = guard.Get();
    const char *sql = R"SQL(
        INSERT OR REPLACE INTO image_storage
            (image_id, from_uid, to_uid, ext, size, md5, width, height,
             created_at, expires_at, recalled, blob)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, 0, '');
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

bool ImageStorage::AppendChunk(const std::string &, int64_t, const uint8_t *, size_t) { return false; }

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

bool ImageStorage::ReadRange(const std::string &, int64_t, int64_t, std::vector<uint8_t> &) { return false; }

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