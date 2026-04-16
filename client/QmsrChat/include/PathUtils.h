/**
 * @file PathUtils.h
 * @brief 跨平台路径工具类
 * @details 提供可执行文件路径、配置目录、日志目录等跨平台获取方法
 */

#ifndef PATHUTILS_H
#define PATHUTILS_H

#include <QString>
#include <QStandardPaths>
#include <QDir>
#include <QFileInfo>

namespace PathUtils
{
    /**
     * @brief 获取应用程序的可执行文件所在目录
     * @return 可执行文件所在目录路径
     */
    inline QString getExecutableDir()
    {
        return QFileInfo(QCoreApplication::applicationFilePath()).absolutePath();
    }

    /**
     * @brief 获取应用程序的可执行文件完整路径
     * @return 可执行文件完整路径
     */
    inline QString getExecutablePath()
    {
        return QFileInfo(QCoreApplication::applicationFilePath()).absoluteFilePath();
    }

    /**
     * @brief 获取配置文件目录
     * @return 配置目录路径（Windows: AppData, Linux: ~/.config/msrchat）
     */
    inline QString getConfigDir()
    {
        QString configPath = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
        QDir dir(configPath);
        if (!dir.exists())
        {
            dir.mkpath(configPath);
        }
        return configPath;
    }

    /**
     * @brief 获取日志文件目录
     * @return 日志目录路径（Windows: AppData/Logs, Linux: ~/.local/share/msrchat/logs）
     */
    inline QString getLogDir()
    {
        QString logPath;
        
#ifdef Q_OS_WIN
        // Windows: 使用 AppData/Local/msrchat/logs
        logPath = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
#elif defined(Q_OS_LINUX)
        // Linux: 使用 ~/.local/share/msrchat/logs
        logPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
#else
        // macOS 和其他平台
        logPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
#endif
        
        logPath += "/logs";
        
        QDir dir(logPath);
        if (!dir.exists())
        {
            dir.mkpath(logPath);
        }
        return logPath;
    }

    /**
     * @brief 获取临时文件目录
     * @return 临时文件目录路径（跨平台使用系统临时目录）
     */
    inline QString getTempDir()
    {
        QString tempPath = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
        tempPath += "/msrchat";
        
        QDir dir(tempPath);
        if (!dir.exists())
        {
            dir.mkpath(tempPath);
        }
        return tempPath;
    }

    /**
     * @brief 获取下载文件目录
     * @return 下载目录路径（跨平台使用系统下载目录）
     */
    inline QString getDownloadDir()
    {
        QString downloadPath = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
        downloadPath += "/msrchat";
        
        QDir dir(downloadPath);
        if (!dir.exists())
        {
            dir.mkpath(downloadPath);
        }
        return downloadPath;
    }

    /**
     * @brief 获取数据库文件路径
     * @return 数据库文件路径
     */
    inline QString getDatabasePath()
    {
        QString dataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        
        QDir dir(dataPath);
        if (!dir.exists())
        {
            dir.mkpath(dataPath);
        }
        
        return dataPath + "/msrchat.db";
    }

    /**
     * @brief 确保目录存在，不存在则创建
     * @param dirPath 目录路径
     * @return true 创建成功或已存在，false 创建失败
     */
    inline bool ensureDirExists(const QString &dirPath)
    {
        QDir dir(dirPath);
        if (!dir.exists())
        {
            return dir.mkpath(dirPath);
        }
        return true;
    }

    /**
     * @brief 获取平台特定的路径分隔符
     * @return 路径分隔符字符串
     */
    inline QString getPathSeparator()
    {
#ifdef Q_OS_WIN
        return QStringLiteral("\\");
#else
        return QStringLiteral("/");
#endif
    }

    /**
     * @brief 连接路径（跨平台）
     * @param path1 路径1
     * @param path2 路径2
     * @return 连接后的路径
     */
    inline QString joinPath(const QString &path1, const QString &path2)
    {
        return QDir::cleanPath(path1 + getPathSeparator() + path2);
    }
}

#endif // PATHUTILS_H
