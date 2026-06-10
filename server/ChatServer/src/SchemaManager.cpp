/**
 * @file SchemaManager.cpp
 * @brief 数据库 Schema 管理实现
 * @details 从 SQLiteMgr.cpp 拆分（Phase 5D），包含建表 SQL 和多阶段迁移逻辑。
 */
#include "SchemaManager.h"
#include <spdlog/spdlog.h>
#include <string>

bool SchemaManager::CreateAllTables(sqlite3 *db)
{
    const char *sql = R"(
        CREATE TABLE IF NOT EXISTS users (
            uid INTEGER PRIMARY KEY AUTOINCREMENT,
            username TEXT UNIQUE NOT NULL,
            password_hash TEXT NOT NULL,
            email TEXT DEFAULT '',
            avatar_path TEXT DEFAULT '',
            created_at INTEGER NOT NULL
        );
        
        CREATE TABLE IF NOT EXISTS messages (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            from_uid INTEGER NOT NULL,
            to_uid INTEGER NOT NULL,
            content TEXT NOT NULL,
            timestamp INTEGER NOT NULL,
            status INTEGER DEFAULT 0,
            client_msg_id TEXT DEFAULT '',
            UNIQUE(from_uid, to_uid, timestamp)
        );
        
        CREATE INDEX IF NOT EXISTS idx_messages_search 
            ON messages(from_uid, to_uid, timestamp DESC);
        
        CREATE INDEX IF NOT EXISTS idx_messages_keyword 
            ON messages(from_uid, to_uid);
        
        CREATE TABLE IF NOT EXISTS offline_messages (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            from_uid INTEGER NOT NULL,
            to_uid INTEGER NOT NULL,
            content TEXT NOT NULL,
            timestamp INTEGER NOT NULL,
            status INTEGER DEFAULT 0,
            client_msg_id TEXT DEFAULT '',
            type INTEGER DEFAULT 0,
            image_id TEXT DEFAULT ''
        );
        
        CREATE TABLE IF NOT EXISTS verify_codes (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            email TEXT NOT NULL,
            code TEXT NOT NULL,
            created_at INTEGER NOT NULL,
            expires_at INTEGER NOT NULL
        );

        CREATE TABLE IF NOT EXISTS tokens (
            uid INTEGER PRIMARY KEY,
            token TEXT NOT NULL,
            created_at INTEGER NOT NULL
        );

        CREATE TABLE IF NOT EXISTS recall_notify_queue (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            uid INTEGER NOT NULL,
            msg_timestamp INTEGER NOT NULL,
            recall_uid INTEGER NOT NULL,
            recall_ts INTEGER NOT NULL,
            recalled_to INTEGER NOT NULL
        );
    )";

    char *err_msg = nullptr;
    if (sqlite3_exec(db, sql, nullptr, nullptr, &err_msg) != SQLITE_OK)
    {
        if (err_msg)
        {
            sqlite3_free(err_msg);
        }
        return false;
    }

    // Phase 2 — 向后兼容迁移：users 表增加 email 列（ignore duplicate column）
    const char *migration_sql = "ALTER TABLE users ADD COLUMN email TEXT DEFAULT ''";
    char *mig_err = nullptr;
    int mig_rc = sqlite3_exec(db, migration_sql, nullptr, nullptr, &mig_err);
    if (mig_rc != SQLITE_OK && mig_err)
    {
        std::string err_str(mig_err);
        sqlite3_free(mig_err);
        if (err_str.find("duplicate column") == std::string::npos)
        {
            spdlog::warn("[SchemaManager] Migration ALTER TABLE users failed: {}", err_str);
        }
    }

    const char *offline_migration_sql = "ALTER TABLE offline_messages ADD COLUMN client_msg_id TEXT DEFAULT ''";
    mig_rc = sqlite3_exec(db, offline_migration_sql, nullptr, nullptr, &mig_err);
    if (mig_rc != SQLITE_OK && mig_err)
    {
        std::string err_str(mig_err);
        sqlite3_free(mig_err);
        if (err_str.find("duplicate column") == std::string::npos)
        {
            spdlog::warn("[SchemaManager] Migration ALTER TABLE offline_messages failed: {}", err_str);
        }
    }

    // === Phase 7 — messages 表增 6 列（recall/edit + type/image_id）===
    const char *p7_migrations[] = {
        "ALTER TABLE messages ADD COLUMN type INTEGER DEFAULT 0",
        "ALTER TABLE messages ADD COLUMN image_id TEXT DEFAULT ''",
        "ALTER TABLE messages ADD COLUMN recalled INTEGER DEFAULT 0",
        "ALTER TABLE messages ADD COLUMN recalled_at INTEGER DEFAULT 0",
        "ALTER TABLE messages ADD COLUMN edited INTEGER DEFAULT 0",
        "ALTER TABLE messages ADD COLUMN edited_at INTEGER DEFAULT 0"
    };
    for (const char *p7_sql : p7_migrations)
    {
        char *p7_err = nullptr;
        int p7_rc = sqlite3_exec(db, p7_sql, nullptr, nullptr, &p7_err);
        if (p7_rc != SQLITE_OK && p7_err)
        {
            std::string p7_err_str(p7_err);
            sqlite3_free(p7_err);
            if (p7_err_str.find("duplicate column") == std::string::npos)
            {
                spdlog::warn("[SchemaManager] P7 migration failed: {} (err={})", p7_sql, p7_err_str);
            }
        }
    }

    // === Phase 2 — messages 表增 client_msg_id 列 + 去重索引 ===
    {
        const char *c2_sql = "ALTER TABLE messages ADD COLUMN client_msg_id TEXT DEFAULT ''";
        char *c2_err = nullptr;
        int c2_rc = sqlite3_exec(db, c2_sql, nullptr, nullptr, &c2_err);
        if (c2_rc != SQLITE_OK && c2_err)
        {
            std::string c2_err_str(c2_err);
            sqlite3_free(c2_err);
            if (c2_err_str.find("duplicate column") == std::string::npos)
            {
                spdlog::warn("[SchemaManager] Phase 2 migration failed: {} (err={})", c2_sql, c2_err_str);
            }
        }
    }
    {
        const char *idx_sql =
            "CREATE UNIQUE INDEX IF NOT EXISTS idx_messages_client_msg_id "
            "ON messages(client_msg_id) WHERE client_msg_id != ''";
        char *idx_err = nullptr;
        sqlite3_exec(db, idx_sql, nullptr, nullptr, &idx_err);
        if (idx_err)
        {
            spdlog::warn("[SchemaManager] Phase 2 index creation failed: {}", idx_err);
            sqlite3_free(idx_err);
        }
    }

    // === Phase 4 — 离线消息查询索引 ===
    {
        const char *oidx_sql =
            "CREATE INDEX IF NOT EXISTS idx_offline_to_uid "
            "ON offline_messages(to_uid, id)";
        char *oidx_err = nullptr;
        sqlite3_exec(db, oidx_sql, nullptr, nullptr, &oidx_err);
        if (oidx_err)
        {
            spdlog::warn("[SchemaManager] Phase 4 offline index creation failed: {}", oidx_err);
            sqlite3_free(oidx_err);
        }
    }

    // === Phase D — offline_messages 表增 type + image_id 列 ===
    const char *pd_migrations[] = {
        "ALTER TABLE offline_messages ADD COLUMN type INTEGER DEFAULT 0",
        "ALTER TABLE offline_messages ADD COLUMN image_id TEXT DEFAULT ''"
    };
    for (const char *pd_sql : pd_migrations)
    {
        char *pd_err = nullptr;
        int pd_rc = sqlite3_exec(db, pd_sql, nullptr, nullptr, &pd_err);
        if (pd_rc != SQLITE_OK && pd_err)
        {
            std::string pd_err_str(pd_err);
            sqlite3_free(pd_err);
            if (pd_err_str.find("duplicate column") == std::string::npos)
            {
                spdlog::warn("[SchemaManager] PD migration failed: {} (err={})", pd_sql, pd_err_str);
            }
        }
    }

    return true;
}
