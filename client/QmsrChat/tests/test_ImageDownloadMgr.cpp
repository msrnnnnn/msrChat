#include "ImageDownloadMgr.h"
#include <QSignalSpy>
#include <QStandardPaths>
#include <gtest/gtest.h>
#include <QDateTime>

TEST(ImageDownloadMgrTest, ExpiredImageEmitsFailed)
{
    auto &mgr = ImageDownloadMgr::Instance();
    QString id = "test-expired-" + QString::number(QDateTime::currentMSecsSinceEpoch());
    mgr.Request(id);
    QSignalSpy spy(&mgr, &ImageDownloadMgr::sigImageFailed);
    mgr.OnDownloadRsp(4040, id, 0);
    EXPECT_EQ(spy.count(), 1);
    auto args = spy.takeFirst();
    EXPECT_EQ(args.at(0).toString(), id);
    EXPECT_EQ(args.at(1).toInt(), 4040);
}

TEST(ImageDownloadMgrTest, CachePathFormat)
{
    auto &mgr = ImageDownloadMgr::Instance();
    QString p = mgr.GetCachePath("uuid-1", "jpg");
    EXPECT_TRUE(p.endsWith("client_image_cache/uuid-1.jpg"));
}

TEST(ImageDownloadMgrTest, FileCompleteUpdatesCache)
{
    auto &mgr = ImageDownloadMgr::Instance();
    QString id = "uuid-cached";
    mgr.Request(id);
    QString p = "/tmp/client_image_cache/" + id + ".jpg";
    mgr.OnFileRecvComplete(id, p, true);
    EXPECT_TRUE(mgr.IsCached(id));
}