/**
 * @file SignalHandler.cpp
 * @brief Linux 信号处理与守护进程管理实现
 *
 * 面试可展开的知识点：
 *   - sigaction 结构体：sa_handler（处理函数）、sa_mask（信号掩码）、sa_flags（标志位）
 *   - SA_RESTART：被信号中断的慢系统调用（read/write/accept）自动重启，避免 EINTR
 *   - SA_RESETHAND：处理一次后恢复默认行为（不用于我们的场景，会导致第二次 SIGINT 直接杀死进程）
 *   - 信号掩码 sa_mask：处理某个信号时临时阻塞的其他信号集合
 *   - fork() 返回值：父进程返回子进程 PID，子进程返回 0，失败返回 -1
 *   - setsid()：创建新会话，调用进程成为会话首进程和进程组长
 */
#include "SignalHandler.h"

#ifndef _WIN32
#include <cerrno>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <fstream>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

/// 全局回调（信号处理函数只能访问全局/静态变量）
static std::function<void()> g_on_shutdown;
static std::function<void()> g_on_reload;

/**
 * @brief SIGINT/SIGTERM 信号处理函数
 *
 * 注意事项（面试常问）：
 *   - 信号处理函数中只能调用 async-signal-safe 的函数
 *   - write() 是 async-signal-safe 的，printf/fprintf 不是
 *   - 这里只设置标志位，实际工作在主循环中完成（最安全的做法）
 *   - 但为了简洁，这里直接调用回调（项目中回调内部也是设置标志 + 通知 io_context）
 */
static void HandleShutdown(int sig)
{
    const char *msg = (sig == SIGINT) ? "Caught SIGINT\n" : "Caught SIGTERM\n";
    // write() 是 async-signal-safe 的，可以在信号处理函数中使用
    write(STDOUT_FILENO, msg, strlen(msg));
    if (g_on_shutdown)
    {
        g_on_shutdown();
    }
}

/**
 * @brief SIGHUP 信号处理函数
 *
 * SIGHUP 的语义：
 *   - 原始含义：终端断开时，内核向前台进程组发送 SIGHUP
 *   - 守护进程惯例：用 SIGHUP 触发配置重载（类似 nginx -s reload）
 *   - 不需要重启进程，不中断正在进行的连接
 */
static void HandleReload(int sig)
{
    (void)sig;
    const char *msg = "Caught SIGHUP, reloading config...\n";
    write(STDOUT_FILENO, msg, strlen(msg));
    if (g_on_reload)
    {
        g_on_reload();
    }
}

void SignalHandler::Setup(std::function<void()> on_shutdown, std::function<void()> on_reload)
{
    g_on_shutdown = std::move(on_shutdown);
    g_on_reload = std::move(on_reload);

    // ---- SIGINT (Ctrl+C) 和 SIGTERM (kill) → 优雅关闭 ----
    struct sigaction sa_shutdown = {};
    sa_shutdown.sa_handler = HandleShutdown;
    sa_shutdown.sa_flags = SA_RESTART; // 被中断的系统调用自动重启
    sigemptyset(&sa_shutdown.sa_mask);

    if (sigaction(SIGINT, &sa_shutdown, nullptr) == -1)
    {
        perror("sigaction(SIGINT)");
    }
    if (sigaction(SIGTERM, &sa_shutdown, nullptr) == -1)
    {
        perror("sigaction(SIGTERM)");
    }

    // ---- SIGHUP → 配置热加载 ----
    struct sigaction sa_reload = {};
    sa_reload.sa_handler = HandleReload;
    sa_reload.sa_flags = SA_RESTART;
    sigemptyset(&sa_reload.sa_mask);

    if (sigaction(SIGHUP, &sa_reload, nullptr) == -1)
    {
        perror("sigaction(SIGHUP)");
    }

    // ---- SIGPIPE → 忽略 ----
    // 网络编程中，向已关闭的 socket 写入会触发 SIGPIPE
    // 忽略它，让 write() 返回 EPIPE 错误，由应用层处理
    struct sigaction sa_ignore = {};
    sa_ignore.sa_handler = SIG_IGN;
    sigemptyset(&sa_ignore.sa_mask);

    if (sigaction(SIGPIPE, &sa_ignore, nullptr) == -1)
    {
        perror("sigaction(SIGPIPE)");
    }
}

bool SignalHandler::Daemonize()
{
    // ---- 第一次 fork ----
    // 父进程退出，子进程继续
    // 子进程成为孤儿进程，被 init(PID 1) 收养
    pid_t pid = fork();
    if (pid < 0)
    {
        perror("fork() first");
        return false;
    }
    if (pid > 0)
    {
        // 父进程：打印子进程 PID 后退出
        _exit(0);
    }

    // ---- 创建新会话 ----
    // 调用进程成为新会话的首进程和进程组组长
    // 脱离控制终端，不会收到终端的 SIGHUP
    if (setsid() < 0)
    {
        perror("setsid()");
        return false;
    }

    // ---- 第二次 fork ----
    // 再次 fork 后，子子进程不再是会话首进程
    // 这防止进程通过 open("/dev/tty") 重新获取控制终端
    pid = fork();
    if (pid < 0)
    {
        perror("fork() second");
        return false;
    }
    if (pid > 0)
    {
        // 第一次 fork 的子进程：退出
        _exit(0);
    }

    // ---- 此时是第二次 fork 的子进程（最终的守护进程）----

    // 设置文件创建掩码
    umask(0);

    // 切换工作目录到根目录，避免占用某个挂载点导致无法 umount
    if (chdir("/") < 0)
    {
        perror("chdir(/)");
        return false;
    }

    // 关闭标准输入/输出/错误，重定向到 /dev/null
    // 守护进程没有控制终端，不应该从 stdin 读或向 stdout/stderr 写
    int fd = open("/dev/null", O_RDWR);
    if (fd >= 0)
    {
        dup2(fd, STDIN_FILENO);  // 0 → /dev/null
        dup2(fd, STDOUT_FILENO); // 1 → /dev/null
        dup2(fd, STDERR_FILENO); // 2 → /dev/null
        if (fd > STDERR_FILENO)
        {
            close(fd);
        }
    }

    return true;
}

bool SignalHandler::WritePidFile(const std::string &path)
{
    // 检查 PID 文件是否已存在
    std::ifstream check(path);
    if (check.is_open())
    {
        int existing_pid = 0;
        check >> existing_pid;
        check.close();

        if (existing_pid > 0)
        {
            // 检查该进程是否仍在运行
            // kill(pid, 0) 不发送信号，仅检查进程是否存在
            if (kill(existing_pid, 0) == 0 || errno == EPERM)
            {
                fprintf(stderr, "Process already running (PID %d, file: %s)\n", existing_pid, path.c_str());
                return false;
            }
            // 进程已不存在，清理残留 PID 文件
            fprintf(stderr, "Stale PID file found (PID %d not running), overwriting\n", existing_pid);
        }
    }

    // 写入当前进程 PID
    std::ofstream out(path);
    if (!out.is_open())
    {
        perror(("WritePidFile: " + path).c_str());
        return false;
    }
    out << getpid() << "\n";
    out.close();

    // 设置文件权限为 644（所有者读写，组和其他只读）
    chmod(path.c_str(), S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    return true;
}

void SignalHandler::RemovePidFile(const std::string &path)
{
    if (unlink(path.c_str()) != 0 && errno != ENOENT)
    {
        perror(("RemovePidFile: " + path).c_str());
    }
}

#else // _WIN32

void SignalHandler::Setup(std::function<void()> on_shutdown, std::function<void()> on_reload)
{
    // Windows 下信号处理由 boost::asio::signal_set 处理
    // 这里仅保存回调，不注册 sigaction
    (void)on_shutdown;
    (void)on_reload;
}

bool SignalHandler::Daemonize()
{
    fprintf(stderr, "Daemonize is not supported on Windows\n");
    return false;
}

bool SignalHandler::WritePidFile(const std::string &path)
{
    (void)path;
    return true;
}

void SignalHandler::RemovePidFile(const std::string &path)
{
    (void)path;
}

#endif
