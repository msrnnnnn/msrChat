#pragma once
/**
 * @file SignalHandler.h
 * @brief Linux 信号处理与守护进程管理
 *
 * 面试要点：
 *   1. sigaction vs signal：
 *      - signal() 在不同 Unix 系统上行为不同（System V 自动重置，BSD 不重置）
 *      - sigaction() 行为明确、可移植，可设置 SA_RESTART 等标志
 *   2. SIGHUP 热加载：
 *      - 传统 Unix：终端断开时发送 SIGHUP 给前台进程组
 *      - 守护进程惯例：用 SIGHUP 通知进程重新加载配置
 *      - 不需要重启进程，不中断服务
 *   3. 守护进程（daemon）：
 *      - fork() → 父进程退出，子进程被 init 收养
 *      - setsid() → 创建新会话，脱离控制终端
 *      - 再次 fork() → 防止进程重新获取控制终端
 *      - umask(0) → 清除文件权限掩码
 *      - 关闭 stdin/stdout/stderr → 重定向到 /dev/null
 *   4. PID 文件：
 *      - /var/run/xxx.pid 或 /tmp/xxx.pid
 *      - 启动时检查：文件存在且进程存活 → 拒绝重复启动
 *      - 退出时清理：删除 PID 文件
 *
 * 编译条件：
 *   - Linux/macOS：完整功能（sigaction、daemon、PID 文件）
 *   - Windows：仅提供回调注册，信号处理由 boost::asio::signal_set 处理
 */
#ifndef SIGNAL_HANDLER_H
#define SIGNAL_HANDLER_H

#include <functional>
#include <string>

class SignalHandler
{
public:
    /**
     * @brief 注册信号处理器
     * @param on_shutdown   SIGINT/SIGTERM 触发，执行优雅关闭
     * @param on_reload     SIGHUP 触发，重新加载配置（Linux only）
     *
     * Linux 实现细节：
     *   - 使用 sigaction() 注册 SA_RESTART 标志，被信号中断的系统调用自动重启
     *   - SIGINT  (Ctrl+C)  → on_shutdown
     *   - SIGTERM (kill)    → on_shutdown
     *   - SIGHUP  (kill -HUP) → on_reload
     *   - SIGPIPE 直接忽略（网络编程中写入已断开的管道会产生此信号）
     */
    static void Setup(std::function<void()> on_shutdown, std::function<void()> on_reload);

    /**
     * @brief 守护进程化（double-fork 模式）
     * @return true 成功，false 失败
     *
     * 流程（Linux only）：
     *   1. fork() → 父进程 _exit(0)，子进程继续
     *      - 子进程成为孤儿进程，被 init(PID 1) 收养
     *   2. setsid() → 创建新会话（session），成为新会话首进程
     *      - 脱离控制终端，不会收到终端的 SIGHUP
     *   3. fork() 再次 fork → 子子进程不是会话首进程
     *      - 防止进程通过 open("/dev/tty") 重新获取控制终端
     *   4. umask(0) → 清除文件创建掩码，避免继承父进程的 umask
     *   5. chdir("/") → 切换到根目录，避免占用挂载点
     *   6. 关闭 stdin/stdout/stderr (fd 0,1,2) → 重定向到 /dev/null
     *
     * 为什么不直接用 daemon() 函数？
     *   - daemon() 是 BSD 扩展，不是 POSIX 标准
     *   - daemon(0,0) 不做第二次 fork，安全性稍差
     *   - 手动实现可以展示对 fork/setsid 的理解
     */
    static bool Daemonize();

    /**
     * @brief 写入 PID 文件
     * @param path PID 文件路径，如 "/var/run/chatserver.pid"
     * @return true 成功，false 失败
     *
     * 实现：
     *   - 写入当前进程 PID (getpid())
     *   - 文件已存在时检查其中记录的 PID 是否仍在运行（kill(pid, 0)）
     *   - 进程仍在运行 → 返回 false（防止重复启动）
     *   - 进程已不存在 → 覆盖写入（清理残留）
     */
    static bool WritePidFile(const std::string &path);

    /**
     * @brief 删除 PID 文件
     * @param path PID 文件路径
     */
    static void RemovePidFile(const std::string &path);
};

#endif
