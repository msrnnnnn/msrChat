# 私聊功能代码中的死锁分析

## 📌 问题概述

这段代码在高并发场景下会发生**死锁**，原因是不合理的 **FOR UPDATE 查询** 结合后续的 **INSERT 操作**，导致间隙锁、插入意向锁之间的冲突。

## 表结构

### private_chat表

![image-20251126232246544](https://cdn.llfc.club/img/image-20251126232246544.png)

### chat_thread表

![image-20251126232625472](https://cdn.llfc.club/img/image-20251126232625472.png)

**聊天界面**

![image.png](https://cdn.nlark.com/yuque/0/2025/png/27211935/1763872788732-cfefc3eb-be45-43b1-989a-db567fe67262.png?x-oss-process=image%2Fwatermark%2Ctype_d3F5LW1pY3JvaGVp%2Csize_30%2Ctext_5oGL5oGL6aOO6L6wemFjaw%3D%3D%2Ccolor_FFFFFF%2Cshadow_50%2Ct_80%2Cg_se%2Cx_10%2Cy_10%2Fformat%2Cwebp)



## 📌死锁源码

``` cpp
bool MysqlDao::CreatePrivateChat(int user1_id, int user2_id, int& thread_id)
{
	auto con = pool_->getConnection();
	if (!con) {
		return false;
	}
	Defer defer([this, &con]() {
		pool_->returnConnection(std::move(con));
		});
	auto& conn = con->_con;
	try {
		// 开启事务
		conn->setAutoCommit(false);
		// 1. 查询是否已存在私聊并加行级锁
		int uid1 = std::min(user1_id, user2_id);
		int uid2 = std::max(user1_id, user2_id);
		std::string check_sql =
			"SELECT thread_id FROM private_chat "
			"WHERE (user1_id = ? AND user2_id = ?) "
			"FOR UPDATE;";

		std::unique_ptr<sql::PreparedStatement> pstmt(conn->prepareStatement(check_sql));
		pstmt->setInt64(1, uid1);
		pstmt->setInt64(2, uid2);
		std::unique_ptr<sql::ResultSet> res(pstmt->executeQuery());

		if (res->next()) {
			// 如果已存在，返回该 thread_id
			thread_id = res->getInt("thread_id");
			conn->commit();  // 提交事务
			return true;
		}

		// 2. 如果未找到，创建新的 chat_thread 和 private_chat 记录
		// 在 chat_thread 表插入新记录
		std::string insert_chat_thread_sql =
			"INSERT INTO chat_thread (type, created_at) VALUES ('private', NOW());";

		std::unique_ptr<sql::PreparedStatement> pstmt_insert_thread(conn->prepareStatement(insert_chat_thread_sql));
		pstmt_insert_thread->executeUpdate();

		// 获取新插入的 thread_id
		std::string get_last_insert_id_sql = "SELECT LAST_INSERT_ID();";
		std::unique_ptr<sql::PreparedStatement> pstmt_last_insert_id(conn->prepareStatement(get_last_insert_id_sql));
		std::unique_ptr<sql::ResultSet> res_last_id(pstmt_last_insert_id->executeQuery());
		res_last_id->next();
		thread_id = res_last_id->getInt(1);

		// 3. 在 private_chat 表插入新记录
		std::string insert_private_chat_sql =
			"INSERT INTO private_chat (thread_id, user1_id, user2_id, created_at) "
			"VALUES (?, ?, ?, NOW());";


		std::unique_ptr<sql::PreparedStatement> pstmt_insert_private(conn->prepareStatement(insert_private_chat_sql));
		pstmt_insert_private->setInt64(1, thread_id);
		pstmt_insert_private->setInt64(2, uid1);
		pstmt_insert_private->setInt64(3, uid2);
		pstmt_insert_private->executeUpdate();

		// 提交事务
		conn->commit();
		return true;
	}
	catch (sql::SQLException& e) {
		std::cerr << "SQLException: " << e.what() << std::endl;
		conn->rollback();
		return false;
	}
	return false;
}
```

![image-20251204130749212](https://cdn.llfc.club/img/image-20251204130749212.png)

## InnoDB 的四种主要锁类型

### 按粒度分类

MySQL InnoDB存储引擎中这四种重要的锁类型：

> **记录锁（Record Lock）**
>
> 记录锁是最基本的行锁类型，它锁定的是索引记录本身。当你使用唯一索引或主键进行等值查询并且找到记录时，InnoDB会使用记录锁。
>
> 例如：`SELECT * FROM users WHERE id = 10 FOR UPDATE`，这会在id=10的索引记录上加记录锁。
>
> **临键锁（Next-Key Lock）**
>
> 临键锁是记录锁和间隙锁的组合，锁定了一个范围，包括记录本身。这是InnoDB默认的行锁算法，主要用于解决幻读问题。
>
> 临键锁的范围是左开右闭区间，比如索引值为10, 20, 30的记录，临键锁可能是：
>
> - (-∞, 10]
> - (10, 20]
> - (20, 30]
> - (30, +∞)
>
> 当进行范围查询时，InnoDB通常会使用临键锁来锁定扫描到的索引范围，防止其他事务在这个范围内插入新记录
>
> **插入意向锁（Insert Intention Lock）**
>
> 插入意向锁是一种特殊的间隙锁，在INSERT操作之前设置。它的特点是：
>
> 1. **不会阻塞其他插入意向锁**：多个事务可以同时在同一个间隙中获取插入意向锁，只要它们插入的位置不同
> 2. **会被间隙锁和临键锁阻塞**：如果间隙已经被其他事务的间隙锁或临键锁锁定，插入操作会等待
> 3. **提高并发性**：允许多个事务并发地向同一间隙插入不同的记录
>
> 例如，如果索引中有记录10和20，两个事务可以同时在(10, 20)这个间隙中插入不同的值（如12和15），因为插入意向锁之间不冲突。
>
> 这三种锁机制共同协作，在保证数据一致性的同时，尽可能提高并发性能。
>
>  **间隙锁 (Gap Lock)**                            │
>      • 锁定索引记录之间的间隙                       │
>      • 不包括记录本身                               │
>      • 例如: 记录3和记录7之间的空隙   

### 临键锁如何工作

让我用具体的例子来解释临键锁是如何"组合"记录锁和间隙锁的。

**先理解间隙锁（Gap Lock）**

间隙锁锁定的是**两个索引记录之间的间隙**,不包括记录本身。它的作用是防止其他事务在这个间隙中插入新记录。

**临键锁的组合特性**

假设有一张表，索引列有值：10, 20, 30

```
索引值:    10        20        30
间隙:   (∞,10)  (10,20)  (20,30)  (30,∞)
```

**临键锁 = 间隙锁 + 记录锁**，具体来说：

临键锁 **(10, 20]** 意味着：

- **间隙锁部分**：锁定 (10, 20) 这个开区间，防止插入11-19的记录
- **记录锁部分**：锁定值为20的记录本身

**实际场景举例**

```
-- 假设表中有记录: id = 10, 20, 30
SELECT * FROM users WHERE id >= 20 FOR UPDATE;
```

这个查询会加临键锁：

- **(10, 20]** - 锁住间隙(10,20)和记录20
- **(20, 30]** - 锁住间隙(20,30)和记录30
- **(30, +∞)** - 锁住30之后的所有间隙

**为什么要这样组合？**

这种组合设计巧妙地解决了**幻读问题**：

```mysql
-- 事务A
SELECT * FROM users WHERE id >= 20 FOR UPDATE;
-- 结果：找到id=20, 30两条记录

-- 如果只有记录锁，事务B可以：
INSERT INTO users VALUES (25, 'New');  -- 成功插入！

-- 事务A再次查询
SELECT * FROM users WHERE id >= 20 FOR UPDATE;
-- 结果：找到id=20, 25, 30三条记录 ← 出现幻读！
```

但有了临键锁的**间隙锁部分**，事务B的插入会被阻塞，因为25落在了(20,30]的间隙范围内。

**总结理解**

**临键锁 = 锁住一个范围的两个部分**：

1. **记录锁部分**：锁住右边界的那条记录（比如20）
2. **间隙锁部分**：锁住左边界到右边界之间的间隙（比如10到20之间）

这样既保护了记录本身不被修改，又保护了间隙不被插入新记录，从而在可重复读隔离级别下避免幻读。

### 按类型分类

> 共享锁（Shared Lock，S锁）- 读锁，允许多个事务同时持有
>
> 排他锁（Exclusive Lock，X锁）- 写锁，独占访问

### 它们的组合关系

**记录锁可以是共享锁，也可以是排他锁**：

```mysql
-- 记录锁 + 共享锁
SELECT * FROM users WHERE id = 10 FOR SHARE;
-- 在id=10的记录上加"共享记录锁"

-- 记录锁 + 排他锁
SELECT * FROM users WHERE id = 10 FOR UPDATE;
-- 在id=10的记录上加"排他记录锁"

UPDATE users SET name = 'John' WHERE id = 10;
-- 也是加"排他记录锁"
```

### 形象的比喻

把**记录锁**想象成一个**房间**，而**排他锁/共享锁**是**门锁的类型**：

- **共享锁（S锁）**：像"阅览室"，多个人可以同时进来看书（多个事务可以同时读）
- **排他锁（X锁）**：像"编辑室"，只能一个人进去修改文档（只有一个事务可以写，其他读写都被阻塞）

### 完整的描述方式

所以准确的说法应该是：

- "在这条记录上加了**排他**的记录锁"
- "在这条记录上加了**共享**的记录锁"

## 🔑 间隙锁的特性

```
特性1: 间隙锁之间相互兼容
┌──────────────────────────────────┐
│  事务A: 持有 Gap(1,5)            │
│  事务B: 可以获取 Gap(1,5) ✅     │
│  事务C: 可以获取 Gap(1,5) ✅     │
└──────────────────────────────────┘

特性2: 间隙锁阻止插入
┌──────────────────────────────────┐
│  事务A: 持有 Gap(1,5)            │
│  事务B: INSERT id=3 ❌           │
│         (需要插入意向锁,被阻塞)  │
└──────────────────────────────────┘

特性3: 仅在 RR 隔离级别下生效
┌──────────────────────────────────┐
│  REPEATABLE READ: 有间隙锁 ✅    │
│  READ COMMITTED:  无间隙锁 ✅    │
└──────────────────────────────────┘
```

## 🔑 插入意向锁的特性

```markdown
特性1: 与间隙锁互斥
┌──────────────────────────────────┐
│  事务A: 持有 Gap Lock(1,5)       │
│  事务B: INSERT id=3              │
│         ↓                        │
│         需要 Insert Intention    │
│         ❌ 被A的间隙锁阻塞       │
└──────────────────────────────────┘

特性2: 插入意向锁之间可以兼容(插入不同行时)
┌──────────────────────────────────┐
│  事务A: INSERT id=2 (等待中)     │
│  事务B: INSERT id=4 (等待中)     │
│         ↓                        │
│         如果没有间隙锁阻塞       │
│         两者可以并发执行 ✅      │
└──────────────────────────────────┘
```

### 问题代码结构

```cpp
bool CreatePrivateChat(int user1_id, int user2_id, int& thread_id)
{
    conn->setAutoCommit(false);  // 开启事务
    
    // ⚠️ 第一步: SELECT ... FOR UPDATE
    // 如果未找到记录 → 获取间隙锁
    std::string check_sql =
        "SELECT thread_id FROM private_chat "
        "WHERE (user1_id = ? AND user2_id = ?) "
        "FOR UPDATE;";  
    
    if (res->next()) {
        // 找到记录,返回
        thread_id = res->getInt("thread_id");
        conn->commit();
        return true;
    }
    
    // ⚠️ 第二步: INSERT 新记录
    // 需要获取插入意向锁 → 与间隙锁冲突
    std::string insert_sql =
        "INSERT INTO private_chat "
        "(thread_id, user1_id, user2_id, created_at) "
        "VALUES (?, ?, ?, NOW());";
    
    conn->commit();
}
```

### 表结构假设

```
CREATE TABLE private_chat (
    thread_id INT,
    user1_id INT,
    user2_id INT,
    created_at DATETIME,
    INDEX idx_users (user1_id, user2_id)  -- 复合索引
);

-- 当前数据
┌──────────┬──────────┬──────────┐
│ user1_id │ user2_id │thread_id │
├──────────┼──────────┼──────────┤
│    1     │    3     │    10    │
│    2     │    4     │    20    │
│    5     │    8     │    30    │
└──────────┴──────────┴──────────┘

-- 索引树的间隙
Gap: (-∞, (1,3))
Gap: ((1,3), (2,4))  ← 死锁发生的间隙
Gap: ((2,4), (5,8))
Gap: ((5,8), +∞)
```

------

## 第三部分:死锁发生的完整流程

### 并发场景

**两个线程同时调用**: `CreatePrivateChat(1, 2, thread_id)`

```
┌─────────────────────────────────────────────────────┐
│  并发请求:                                          │
│  • 线程1: CreatePrivateChat(1, 2, thread_id_A)      │
│  • 线程2: CreatePrivateChat(1, 2, thread_id_B)      │
│                                                     │
│  目标: 创建 user1_id=1, user2_id=2 的私聊          │
│  索引位置: 落在间隙 ((1,3), (2,4)) 之间            │
└─────────────────────────────────────────────────────┘
```

### 详细时间线

```
时间    事务 A                              事务 B
────────────────────────────────────────────────────────────
T0      BEGIN                              BEGIN
        ↓                                  ↓

T1      SELECT ... FOR UPDATE              
        WHERE user1_id=1 AND user2_id=2    (等待中)
        ↓                                  
        检查索引: (1,2)                    
        ↓                                  
        未找到记录                         
        ↓                                  

T2      获取间隙锁 Gap((1,3), (2,4)) ✅    SELECT ... FOR UPDATE
        (锁定查询区间)                     WHERE user1_id=1 AND user2_id=2
                                          ↓
                                          检查索引: (1,2)
                                          ↓
                                          未找到记录
                                          ↓

T3      准备 INSERT                        获取间隙锁 Gap((1,3), (2,4)) ✅
        thread_id = 100                    ⚠️ 间隙锁兼容! 成功获取!
        ↓                                  ↓

T4      INSERT INTO private_chat           准备 INSERT
        VALUES (100, 1, 2, NOW())          thread_id = 101
        ↓                                  ↓
        需要插入意向锁                     
        Insert Intention Lock              
        ↓                                  

T5      检测到 B 持有 Gap Lock             INSERT INTO private_chat
        ❌ 插入意向锁与间隙锁冲突          VALUES (101, 1, 2, NOW())
        ⏳ 等待 B 释放间隙锁...            ↓
                                          需要插入意向锁
                                          Insert Intention Lock
                                          ↓

T6      (继续等待)                         检测到 A 持有 Gap Lock
        持有: Gap Lock                     ❌ 插入意向锁与间隙锁冲突
        等待: B 释放 Gap Lock              ⏳ 等待 A 释放间隙锁...
                                          ↓
                                          持有: Gap Lock
                                          等待: A 释放 Gap Lock

T7      💀 DEADLOCK DETECTED!
        ─────────────────────────────────────────────
        MySQL 检测到循环等待:
        • A 持有间隙锁, 等待 B 释放间隙锁
        • B 持有间隙锁, 等待 A 释放间隙锁
        ─────────────────────────────────────────────
        ↓
        回滚事务 B (选择代价较小的事务)
        ↓
        事务 A 继续执行
        INSERT 成功 ✅
        COMMIT ✅
```

------

## 第四部分:死锁的核心机制

### 锁兼容性矩阵

```
┌───────────────────────────────────────────────────┐
│         锁类型兼容性(RR隔离级别)                   │
├─────────────┬────────┬──────────┬───────────────┤
│  持有\请求  │ 间隙锁 │ 记录锁   │ 插入意向锁    │
├─────────────┼────────┼──────────┼───────────────┤
│  间隙锁     │ ✅兼容 │ ✅兼容   │ ❌ 冲突       │
│  记录锁     │ ✅兼容 │ ❌ 冲突  │ ❌ 冲突       │
│  插入意向锁 │ ❌冲突 │ ❌ 冲突  │ ✅兼容(不同行)│
└─────────────┴────────┴──────────┴───────────────┘

⚠️ 核心规则:
• 间隙锁 + 间隙锁 = ✅ 兼容(可以同时持有)
• 间隙锁 + 插入意向锁 = ❌ 冲突(互相阻塞)
```

### 死锁的四个必要条件

```
┌──────────────────────────────────────────────┐
│         本案例中的死锁条件分析                │
├──────────────────────────────────────────────┤
│                                              │
│  1️⃣  互斥条件 ✅                             │
│      插入意向锁 与 间隙锁 互斥               │
│                                              │
│  2️⃣  持有并等待 ✅                           │
│      A 持有间隙锁, 等待获取插入意向锁        │
│      B 持有间隙锁, 等待获取插入意向锁        │
│                                              │
│  3️⃣  不可剥夺 ✅                             │
│      间隙锁只能在事务提交/回滚时释放         │
│                                              │
│  4️⃣  循环等待 ✅                             │
│      A 等待 B → B 等待 A                     │
│                                              │
└──────────────────────────────────────────────┘
```

### 可视化流程图

```
┌────────────────────────────────────────────────────┐
│              死锁形成的循环依赖图                   │
└────────────────────────────────────────────────────┘

        事务 A                      事务 B
          │                           │
          │                           │
     [持有间隙锁]                [持有间隙锁]
      Gap(1,3)~(2,4)             Gap(1,3)~(2,4)
          │                           │
          │                           │
          ▼                           ▼
    需要插入意向锁                需要插入意向锁
    Insert Intention              Insert Intention
          │                           │
          │◄─────────┐   ┌───────────►│
          │          │   │            │
          │       等待B释放│ 等待A释放  │
          │        间隙锁 │  间隙锁    │
          │          │   │            │
          └──────────┘   └────────────┘
                💀 循环等待


解释:
• 两个事务都在 T2-T3 成功获取了间隙锁(兼容)
• 两个事务都在 T5-T6 尝试获取插入意向锁
• 插入意向锁被对方的间隙锁阻塞
• 形成 A→B→A 的循环依赖
```

------

## 第五部分:为什么会发生死锁?

### 根本原因分析

```
┌─────────────────────────────────────────────────┐
│           死锁的三个关键因素                     │
└─────────────────────────────────────────────────┘

因素1: FOR UPDATE 的锁定范围
─────────────────────────────
当 SELECT ... FOR UPDATE 未找到记录时:
  • 不仅扫描索引范围
  • 还会对扫描的间隙加间隙锁
  • 目的: 防止其他事务插入满足条件的记录(防止幻读)

查询: WHERE user1_id=1 AND user2_id=2
索引: idx_users(user1_id, user2_id)
加锁: Gap Lock on ((1,3), (2,4))


因素2: 间隙锁的兼容性
─────────────────────────────
两个事务可以同时持有同一间隙的间隙锁:
  • 事务A: Gap Lock ✅
  • 事务B: Gap Lock ✅ (不冲突)
  
⚠️ 这是死锁的前提条件!


因素3: 插入意向锁的互斥性
─────────────────────────────
INSERT 操作需要获取插入意向锁:
  • 检查目标间隙是否有间隙锁
  • 如果有 → 等待间隙锁释放
  
事务A: INSERT → 需要插入意向锁 → 被B的间隙锁阻塞
事务B: INSERT → 需要插入意向锁 → 被A的间隙锁阻塞
  
💀 循环等待形成!
```

## 死锁可以回滚还需要修复吗

### 🚨 **问题1: 用户体验差**

```
用户请求流程:
┌─────────────────────────────────────────┐
│  用户A: 创建与用户B的私聊               │
│   ↓                                     │
│  发起请求...                            │
│   ↓                                     │
│  💀 死锁! 事务被回滚                    │
│   ↓                                     │
│  返回 false                             │
│   ↓                                     │
│  ❌ 用户看到"创建失败"                  │
│   ↓                                     │
│  😤 用户体验: 明明是正常操作,为什么失败?│
└─────────────────────────────────────────┘
```

**问题**: 虽然没有数据错误,但用户需要**重试**,体验不好。

------

### 🚨 **问题2: 高并发下的性能损耗**

```
高并发场景 (100个并发请求):
┌──────────────────────────────────────────┐
│  100 个并发请求创建私聊                  │
│   ↓                                      │
│  • 50 个事务获取间隙锁 ✅                │
│  • 50 个事务也获取间隙锁 ✅              │
│   ↓                                      │
│  • 所有事务尝试 INSERT                   │
│  • 💀 大量死锁!                          │
│   ↓                                      │
│  • 50% 的事务被回滚                      │
│  • 这些事务需要重试                      │
│   ↓                                      │
│  • 数据库压力翻倍                        │
│  • CPU 消耗在死锁检测上                  │
│  • 大量事务日志写入                      │
└──────────────────────────────────────────┘

性能损失:
• 死锁检测: 消耗 CPU
• 回滚操作: 消耗 IO
• 客户端重试: 消耗网络和数据库连接
```



## 第六部分:解决方案

✅先INSERT后SELECT(乐观策略)

**原理**: 先尝试插入,如果失败说明已存在,再查询

```cpp
// 在 private_chat 表上添加唯一索引
// CREATE UNIQUE INDEX uk_user_pair ON private_chat(user1_id, user2_id);
bool CreatePrivateChat(int user1_id, int user2_id, int& thread_id)
{
    auto con = pool_->getConnection();
    Defer defer([this, &con]() { pool_->returnConnection(std::move(con)); });
    auto& conn = con->_con;
    
    int uid1 = std::min(user1_id, user2_id);
    int uid2 = std::max(user1_id, user2_id);
    
    try {
        conn->setAutoCommit(false);
        
        // 先创建 chat_thread
        std::string insert_thread_sql =
            "INSERT INTO chat_thread (type, created_at) VALUES ('private', NOW());";
        std::unique_ptr<sql::PreparedStatement> pstmt_thread(
            conn->prepareStatement(insert_thread_sql));
        pstmt_thread->executeUpdate();
        
        // 获取 thread_id
        std::unique_ptr<sql::PreparedStatement> pstmt_id(
            conn->prepareStatement("SELECT LAST_INSERT_ID();"));
        std::unique_ptr<sql::ResultSet> res_id(pstmt_id->executeQuery());
        res_id->next();
        thread_id = res_id->getInt(1);
        
        // ✅ 直接尝试插入
        std::string insert_sql =
            "INSERT INTO private_chat (thread_id, user1_id, user2_id, created_at) "
            "VALUES (?, ?, ?, NOW());";
        
        std::unique_ptr<sql::PreparedStatement> pstmt_insert(
            conn->prepareStatement(insert_sql));
        pstmt_insert->setInt64(1, thread_id);
        pstmt_insert->setInt64(2, uid1);
        pstmt_insert->setInt64(3, uid2);
        pstmt_insert->executeUpdate();
        
        conn->commit();
        return true;
    }
    catch (sql::SQLException& e) {
        conn->rollback();
        
        // 如果是唯一键冲突 (错误码 1062)
        if (e.getErrorCode() == 1062) {
            // ✅ 查询已存在的记录
            try {
                std::string query_sql =
                    "SELECT thread_id FROM private_chat "
                    "WHERE user1_id = ? AND user2_id = ?;";
                
                std::unique_ptr<sql::PreparedStatement> pstmt_query(
                    conn->prepareStatement(query_sql));
                pstmt_query->setInt64(1, uid1);
                pstmt_query->setInt64(2, uid2);
                std::unique_ptr<sql::ResultSet> res(pstmt_query->executeQuery());
                
                if (res->next()) {
                    thread_id = res->getInt("thread_id");
                    return true;
                }
            }
            catch (sql::SQLException& e2) {
                std::cerr << "Query error: " << e2.what() << std::endl;
            }
        }
        
        std::cerr << "SQLException: " << e.what() << std::endl;
        return false;
    }
}
```

**优点**:

- ✅ 避免死锁
- ✅ 大部分情况下性能好(只有一次数据库操作)

**缺点**:

- ⚠️ 需要唯一索引
- ⚠️ 依赖异常处理



## 记录锁死锁分析

![image-20251204140931698](https://cdn.llfc.club/img/image-20251204140931698.png)

``` cpp
bool MysqlDao::AddFriend(const int& from, const int& to, std::string back_name,
	std::vector<std::shared_ptr<AddFriendMsg>>& chat_datas) {
	auto con = pool_->getConnection();
	if (con == nullptr) {
		return false;
	}

	Defer defer([this, &con]() {
		pool_->returnConnection(std::move(con));
		});

	try {

		//开始事务
		con->_con->setAutoCommit(false);
		std::string reverse_back;
		std::string apply_desc;

		{
			// 1. 锁定并读取
			std::unique_ptr<sql::PreparedStatement> selStmt(con->_con->prepareStatement(
				"SELECT back_name, descs "
				"FROM friend_apply "
				"WHERE from_uid = ? AND to_uid = ? "
				"FOR UPDATE"
			));
			selStmt->setInt(1, to);
			selStmt->setInt(2, from);

			std::unique_ptr<sql::ResultSet> rsSel(selStmt->executeQuery());

			if (rsSel->next()) {
				reverse_back = rsSel->getString("back_name");
				apply_desc = rsSel->getString("descs");
			}
			else {
				// 没有对应的申请记录，直接 rollback 并返回失败
				con->_con->rollback();
				return false;
			}
		}
		
		{
			// 2. 执行真正的更新
			std::unique_ptr<sql::PreparedStatement> updStmt(con->_con->prepareStatement(
				"UPDATE friend_apply "
				"SET status = 1 "
				"WHERE from_uid = ? AND to_uid = ?"
			));
	
			updStmt->setInt(1, to);
			updStmt->setInt(2, from);

			if (updStmt->executeUpdate() != 1) {
				// 更新行数不对，回滚
				con->_con->rollback();
				return false;
			}
		}
		
		{
			// 3. 准备第一个SQL语句, 插入认证方好友数据
			std::unique_ptr<sql::PreparedStatement> pstmt(con->_con->prepareStatement("INSERT IGNORE INTO friend(self_id, friend_id, back) "
				"VALUES (?, ?, ?) "
			));
			//反过来的申请时from，验证时to
			pstmt->setInt(1, from); // from id
			pstmt->setInt(2, to);
			pstmt->setString(3, back_name);
			// 执行更新
			int rowAffected = pstmt->executeUpdate();
			if (rowAffected < 0) {
				con->_con->rollback();
				return false;
			}

			//准备第二个SQL语句，插入申请方好友数据
			std::unique_ptr<sql::PreparedStatement> pstmt2(con->_con->prepareStatement("INSERT IGNORE INTO friend(self_id, friend_id, back) "
				"VALUES (?, ?, ?) "
			));
			//反过来的申请时from，验证时to
			pstmt2->setInt(1, to); // from id
			pstmt2->setInt(2, from);
			pstmt2->setString(3, reverse_back);
			// 执行更新
			int rowAffected2 = pstmt2->executeUpdate();
			if (rowAffected2 < 0) {
				con->_con->rollback();
				return false;
			}
		}

		// 4. 创建 chat_thread
		long long threadId = 0;
		{
			std::unique_ptr<sql::PreparedStatement> threadStmt(con->_con->prepareStatement(
				"INSERT INTO chat_thread (type, created_at) VALUES ('private', NOW());"
			));
			
			threadStmt->executeUpdate();

			std::unique_ptr<sql::Statement> stmt(con->_con->createStatement());
			std::unique_ptr<sql::ResultSet> rs(
				stmt->executeQuery("SELECT LAST_INSERT_ID()")
			);

			if (rs->next()) {
				threadId = rs->getInt64(1);
			}
			else {
				return false;
			}
		}

		// 5. 插入 private_chat
		{
			std::unique_ptr<sql::PreparedStatement> pcStmt(con->_con->prepareStatement(
				"INSERT INTO private_chat(thread_id, user1_id, user2_id) VALUES (?, ?, ?)"
			));

			pcStmt->setInt64(1, threadId);
			pcStmt->setInt(2, from);
			pcStmt->setInt(3, to);
			if (pcStmt->executeUpdate() < 0) return false;
		}

		// 6. 可选：插入初始消息到 chat_message
		if (apply_desc.empty() == false)
		{
			std::unique_ptr<sql::PreparedStatement> msgStmt(con->_con->prepareStatement(
				"INSERT INTO chat_message(thread_id, sender_id, recv_id, content,created_at, updated_at, status) VALUES (?, ?, ?, ?,NOW(),NOW(),?)"
			));
		
			msgStmt->setInt64(1, threadId);
			msgStmt->setInt(2, to);
			msgStmt->setInt(3, from);
			msgStmt->setString(4, apply_desc);
			msgStmt->setInt(5, 2);
			if (msgStmt->executeUpdate() < 0) { return false; }

			std::unique_ptr<sql::Statement> stmt(con->_con->createStatement());
			std::unique_ptr<sql::ResultSet> rs(
				stmt->executeQuery("SELECT LAST_INSERT_ID()")
			);
			if (rs->next()) {
				auto messageId = rs->getInt64(1);
				auto tx_data = std::make_shared<AddFriendMsg>();
				tx_data->set_sender_id(to);
				tx_data->set_msg_id(messageId);
				tx_data->set_msgcontent(apply_desc);
				tx_data->set_thread_id(threadId);
				tx_data->set_unique_id("");
				tx_data->set_status(2);
				std::cout << "addfriend insert message success" << std::endl;
				chat_datas.push_back(tx_data);
			}
			else {
				return false;
			}
		}

		{
			std::unique_ptr<sql::PreparedStatement> msgStmt(con->_con->prepareStatement(
				"INSERT INTO chat_message(thread_id, sender_id, recv_id, content, created_at, updated_at, status) VALUES (?, ?, ?, ?,NOW(),NOW(),?)"
			));
		
			msgStmt->setInt64(1, threadId);
			msgStmt->setInt(2, from);
			msgStmt->setInt(3, to);
			msgStmt->setString(4, "We are friends now!");

			msgStmt->setInt(5, 2);

			if (msgStmt->executeUpdate() < 0) { return false; }

			std::unique_ptr<sql::Statement> stmt(con->_con->createStatement());
			std::unique_ptr<sql::ResultSet> rs(
				stmt->executeQuery("SELECT LAST_INSERT_ID()")
			);
			if (rs->next()) {
				auto messageId = rs->getInt64(1);
				auto tx_data = std::make_shared<AddFriendMsg>();
				tx_data->set_sender_id(from);
				tx_data->set_msg_id(messageId);
				tx_data->set_msgcontent("We are friends now!");
				tx_data->set_thread_id(threadId);
				tx_data->set_unique_id("");
				tx_data->set_status(2);
				chat_datas.push_back(tx_data);
			}
			else {
				return false;
			}
		}

		// 提交事务
		con->_con->commit();
		std::cout << "addfriend insert friends success" << std::endl;

		return true;
	}
	catch (sql::SQLException& e) {
		// 如果发生错误，回滚事务
		if (con) {
			con->_con->rollback();
		}
		std::cerr << "SQLException: " << e.what();
		std::cerr << " (MySQL error code: " << e.getErrorCode();
		std::cerr << ", SQLState: " << e.getSQLState() << " )" << std::endl;
		return false;
	}


	return true;
}
```

**分析**

这段代码死锁的关键点在于同意好友好插入用户顺序不同，导致死锁。

``` cpp
{
			// 3. 准备第一个SQL语句, 插入认证方好友数据
			std::unique_ptr<sql::PreparedStatement> pstmt(con->_con->prepareStatement("INSERT IGNORE INTO friend(self_id, friend_id, back) "
				"VALUES (?, ?, ?) "
			));
			//反过来的申请时from，验证时to
			pstmt->setInt(1, from); // from id
			pstmt->setInt(2, to);
			pstmt->setString(3, back_name);
			// 执行更新
			int rowAffected = pstmt->executeUpdate();
			if (rowAffected < 0) {
				con->_con->rollback();
				return false;
			}

			//准备第二个SQL语句，插入申请方好友数据
			std::unique_ptr<sql::PreparedStatement> pstmt2(con->_con->prepareStatement("INSERT IGNORE INTO friend(self_id, friend_id, back) "
				"VALUES (?, ?, ?) "
			));
			//反过来的申请时from，验证时to
			pstmt2->setInt(1, to); // from id
			pstmt2->setInt(2, from);
			pstmt2->setString(3, reverse_back);
			// 执行更新
			int rowAffected2 = pstmt2->executeUpdate();
			if (rowAffected2 < 0) {
				con->_con->rollback();
				return false;
			}
		}

```



### 解决方案

保证插入顺序一致即可

``` cpp
bool MysqlDao::AddFriend(const int& from, const int& to, std::string back_name,
	std::vector<std::shared_ptr<AddFriendMsg>>& chat_datas) {
	auto con = pool_->getConnection();
	if (con == nullptr) {
		return false;
	}

	Defer defer([this, &con]() {
		pool_->returnConnection(std::move(con));
		});

	try {
		// 开始事务
		con->_con->setAutoCommit(false);
		std::string reverse_back;
		std::string apply_desc;

		{
			// 1. 锁定并读取
			std::unique_ptr<sql::PreparedStatement> selStmt(con->_con->prepareStatement(
				"SELECT back_name, descs "
				"FROM friend_apply "
				"WHERE from_uid = ? AND to_uid = ? "
				"FOR UPDATE"
			));
			selStmt->setInt(1, to);
			selStmt->setInt(2, from);

			std::unique_ptr<sql::ResultSet> rsSel(selStmt->executeQuery());

			if (rsSel->next()) {
				reverse_back = rsSel->getString("back_name");
				apply_desc = rsSel->getString("descs");
			}
			else {
				// 没有对应的申请记录，直接 rollback 并返回失败
				con->_con->rollback();
				return false;
			}
		}

		{
			// 2. 执行真正的更新
			std::unique_ptr<sql::PreparedStatement> updStmt(con->_con->prepareStatement(
				"UPDATE friend_apply "
				"SET status = 1 "
				"WHERE from_uid = ? AND to_uid = ?"
			));

			updStmt->setInt(1, to);
			updStmt->setInt(2, from);

			if (updStmt->executeUpdate() != 1) {
				// 更新行数不对，回滚
				con->_con->rollback();
				return false;
			}
		}

		{
			// 3. 插入好友关系 - 关键改进：按照固定顺序插入避免死锁
			// 确定插入顺序：始终按照 uid 大小顺序
			int smaller_uid = std::min(from, to);
			int larger_uid = std::max(from, to);

			// 第一次插入：较小的 uid 作为 self_id
			std::unique_ptr<sql::PreparedStatement> pstmt(con->_con->prepareStatement(
				"INSERT IGNORE INTO friend(self_id, friend_id, back) "
				"VALUES (?, ?, ?)"
			));

			if (from == smaller_uid) {
				pstmt->setInt(1, from);
				pstmt->setInt(2, to);
				pstmt->setString(3, back_name);
			}
			else {
				pstmt->setInt(1, to);
				pstmt->setInt(2, from);
				pstmt->setString(3, reverse_back);
			}

			int rowAffected = pstmt->executeUpdate();
			if (rowAffected < 0) {
				con->_con->rollback();
				return false;
			}

			// 第二次插入：较大的 uid 作为 self_id
			std::unique_ptr<sql::PreparedStatement> pstmt2(con->_con->prepareStatement(
				"INSERT IGNORE INTO friend(self_id, friend_id, back) "
				"VALUES (?, ?, ?)"
			));

			if (from == larger_uid) {
				pstmt2->setInt(1, from);
				pstmt2->setInt(2, to);
				pstmt2->setString(3, back_name);
			}
			else {
				pstmt2->setInt(1, to);
				pstmt2->setInt(2, from);
				pstmt2->setString(3, reverse_back);
			}

			int rowAffected2 = pstmt2->executeUpdate();
			if (rowAffected2 < 0) {
				con->_con->rollback();
				return false;
			}
		}

		// 4. 创建 chat_thread
		long long threadId = 0;
		{
			std::unique_ptr<sql::PreparedStatement> threadStmt(con->_con->prepareStatement(
				"INSERT INTO chat_thread (type, created_at) VALUES ('private', NOW())"
			));

			threadStmt->executeUpdate();

			std::unique_ptr<sql::Statement> stmt(con->_con->createStatement());
			std::unique_ptr<sql::ResultSet> rs(
				stmt->executeQuery("SELECT LAST_INSERT_ID()")
			);

			if (rs->next()) {
				threadId = rs->getInt64(1);
			}
			else {
				con->_con->rollback();
				return false;
			}
		}

		// 5. 插入 private_chat
		{
			std::unique_ptr<sql::PreparedStatement> pcStmt(con->_con->prepareStatement(
				"INSERT INTO private_chat(thread_id, user1_id, user2_id) VALUES (?, ?, ?)"
			));

			pcStmt->setInt64(1, threadId);
			pcStmt->setInt(2, from);
			pcStmt->setInt(3, to);

			if (pcStmt->executeUpdate() < 0) {
				con->_con->rollback();
				return false;
			}
		}

		// 6. 插入初始消息（申请描述）
		if (!apply_desc.empty())
		{
			std::unique_ptr<sql::PreparedStatement> msgStmt(con->_con->prepareStatement(
				"INSERT INTO chat_message(thread_id, sender_id, recv_id, content, created_at, updated_at, status) "
				"VALUES (?, ?, ?, ?, NOW(), NOW(), ?)"
			));

			msgStmt->setInt64(1, threadId);
			msgStmt->setInt(2, to);
			msgStmt->setInt(3, from);
			msgStmt->setString(4, apply_desc);
			msgStmt->setInt(5, 2);

			if (msgStmt->executeUpdate() < 0) {
				con->_con->rollback();
				return false;
			}

			std::unique_ptr<sql::Statement> stmt(con->_con->createStatement());
			std::unique_ptr<sql::ResultSet> rs(
				stmt->executeQuery("SELECT LAST_INSERT_ID()")
			);

			if (rs->next()) {
				auto messageId = rs->getInt64(1);
				auto tx_data = std::make_shared<AddFriendMsg>();
				tx_data->set_sender_id(to);
				tx_data->set_msg_id(messageId);
				tx_data->set_msgcontent(apply_desc);
				tx_data->set_thread_id(threadId);
				tx_data->set_unique_id("");
				tx_data->set_status(2);
				std::cout << "addfriend insert message success" << std::endl;
				chat_datas.push_back(tx_data);
			}
			else {
				con->_con->rollback();
				return false;
			}
		}

		// 7. 插入成为好友的消息
		{
			std::unique_ptr<sql::PreparedStatement> msgStmt(con->_con->prepareStatement(
				"INSERT INTO chat_message(thread_id, sender_id, recv_id, content, created_at, updated_at, status) "
				"VALUES (?, ?, ?, ?, NOW(), NOW(), ?)"
			));

			msgStmt->setInt64(1, threadId);
			msgStmt->setInt(2, from);
			msgStmt->setInt(3, to);
			msgStmt->setString(4, "We are friends now!");
			msgStmt->setInt(5, 2);

			if (msgStmt->executeUpdate() < 0) {
				con->_con->rollback();
				return false;
			}

			std::unique_ptr<sql::Statement> stmt(con->_con->createStatement());
			std::unique_ptr<sql::ResultSet> rs(
				stmt->executeQuery("SELECT LAST_INSERT_ID()")
			);

			if (rs->next()) {
				auto messageId = rs->getInt64(1);
				auto tx_data = std::make_shared<AddFriendMsg>();
				tx_data->set_sender_id(from);
				tx_data->set_msg_id(messageId);
				tx_data->set_msgcontent("We are friends now!");
				tx_data->set_thread_id(threadId);
				tx_data->set_unique_id("");
				tx_data->set_status(2);
				chat_datas.push_back(tx_data);
			}
			else {
				con->_con->rollback();
				return false;
			}
		}

		// 提交事务
		con->_con->commit();
		std::cout << "addfriend insert friends success" << std::endl;

		return true;
	}
	catch (sql::SQLException& e) {
		// 如果发生错误，回滚事务
		if (con) {
			con->_con->rollback();
		}
		std::cerr << "SQLException: " << e.what();
		std::cerr << " (MySQL error code: " << e.getErrorCode();
		std::cerr << ", SQLState: " << e.getSQLState() << " )" << std::endl;

		// 如果是死锁错误（1213），可以考虑重试
		if (e.getErrorCode() == 1213) {
			std::cerr << "Deadlock detected, consider retry" << std::endl;
		}

		return false;
	}

	return true;
}
```





​                                                                                                                                                                                                                                    