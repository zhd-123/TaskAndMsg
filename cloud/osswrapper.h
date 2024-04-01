#ifndef OSSWRAPPER_H
#define OSSWRAPPER_H

#include <QObject>
#include <QThread>
#include <QVariant>
#include <QMutex>
#include "alibabacloud/oss/OssClient.h"
#include "alibabacloud/oss/OssFwd.h"
#include "alibabacloud/oss/Const.h"
#include "alibabacloud/oss/model/InitiateMultipartUploadResult.h"
#include "alibabacloud/oss/model/InitiateMultipartUploadRequest.h"
#include "alibabacloud/oss/model/UploadPartRequest.h"
#include "alibabacloud/oss/model/PutObjectRequest.h"
#include "alibabacloud/oss/model/PutObjectResult.h"
#include "alibabacloud/oss/model/AbortMultipartUploadRequest.h"
#include "alibabacloud/oss/model/ListPartsRequest.h"
#include "alibabacloud/oss/model/ListPartsResult.h"
#include "alibabacloud/oss/model/CompleteMultipartUploadRequest.h"
#include "alibabacloud/oss/model/CompleteMultipartUploadResult.h"
#include "cloudpredefine.h"

using namespace AlibabaCloud::OSS;

class RequestTask;
class OssWrapper : public QObject
{
    Q_OBJECT
public:
    struct ProgressUserData {
        OssWrapper *inst;
        int partIndex;
        ProgressUserData() {
            partIndex = 0;
            inst = nullptr;
        }
        ProgressUserData(OssWrapper *inst, int partIndex) {
            this->partIndex = partIndex;
            this->inst = inst;
        }
    };
    enum TransferType {
        Upload_File = 0,
        Upload_Thumbnail,
        Download_File,
        Download_Thumbnail,
    };

    OssWrapper(TransferType type, QObject *parent=nullptr);
    ~OssWrapper();

    Q_INVOKABLE void uploadFile(const OssUploadParam &uploadParam);
    Q_INVOKABLE void uploadFileDone();
    Q_INVOKABLE void uploadThumbnail(const OssUploadParam &uploadParam);
    Q_INVOKABLE void downloadFile(const OssDownloadParam& downloadParam);
    Q_INVOKABLE void downloadThumbnail(const OssDownloadParam& downloadParam);
    Q_INVOKABLE void abort();

    void setUploadParam(const OssUploadParam &param, int partIndex); // partIndex 用于分片完成进行save
    OssUploadParam getUploadParam();

    void setDownloadParam(const OssDownloadParam &param, int partIndex); // partIndex 用于分片完成进行save
    OssDownloadParam getDownloadParam();
    void progressCallback(int partIndex, quint64 increment, quint64 transfered, quint64 total);
    TransferType getType();
private:
    void segmentDownload(const QString &url, int index);
    void abortSegmentDownload();
    Q_INVOKABLE bool verifyFile();
    QString getDownloadTempFile(const OssDownloadParam &param);
signals:
    void finished();
    void saveUploadParam(const OssUploadParam &param);
    void saveDownloadParam(const OssDownloadParam &param);
    void uploadResult(bool success, const OssUploadParam &param);
    void downloadResult(bool success, const OssDownloadParam &param);
    void uploadProgress(const OssUploadParam &param);
    void downloadProgress(const OssDownloadParam &param);
    void downloadThumbnailDone(const OssDownloadParam &param);
public slots:
    void downLoadThumbnailFinished(int state, int code,const QByteArray &data, QVariant opaque);
    void onSegmentDownloadFinished(int state, int code,const QByteArray &data, QVariant opaque);

private:
    OssClient *mClient;
    QThread mThread;
    TransferType mType;
    OssUploadParam mUploadParam;
    OssDownloadParam mDownloadParam;
    QList<ProgressUserData *> mUserDataList;
    // 限制uploaddone只调用一次
    bool uploadDone;
    QList<RequestTask*> mRequestList;
    bool mAbort;
    QMutex mTaskMutex;
    quint64 mAlreadyUpload;
    quint64 mAlreadyDownload;
};

#endif // OSSWRAPPER_H
