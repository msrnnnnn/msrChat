/**
 * @file FileCoordinator.cpp
 * @brief 文件/图片发送接收协调器实现
 * @details Phase 5B.6 从 ChatController.cpp 提取。
 */
#include "FileCoordinator.h"
#include "DbWorker.h"
#include "FileRecvMgr.h"
#include "FileSendMgr.h"
#include "ImageDownloadMgr.h"
#include "TcpMgr.h"
#include <QDebug>
#include <QDateTime>
#include <QFileInfo>
#include <QImage>
#include <QUuid>

FileCoordinator::FileCoordinator(QObject *parent)
    : QObject(parent)
{
}

void FileCoordinator::setChatModel(ChatListModel *model)
{
    _chat_model = model;
}

void FileCoordinator::setTargetUid(int uid)
{
    _target_uid = uid;
}

void FileCoordinator::setCurrentUid(int uid)
{
    _current_uid = uid;
}

void FileCoordinator::setMaxReceivedTimestamp(qint64 *ts_ptr)
{
    _max_received_ts = ts_ptr;
}

QString FileCoordinator::normalizeFilePath(const QString &rawPath)
{
    QString cleanPath = rawPath;
    if (cleanPath.startsWith("file:///")) {
        cleanPath = cleanPath.mid(8);
    } else if (cleanPath.startsWith("file://")) {
        cleanPath = cleanPath.mid(7);
    }
    if (cleanPath.startsWith("/") && cleanPath.length() >= 3 && cleanPath[2] == ':') {
        cleanPath = cleanPath.mid(1);
    }
    return cleanPath;
}

/**
 * @brief 连接文件/图片相关信号槽
 */
void FileCoordinator::connectSignals()
{
    connect(
        &FileSendMgr::Instance(), &FileSendMgr::sigSendProgress, this,
        [this](int64_t task_id, int progress, int64_t sent, int64_t total)
        { emit sigFileSendProgress(task_id, progress, sent, total); }, Qt::QueuedConnection);
    connect(
        &FileSendMgr::Instance(), &FileSendMgr::sigSendComplete, this,
        [this](int64_t task_id, bool success, const QString &error)
        { emit sigFileSendComplete(task_id, success, error); }, Qt::QueuedConnection);
    connect(
        &FileRecvMgr::Instance(), &FileRecvMgr::sigRecvProgress, this,
        [this](int64_t task_id, int progress, int64_t received, int64_t total)
        { emit sigFileRecvProgress(task_id, progress, received, total); }, Qt::QueuedConnection);
    connect(
        &FileRecvMgr::Instance(), &FileRecvMgr::sigRecvStarted, this,
        [this](int64_t task_id, const QString &filename, int64_t total_size)
        { emit sigFileRecvStarted(task_id, filename, total_size); }, Qt::QueuedConnection);
    connect(
        &FileRecvMgr::Instance(), &FileRecvMgr::sigRecvComplete, this,
        [this](int64_t task_id, const QString &filepath, bool success, const QString &error)
        {
            if (success && _chat_model)
            {
                QFileInfo fi(filepath);
                QString stem = fi.completeBaseName();
                QUuid uuid(stem);
                if (!uuid.isNull())
                {
                    _chat_model->UpdateImagePath(stem, filepath);
                    DbThreadManager::Instance().UpdateImagePath(stem, filepath);
                    ImageDownloadMgr::Instance().OnFileRecvComplete(stem, filepath, true);
                }
            }
            emit sigFileRecvComplete(task_id, filepath, success, error);
        }, Qt::QueuedConnection);

    connect(TcpMgr::Instance(), &TcpMgr::sigChatImage, this, &FileCoordinator::slotOnChatImage, Qt::QueuedConnection);
    connect(TcpMgr::Instance(), &TcpMgr::sigImageDownloadRsp, this, &FileCoordinator::slotOnImageDownloadRsp, Qt::QueuedConnection);

    // 桥接信号到 TcpMgr
    connect(this, &FileCoordinator::sigSendImageMsg,
            TcpMgr::Instance(), &TcpMgr::slot_send_chat_image, Qt::QueuedConnection);

    // ImageDownloadMgr → TcpMgr
    connect(&ImageDownloadMgr::Instance(), &ImageDownloadMgr::sigRequestDownload,
            TcpMgr::Instance(), &TcpMgr::slot_send_image_download_req, Qt::QueuedConnection);
    connect(&ImageDownloadMgr::Instance(), &ImageDownloadMgr::sigImageFailed,
            this, [this](const QString &image_id, int reason) {
                if (_chat_model) {
                    _chat_model->UpdateImagePath(image_id, QStringLiteral("error"));
                }
                qWarning() << "[FileCoordinator] image download permanently failed:" << image_id << "reason:" << reason;
            }, Qt::QueuedConnection);
}

/**
 * @brief 断开文件/图片相关信号槽
 */
void FileCoordinator::disconnectSignals()
{
    disconnect(&FileSendMgr::Instance(), nullptr, this, nullptr);
    disconnect(&FileRecvMgr::Instance(), nullptr, this, nullptr);
    disconnect(TcpMgr::Instance(), &TcpMgr::sigChatImage, this, &FileCoordinator::slotOnChatImage);
    disconnect(TcpMgr::Instance(), &TcpMgr::sigImageDownloadRsp, this, &FileCoordinator::slotOnImageDownloadRsp);
    disconnect(this, &FileCoordinator::sigSendImageMsg, TcpMgr::Instance(), &TcpMgr::slot_send_chat_image);
    disconnect(&ImageDownloadMgr::Instance(), nullptr, this, nullptr);
    disconnect(&ImageDownloadMgr::Instance(), nullptr, TcpMgr::Instance(), nullptr);
}

void FileCoordinator::sendFile(const QString &filePath)
{
    qDebug() << "[FileCoordinator] sendFile called, path:" << filePath << "target_uid:" << _target_uid;

    if (_target_uid <= 0)
    {
        qWarning() << "[FileCoordinator] sendFile failed: target_uid is" << _target_uid;
        emit sigError(QStringLiteral("请先指定目标用户"));
        return;
    }

    QString cleanPath = normalizeFilePath(filePath);
    qDebug() << "[FileCoordinator] cleanPath after processing:" << cleanPath;

    QFileInfo fileInfo(cleanPath);
    qDebug() << "[FileCoordinator] file exists:" << fileInfo.exists() << "isFile:" << fileInfo.isFile();
    if (!fileInfo.exists() || !fileInfo.isFile())
    {
        qWarning() << "[FileCoordinator] sendFile failed: file does not exist:" << cleanPath;
        emit sigError(QStringLiteral("文件不存在或路径无效"));
        return;
    }

    int64_t task_id = QDateTime::currentMSecsSinceEpoch();
    int64_t total_size = fileInfo.size();

    FileReqStruct req;
    req.task_id = task_id;
    req.from_uid = _current_uid;
    req.to_uid = _target_uid;
    req.filename = fileInfo.fileName();
    req.total_size = total_size;
    req.md5 = "";
    TcpMgr::Instance()->slot_send_file_req(req);

    FileSendMgr::Instance().StartSend(task_id, _target_uid, cleanPath);
    emit sigFileSendStarted(task_id, fileInfo.fileName(), total_size);
}

void FileCoordinator::sendImage(const QString &imagePath, const QString &caption)
{
    qDebug() << "[FileCoordinator] sendImage called, path:" << imagePath << "caption_len:" << caption.size();

    if (_target_uid <= 0)
    {
        emit sigError(QStringLiteral("请先指定目标用户"));
        return;
    }

    QString cleanPath = normalizeFilePath(imagePath);
    QFileInfo fileInfo(cleanPath);
    if (!fileInfo.exists() || !fileInfo.isFile())
    {
        emit sigError(QStringLiteral("图片不存在或路径无效"));
        return;
    }

    QImage img(cleanPath);
    if (img.isNull())
    {
        emit sigError(QStringLiteral("无法读取图片"));
        return;
    }
    QString ext = fileInfo.suffix().toLower();
    int64_t total_size = fileInfo.size();

    QString image_id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    int64_t task_id = QDateTime::currentMSecsSinceEpoch();
    QString filename = image_id + "." + ext;

    FileReqStruct req;
    req.task_id = task_id;
    req.from_uid = _current_uid;
    req.to_uid = _target_uid;
    req.filename = filename;
    req.total_size = total_size;
    req.md5 = "";
    TcpMgr::Instance()->slot_send_file_req(req);

    FileSendMgr::Instance().StartSend(task_id, _target_uid, cleanPath);

    ChatImageStruct imgMsg;
    imgMsg.from_uid = _current_uid;
    imgMsg.to_uid = _target_uid;
    imgMsg.image_id = image_id;
    imgMsg.caption = caption;
    imgMsg.timestamp = qMax(QDateTime::currentMSecsSinceEpoch(),
                            _max_received_ts ? *_max_received_ts + 1 : 0LL);
    imgMsg.width = img.width();
    imgMsg.height = img.height();
    imgMsg.ext = ext;
    imgMsg.size = total_size;
    imgMsg.md5 = "";
    emit sigSendImageMsg(imgMsg);

    if (_chat_model)
    {
        ChatMessage m;
        m.from_uid = _current_uid;
        m.to_uid = _target_uid;
        m.type = 1;
        m.image_id = image_id;
        m.image_width = img.width();
        m.image_height = img.height();
        m.image_ext = ext;
        m.content = caption;
        m.timestamp = imgMsg.timestamp;
        m.image_path = cleanPath;
        m.client_msg_id = image_id;
        _chat_model->AddMessage(m);
    }

    emit sigFileSendStarted(task_id, filename, total_size);
}

void FileCoordinator::slotOnChatImage(const ChatImageStruct &msg)
{
    ChatMessage m;
    m.from_uid = msg.from_uid;
    m.to_uid = msg.to_uid;
    m.type = 1;
    m.image_id = msg.image_id;
    m.image_width = msg.width;
    m.image_height = msg.height;
    m.image_ext = msg.ext;
    m.content = msg.caption;
    m.timestamp = msg.timestamp;
    if (_max_received_ts)
        *_max_received_ts = qMax(*_max_received_ts, msg.timestamp);
    m.status = 1;
    m.client_msg_id = msg.image_id;

    if (ImageDownloadMgr::Instance().IsCached(msg.image_id))
    {
        m.image_path = ImageDownloadMgr::Instance().GetCachePath(msg.image_id, msg.ext);
    }

    DbThreadManager::Instance().SaveMessage(m);

    if (_chat_model != nullptr &&
       ((m.from_uid == _target_uid && m.to_uid == _current_uid) ||
        (m.from_uid == _current_uid && m.to_uid == _target_uid)))
    {
        _chat_model->AddMessage(m);
    }

    if (m.image_path.isEmpty())
    {
        ImageDownloadMgr::Instance().Request(msg.image_id, 0, msg.ext);
    }
}

void FileCoordinator::slotOnImageDownloadRsp(const ImageDownloadRspStruct &rsp)
{
    if (!_chat_model) return;
    ImageDownloadMgr::Instance().OnDownloadRsp(rsp.error, rsp.image_id, rsp.offset);
    if (rsp.error != 0) {
        qWarning() << "[FileCoordinator] image download failed: id=" << rsp.image_id << "error=" << rsp.error;
    }
}

void FileCoordinator::openImageViewer(const QString &imageId)
{
    QVariantList list;
    int current = 0;
    int idx = 0;

    if (_chat_model != nullptr)
    {
        const auto messages = _chat_model->GetAllMessages();
        for (const auto &m : messages)
        {
            if (m.type != 1) continue;
            if (m.recalled) continue;
            if (m.image_id == imageId) current = idx;

            QVariantMap entry;
            entry["imageId"] = m.image_id;
            entry["imagePath"] = m.image_path;
            entry["caption"] = m.content;
            list.append(entry);
            ++idx;
        }
    }

    emit sigShowImageViewer(list, current);
}

QVariantList FileCoordinator::getImageListForViewer() const
{
    QVariantList list;
    if (_chat_model != nullptr)
    {
        const auto messages = _chat_model->GetAllMessages();
        for (const auto &m : messages)
        {
            if (m.type != 1) continue;
            if (m.recalled) continue;

            QVariantMap entry;
            entry["imageId"] = m.image_id;
            entry["imagePath"] = m.image_path;
            entry["caption"] = m.content;
            list.append(entry);
        }
    }
    return list;
}
