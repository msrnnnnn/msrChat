#pragma once
/**
 * @file SchemaManager.h
 * @brief 数据库 Schema 管理 —— 建表 + 多阶段迁移
 * @details 从 SQLiteMgr 拆分（Phase 5D），负责所有 DDL 操作。
 */
#ifndef SCHEMA_MANAGER_H
#define SCHEMA_MANAGER_H

#include <sqlite3.h>

class SchemaManager
{
public:
    /// 创建所有表 + 执行向后兼容迁移
    /// @param db 已打开的 SQLite 连接
    /// @return 成功返回 true
    static bool CreateAllTables(sqlite3 *db);
};

#endif // SCHEMA_MANAGER_H
