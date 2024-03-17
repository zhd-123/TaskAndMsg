#ifndef DOWNLOADMANAGER_H
#define DOWNLOADMANAGER_H

#include <QObject>
#include <QList>
#include <QVariant>
#include <QMutex>
#include <QMutexLocker>
#include "util_global.h"
#include "downloadconfig.h"

class RequestTask;
class UTIL_EXPORT DownloadManager : public QObject
{
    Q_OBJECT
public:
    enum DownloadStatus {
        Downloading = 0,
        DownloadFailed,
        DownloadSuccess,
    };
    Q_ENUM(DownloadStatus)
    explicit DownloadManager(QObject *parent = nullptr);

    void setConfig(const QString &configFile, const QString &filePath, bool downloader=false);
    void startDownload(const QString &url, const QMap<QString, QString> &reqParmMap);

    int downloadProgress() { return mProgress; }
    bool isStartDownload(); // 是否经过了下载
    bool isInDownloading(); // 是否在下载中
    void stopDownload();

    void downloadFile(const QStringList& urls);
private:
    void reset();
    void requestFileList(const QString &url, const QMap<QString, QString> &reqParmMap);
    void requestFileInfo(const QString &url);
    void startSegmentDownload();
    void abortSegmentDownload();

    void fileDownload();
    void prepareForDownload();
    void segmentDownload(const QString &url, int index);

    void segmentFinish(int index, bool success, int state);
    void downloadFinished();
    void retryDownload();
    bool verifyFile();

private slots:
    void onGetRawHeaderMap(const QMap<QByteArray, QByteArray> &headerMap);
    void onRequestFileListFinished(int state, int code,const QByteArray &data, QVariant opaque);
    void onRequestFileInfoFinished(int state, int code,const QByteArray &data, QVariant opaque);
    void onSegmentDownloadFinished(int state, int code,const QByteArray &data, QVariant opaque);

signals:
    void progress(int status, int value);
private:
    ConfigParam mConfigParam;
    ConfigParam mCurPackageParam;
    QString mFileListUrl;
    QMap<QString, QString> mReqParmMap;
    QList<QString> mFileUrlList;
    int mFileIndex;
    mutable QList<RequestTask*> mRequestList;

    int mFileFailTimes; // 下载文件失败次数
    int mGetFileListTimes;
    int mGetFileInfoTimes;
    QMutex mMutex;
    // 是否获取到下载信息
    bool mIsGetFileList;
    bool mIsGetFileInfo;

    int mProgress;
    QString mFilePath; // 下载文件
    QString mDownloadConfigFile;
    bool mSegmentFail;

    bool mStartDownload;
    bool mInDownloading;
    bool mIsDownloader; // 是否下载器，channelid是从库读取，否则从注册表
    quint64 mAlreadyDownload;
};

#endif // DOWNLOADMANAGER_H
