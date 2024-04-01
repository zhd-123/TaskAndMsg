#ifndef TRANSPORTCONTROLLER_H
#define TRANSPORTCONTROLLER_H

#include <QObject>
#include <QMap>
#include <QThread>
#include "cloudpredefine.h"
#include "osswrapper.h"
class MainWindow;
enum TransferState;
class TransportController : public QObject
{
    Q_OBJECT
public:
    explicit TransportController(MainWindow*mainWindow, QObject *parent = nullptr);
    ~TransportController();
    void uploadFile(const OssUploadParam& uploadParam);
    void uploadThumbnail(const OssUploadParam& uploadParam);
    void downloadFile(const OssDownloadParam& downloadParam);
    void downloadThumbnail(const OssDownloadParam& downloadParam);
    void abort();
    void abortFileUpload(qint64 fileId);
    void abortFileDownload(qint64 fileId);
    bool checkFileDownloading(qint64 fileId); // 文件是否已经在下载
    bool haveUploadTask();
private:
    void uploadTask(const OssUploadParam& param);
    void downloadTask(const OssDownloadParam& param);
    void additionUploadParam(OssUploadParam *uploadParam);
    void additionDownloadParam(OssDownloadParam *downloadParam);
    int runningTaskNum(const QString &type);
private:
    void clearUploadParam(qint64 fileId);
    void clearDownloadParam(qint64 fileId);
signals:
    void ossUploadResult(bool success, const OssUploadParam &param);
    void ossDownloadResult(bool success, const OssDownloadParam &param);
    void downloadThumbnailDone(const OssDownloadParam &param);
    // 更新ui
    void uploadProgress(const OssUploadParam &param);
    void downloadProgress(const OssDownloadParam &param);
    void transferStateSignal(qint64 fileId, TransferState state);

    void startUploadTask(const OssUploadParam &param);
public slots:
    void onSaveUploadParam(const OssUploadParam &param);
    void onSaveDownloadParam(const OssDownloadParam &param);
    void onUploadResult(bool success, const OssUploadParam &param);
    void onDownloadResult(bool success, const OssDownloadParam &param);
private:
    MainWindow* mMainWindow;
//    QThread mThread;
    // 记录所有实例，用于abort
    QMap<QString, OssWrapper *> mOssInstanceMap;
    QList<OssDownloadParam> mCacheDownloadTaskList;
    QList<OssUploadParam> mCacheUploadTaskList;
};

#endif // TRANSPORTCONTROLLER_H
