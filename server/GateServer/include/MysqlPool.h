/**
 * @file MysqlPool.h
 * @brief MySQL 连接池定义
 * @details 维护一组 MySQL 连接，支持多线程安全地获取和归还连接。
 */
#pragma once

#include <atomic>             // std::atomic
#include <condition_variable> // std::condition_variable
#include <iostream>           // std::cout, std::endl
#include <memory>             // std::unique_ptr
#include <mutex>              // std::mutex, std::unique_lock
#include <queue>              // std::queue
#include <string>

// MySQL Connector/C++ 库
#include <cppconn/connection.h>
#include <cppconn/driver.h>
#include <cppconn/exception.h>
#include <cppconn/prepared_statement.h>
#include <cppconn/resultset.h>
#include <cppconn/statement.h>
#include <mysql_connection.h>
#include <mysql_driver.h>

/**
 * @class   MySqlPool
 * @brief   MySQL 连接池
 */
class MySqlPool
{
public:
    /**
     * @brief   构造函数
     * @param   url      数据库地址 (tcp://host:port)
     * @param   user     用户名
     * @param   pass     密码
     * @param   schema   数据库名
     * @param   poolSize 连接池大小
     */
    MySqlPool(
        const std::string &url, const std::string &user, const std::string &pass, const std::string &schema,
        int poolSize)
        : url_(url),
          user_(user),
          pass_(pass),
          schema_(schema),
          poolSize_(poolSize),
          b_stop_(false)
    {
        try
        {
            for (int i = 0; i < poolSize_; ++i)
            {
                sql::mysql::MySQL_Driver *driver = sql::mysql::get_mysql_driver_instance();
                std::unique_ptr<sql::Connection> con(driver->connect(url_, user_, pass_));
                con->setSchema(schema_);
                pool_.push(std::move(con));
            }
        }
        catch (sql::SQLException &e)
        {
            // 处理异常
            std::cout << "mysql pool init failed: " << e.what() << std::endl;
        }
    }

    /**
     * @brief   析构函数
     * @details 销毁连接池中的所有连接
     */
    ~MySqlPool()
    {
        std::unique_lock<std::mutex> lock(mutex_);
        while (!pool_.empty())
        {
            pool_.pop();
        }
    }

    /**
     * @brief   获取一个数据库连接
     * @details 如果连接池为空，则阻塞等待，直到有连接可用或连接池关闭。
     * @return  std::unique_ptr<sql::Connection> 如果连接池关闭则返回 nullptr
     */
    std::unique_ptr<sql::Connection> getConnection()
    {
        // 使用 RAII 锁 (unique_lock) 自动管理互斥量，
        // 配合 condition_variable 实现线程安全的等待机制。
        std::unique_lock<std::mutex> lock(mutex_);
        
        // 循环检查条件 (Predicate)，防止虚假唤醒 (Spurious Wakeup)。
        cond_.wait(
            lock,
            [this]
            {
                if (b_stop_)
                {
                    return true;
                }
                return !pool_.empty();
            });
        if (b_stop_)
        {
            return nullptr;
        }
        std::unique_ptr<sql::Connection> con(std::move(pool_.front()));
        pool_.pop();
        return con;
    }

    /**
     * @brief   归还一个数据库连接
     * @param   con 归还的连接对象
     */
    void returnConnection(std::unique_ptr<sql::Connection> con)
    {
        std::unique_lock<std::mutex> lock(mutex_);
        if (b_stop_)
        {
            return;
        }
        pool_.push(std::move(con));
        cond_.notify_one();
    }

    /**
     * @brief   关闭连接池
     * @details 唤醒所有等待的线程，并停止分配新连接。
     */
    void Close()
    {
        b_stop_ = true;
        cond_.notify_all();
    }

private:
    std::string url_;                                   ///< 数据库 URL
    std::string user_;                                  ///< 数据库用户名
    std::string pass_;                                  ///< 数据库密码
    std::string schema_;                                ///< 数据库名
    int poolSize_;                                      ///< 连接池大小
    std::queue<std::unique_ptr<sql::Connection>> pool_; ///< 连接队列
    std::mutex mutex_;                                  ///< 互斥锁
    std::condition_variable cond_;                      ///< 条件变量
    std::atomic<bool> b_stop_;                          ///< 停止标志
};