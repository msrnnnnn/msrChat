/**
 * @file SQLiteMgr.cpp
 * @brief SQLite 数据库管理实现 —— 连接池 + Repository 工厂
 * @details Phase 5D 重构后，业务方法已拆分到 AuthRepository / MessageRepository / SchemaManager。
 *          本文件仅保留连接池管理和 Repository 创建逻辑。
 */
#include "SQLiteMgr.h"
#include "AuthRepository.h"
#include "MessageRepository.h"
#include "SchemaManager.h"
#include <spdlog/spdlog.h>

SQLiteConnectionPool::SQLiteConnectionPool(const std::string &db_path, int pool_size)
    : _db_path(db_path),
      _pool_size(pool_size)
{
}

SQLiteConnectionPool::~SQLiteConnectionPool()
{
    Shutdown();
}

/**
 * @brief 初始化数据库连接
 * @param db 输出：sqlite3 指针引用
 * @return 初始化成功返回 true
 * @details 设置 WAL 模式和非同步级别
 */
bool SQLiteConnectionPool::InitializeConnection(sqlite3 **db)
{
    if (sqlite3_open(_db_path.c_str(), db) != SQLITE_OK)
    {
        spdlog::error("[SQLitePool] Failed to open database: {}", _db_path);
        return false;
    }

    char *err_msg = nullptr;

    if (sqlite3_exec(*db, "PRAGMA journal_mode=WAL;", nullptr, nullptr, &err_msg) != SQLITE_OK)
    {
        spdlog::error("[SQLitePool] Failed to set WAL mode: {}", err_msg ? err_msg : "unknown");
        if (err_msg)
            sqlite3_free(err_msg);
        sqlite3_close(*db);
        *db = nullptr;
        return false;
    }

    if (sqlite3_exec(*db, "PRAGMA synchronous=NORMAL;", nullptr, nullptr, &err_msg) != SQLITE_OK)
    {
        spdlog::error("[SQLitePool] Failed to set synchronous mode: {}", err_msg ? err_msg : "unknown");
        if (err_msg)
            sqlite3_free(err_msg);
        sqlite3_close(*db);
        *db = nullptr;
        return false;
    }

    sqlite3_busy_timeout(*db, 5000);  // 5 second busy timeout for concurrent access

    return true;
}

/**
 * @brief 从连接池获取一个连接
 * @return 可用连接，不可用时返回 nullptr
 * @details 条件变量阻塞等待，最多 30 秒；池关闭返回 nullptr
 */
std::shared_ptr<SQLiteConnection> SQLiteConnectionPool::Acquire()
{
    std::unique_lock<std::mutex> lock(_mutex);

    // 等待可用连接或池关闭，超时 30 秒
    if (!_cv.wait_for(lock, std::chrono::seconds(30),
                      [this] { return !_available_connections.empty() || _shutdown.load(); }))
    {
        spdlog::error("[SQLiteConnectionPool] Acquire timed out after 30s");
        return nullptr;
    }

    if (_shutdown.load())
    {
        return nullptr;
    }

    auto conn = _available_connections.front();
    _available_connections.pop();
    conn->SetInUse(true);

    return conn;
}

/**
 * @brief 归还连接到池
 * @param conn 要归还的连接
 * @details 标记未使用后放回可用队列，通知一个等待线程
 */
void SQLiteConnectionPool::Release(std::shared_ptr<SQLiteConnection> conn)
{
    if (!conn)
        return;

    std::lock_guard<std::mutex> lock(_mutex);
    conn->SetInUse(false);
    _available_connections.push(conn);
    _cv.notify_one();
}

/**
 * @brief 关闭连接池
 * @details 设置关闭标志、唤醒所有等待线程、关闭所有连接
 */
void SQLiteConnectionPool::Shutdown()
{
    std::lock_guard<std::mutex> lock(_mutex);

    if (_shutdown.load())
    {
        return;
    }

    _shutdown.store(true);
    _cv.notify_all();

    CloseAllConnections();

    _initialized.store(false);
}

/**
 * @brief 关闭所有连接
 */
void SQLiteConnectionPool::CloseAllConnections()
{
    for (auto &conn : _all_connections)
    {
        if (conn && conn->Get())
        {
            sqlite3_close(conn->Get());
        }
    }
    _all_connections.clear();

    while (!_available_connections.empty())
    {
        _available_connections.pop();
    }
}

SQLiteMgr::~SQLiteMgr()
{
    Shutdown();
}

SQLiteMgr &SQLiteMgr::Instance()
{
    static SQLiteMgr instance;
    return instance;
}

/**
 * @brief 初始化管理器
 * @param db_path 数据库文件路径
 * @param pool_size 连接池大小
 * @return 成功返回 true
 */
bool SQLiteMgr::Init(const std::string &db_path, int pool_size)
{
    if (_initialized.load())
    {
        return true;
    }

    _pool = std::make_shared<SQLiteConnectionPool>(db_path, pool_size);

    sqlite3 *db_temp = nullptr;
    if (!_pool->InitializeConnection(&db_temp))
    {
        return false;
    }

    if (!SchemaManager::CreateAllTables(db_temp))
    {
        sqlite3_close(db_temp);
        return false;
    }

    sqlite3_close(db_temp);

    // 批量创建连接并加入可用队列与全连接列表
    for (int i = 0; i < pool_size; ++i)
    {
        sqlite3 *db = nullptr;
        if (!_pool->InitializeConnection(&db))
        {
            spdlog::error("[SQLiteMgr] Failed to initialize connection {} in pool", i);
            continue;
        }

        if (!SchemaManager::CreateAllTables(db))
        {
            spdlog::error("[SQLiteMgr] Failed to setup tables for connection {} in pool", i);
            sqlite3_close(db);
            continue;
        }

        auto conn = std::make_shared<SQLiteConnection>(db);
        _pool->_available_connections.push(conn);
        _pool->_all_connections.push_back(conn);
    }

    if (_pool->_available_connections.empty())
    {
        spdlog::error("[SQLiteMgr] No connections available in pool");
        return false;
    }

    spdlog::info("[SQLiteMgr] Connection pool initialized with {} connections", _pool->_all_connections.size());

    // Phase 5D — 创建 Repository 实例
    _auth_repo = std::make_unique<AuthRepository>(_pool);
    _msg_repo = std::make_unique<MessageRepository>(_pool);

    _initialized.store(true);
    return true;
}

/**
 * @brief 关闭管理器，释放连接池和 Repository
 */
void SQLiteMgr::Shutdown()
{
    if (!_initialized.load())
    {
        return;
    }

    // Phase 5D — 先销毁 Repository（释放对 pool 的引用）
    _auth_repo.reset();
    _msg_repo.reset();

    if (_pool)
    {
        _pool->Shutdown();
    }

    _pool.reset();
    _initialized.store(false);
}

// ============================================================
// Phase 5D — Repository 访问器
// ============================================================

AuthRepository &SQLiteMgr::Auth()
{
    return *_auth_repo;
}

MessageRepository &SQLiteMgr::Messages()
{
    return *_msg_repo;
}
