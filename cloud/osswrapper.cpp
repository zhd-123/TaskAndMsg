#include "osswrapper.h"
#include <iostream>
#include <fstream>
#include <QDebug>
#include <QImage>
#include <QDir>
#include "requestutil.h"
#include "./utils/filedownload/downloadconfig.h"
#include "./src/usercenter/usercentermanager.h"

void ProgressCallback(size_t increment, int64_t transfered, int64_t total, void* userData)
{
    OssWrapper::ProgressUserData *pUserData = (OssWrapper::ProgressUserData *)userData;
    if (!pUserData) {
        return;
    }
//    qDebug() << "index:"<< pUserData->partIndex <<" total:" << total << ", progress:" << transfered*1.0 / total
//             << "transfered:"<< transfered <<" increment:" << increment;
    pUserData->inst->progressCallback(pUserData->partIndex, increment, transfered, total);
}

OssWrapper::OssWrapper(TransferType type, QObject *parent)
    : QObject(parent)
    , mType(type)
    , mClient(nullptr)
    , mAbort(false)
{
    qDebug() << this << "OssWrapper()" << type;
    if (mType == Upload_File || mType == Upload_Thumbnail) {
        setParent(nullptr);
        moveToThread(&mThread);
        mThread.start();
    }
}

OssWrapper::~OssWrapper()
{
    qDebug() << this << "~OssWrapper()";
    if (mType == Upload_File || mType == Upload_Thumbnail) {
        for (ProgressUserData * userData : mUserDataList) {
            if (userData) {
                delete userData;
            }
        }
        mUserDataList.clear();
        if (mThread.isRunning())
        {
            mThread.terminate();
            mThread.wait(5000);
        }
    }
}

void OssWrapper::uploadFile(const OssUploadParam &uploadParam)
{
    uploadDone = false;
    mUploadParam = uploadParam;
    ClientConfiguration config;
    config.verifySSL = false;
    mClient = new OssClient(mUploadParam.credentials.endpoint.toStdString(), mUploadParam.credentials.accessKeyId.toStdString(),
                            mUploadParam.credentials.accessKeySecret.toStdString(), mUploadParam.credentials.securityToken.toStdString(), config);
    if (mUploadParam.uploadId.isEmpty()) {
        InitiateMultipartUploadRequest initUploadRequest(mUploadParam.credentials.bucketName.toStdString(), mUploadParam.fileInfo.fileRemotePath.toStdString());
        //initUploadRequest.MetaData().addHeader("x-oss-storage-class", "Standard");
        initUploadRequest.setSequential(false);
        InitiateMultipartUploadOutcome uploadIdResult = mClient->InitiateMultipartUpload(initUploadRequest);
        mUploadParam.uploadId = uploadIdResult.result().UploadId().c_str();
    }

    // Upload multiparts to bucket
    if (mUploadParam.uploadSize >= mUploadParam.fileInfo.fileSize) {
        uploadFileDone();
        return;
    }
    mAlreadyUpload = 0;
    for (int i = 0; i < mUploadParam.partTotalNum; i++) {
        std::wstring UploadFilePath = mUploadParam.fileInfo.filePath.replace("\\", "/").toStdWString(); //toLocal8Bit
        std::shared_ptr<std::iostream> content = std::make_shared<std::fstream>(UploadFilePath, std::ios::in|std::ios::binary);
        // check if need upload
        if (mUploadParam.partArray[i].endPos <= mUploadParam.partArray[i].startPos) {
            continue;
        }
        content->seekg(mUploadParam.partArray[i].startPos, std::ios::beg);

        quint64 partSize = mUploadParam.partArray[i].endPos - mUploadParam.partArray[i].startPos + 1;
        UploadPartRequest uploadPartRequest(mUploadParam.credentials.bucketName.toStdString(), mUploadParam.fileInfo.fileRemotePath.toStdString(), content);
        uploadPartRequest.setContentLength(partSize);
        uploadPartRequest.setUploadId(mUploadParam.uploadId.toStdString());
        uploadPartRequest.setPartNumber(i + 1);

        char *buf = new char[partSize];
        content->read(buf, partSize);
        std::string md5 = ComputeContentMD5(buf, partSize).c_str();
        delete [] buf;
        content->seekg(mUploadParam.partArray[i].startPos, std::ios::beg);
        uploadPartRequest.setContentMd5(md5);

        ProgressUserData *userData = new ProgressUserData(this, i);
        mUserDataList.append(userData);
        TransferProgress callback = { ProgressCallback, userData };
        uploadPartRequest.setTransferProgress(callback);
        auto uploadPartOutcome = mClient->UploadPart(uploadPartRequest);
        bool result = uploadPartOutcome.isSuccess();
        qDebug() <<"UploadPart "<< QString::number(i + 1);
        if (result) {
            mUploadParam.partArray[i].eTag = uploadPartOutcome.result().ETag().c_str();
            emit saveUploadParam(mUploadParam);
        } else { // 上传分片失败
            mUploadParam.fileInfo.transFail = true;
            emit uploadResult(false, mUploadParam);
            qDebug() <<"UploadPart "<< QString::number(i + 1) << "fail";
            break;
        }
    }
    qDebug() <<"upload done";
}

void OssWrapper::progressCallback(int partIndex, quint64 increment, quint64 transfered, quint64 total)
{
    mUploadParam.partArray[partIndex].startPos += increment;
    if (mUploadParam.partArray[partIndex].startPos > mUploadParam.partArray[partIndex].endPos) {
        mUploadParam.partArray[partIndex].startPos -= 1;
    }
    mUploadParam.uploadSize += increment;
    OssUploadParam param = mUploadParam;
    param.uploadSize = qMax(mAlreadyUpload, param.uploadSize);
    mAlreadyUpload = param.uploadSize;
    if (!mAbort) {
        emit uploadProgress(param);
    }
    setUploadParam(mUploadParam, partIndex);
}

OssWrapper::TransferType OssWrapper::getType()
{
    return mType;
}

void OssWrapper::uploadFileDone()
{
    if (!uploadDone) {
        uploadDone = true;
        // Complete to upload multiparts
        CompleteMultipartUploadRequest request(mUploadParam.credentials.bucketName.toStdString(), mUploadParam.fileInfo.fileRemotePath.toStdString());
        request.setUploadId(mUploadParam.uploadId.toStdString());
        PartList partList;
        for (int i = 0; i < mUploadParam.partTotalNum; i++) {
            Part part(i + 1, mUploadParam.partArray[i].eTag.toStdString());
            partList.push_back(part);
        }
        request.setPartList(partList);

        auto outcome = mClient->CompleteMultipartUpload(request);
        bool result = outcome.isSuccess();
        if (!result) {
            qWarning() <<"uploadFileDone fail";
            mUploadParam.fileInfo.transFail = true;
            emit uploadResult(false, mUploadParam);
        } else {
            emit uploadResult(true, mUploadParam);
        }
        qDebug() <<"uploadFileDone done!!!!";
        emit finished();
    }
}

void OssWrapper::uploadThumbnail(const OssUploadParam &uploadParam)
{
    mUploadParam = uploadParam;
    if (QFileInfo(mUploadParam.fileInfo.logoPath).exists()) {
        ClientConfiguration config;
        config.verifySSL = false;
        mClient = new OssClient(mUploadParam.credentials.endpoint.toStdString(), mUploadParam.credentials.accessKeyId.toStdString(),
                                mUploadParam.credentials.accessKeySecret.toStdString(), mUploadParam.credentials.securityToken.toStdString(), config);
        std::wstring UploadFilePath = mUploadParam.fileInfo.logoPath.replace("\\", "/").toStdWString(); //toLocal8Bit
        std::shared_ptr<std::iostream> content = std::make_shared<std::fstream>(UploadFilePath, std::ios::in | std::ios::binary);
        PutObjectRequest request(mUploadParam.credentials.bucketName.toStdString(), mUploadParam.fileInfo.logoRemotePath.toStdString(), content);

        auto outcome = mClient->PutObject(request);
    }
    emit finished();
}

void OssWrapper::downloadThumbnail(const OssDownloadParam& downloadParam)
{
    mDownloadParam = downloadParam;
    RequestTask *task = new RequestTask(this);
    connect(task, &RequestTask::requestFinished, task, &RequestTask::deleteLater);
    connect(task, &RequestTask::finished, this, &OssWrapper::downLoadThumbnailFinished);

    RequestTask::RequestStruct reqStruct;
    RequestResource::setSslConfig(&reqStruct.request);
//    RequestResource::addCommonRawHeaders(&reqStruct.request, false, "");
    reqStruct.request.setUrl(QUrl(mDownloadParam.fileInfo.logoUrl));
    reqStruct.timeout = 30000;
    task->setRequest(reqStruct);
    task->setNetworkTip(false);
    RequestManager::instance()->addTask(task, false);
}

void OssWrapper::downLoadThumbnailFinished(int state, int code,const QByteArray &data, QVariant opaque)
{
    if (RequestTask::RequestState::Success == (RequestTask::RequestState)state) {
        QImage image;
        image.loadFromData(data);
        QString fileIdPath = QDir::fromNativeSeparators(CloudPredefine::workSpaceDir() + QDir::separator() + QString::number(mDownloadParam.fileInfo.fileId));
        image.save(fileIdPath + QDir::separator() + mDownloadParam.fileInfo.logoName);
        emit downloadThumbnailDone(mDownloadParam);
    }
    emit finished();
}

void OssWrapper::downloadFile(const OssDownloadParam& downloadParam)
{
    mDownloadParam = downloadParam;
    QString tempFile = getDownloadTempFile(mDownloadParam);
    // create empty file
    if (!QFile::exists(tempFile)) {
        QFile file(tempFile);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            return;
        }
        file.close();
    }
    mAlreadyDownload = 0;
    for (int i = 0; i < mDownloadParam.partTotalNum; i++) {
        // check if need download
        if (mDownloadParam.partArray[i].endPos <= mDownloadParam.partArray[i].startPos) {
            continue;
        }
        segmentDownload(mDownloadParam.fileInfo.fileUrl, i);
    }
    if (mRequestList.size() <= 0) {
        // 已经下载好
        QMetaObject::invokeMethod(this, "verifyFile", Qt::QueuedConnection);
    }
}

void OssWrapper::segmentDownload(const QString &url, int index)
{
    RequestTask *task = new RequestTask(this);
    connect(task, &RequestTask::requestFinished, task, &RequestTask::deleteLater);
    connect(task, &RequestTask::finished, this, [&](int state, int code,const QByteArray &data, QVariant opaque) {
        RequestTask *reqTask = dynamic_cast<RequestTask *>(sender());
        if (reqTask) {
            QMutexLocker locker(&mTaskMutex);
            mRequestList.removeOne(reqTask);
        }
        onSegmentDownloadFinished(state, code, data, opaque);
    });
    connect(task, &RequestTask::downloadProgress, this, [&](qint64 bytesReceived, qint64 bytesTotal) {
        OssDownloadParam param = mDownloadParam;
        param.downloadSize += bytesReceived;
        param.downloadSize = qMax(mAlreadyDownload, param.downloadSize);
        mAlreadyDownload = param.downloadSize;
        if (!mAbort) {
            emit downloadProgress(param);
        }
    });
    RequestTask::RequestStruct reqStruct;
    RequestResource::setSslConfig(&reqStruct.request);
    reqStruct.request.setRawHeader("Range", "bytes="+ QString::number(mDownloadParam.partArray[index].startPos).toLatin1() + "-"
                                   + QString::number(mDownloadParam.partArray[index].endPos).toLatin1());
    QString serverHost = RequestResource::instance()->cloudServerHost();
    if (UserCenterManager::instance()->userContext().mUserInfo.companyUser) {
        serverHost = RequestResource::instance()->cloudEnterpriseServerHost();
    }
    reqStruct.request.setRawHeader("Referer", serverHost.toLatin1());
    // request param
    QMap<QString, QString> reqParmMap;
    QString urlStr = RequestResource::formatUrl(url, reqParmMap);
    reqStruct.request.setUrl(QUrl(urlStr));
    reqStruct.type = RequestTask::RequestType::Get;
    reqStruct.timeout = 60 * 60 * 1000 * 12;
    reqStruct.opaque.setValue(index);
    task->setRequest(reqStruct);
    task->setNetworkTip(false);
    RequestManager::instance()->addTask(task, false);
    QMutexLocker locker(&mTaskMutex);
    mRequestList.append(task);
}

void OssWrapper::onSegmentDownloadFinished(int state, int code,const QByteArray &data, QVariant opaque)
{
    int index = opaque.toInt();
    if (RequestTask::RequestState::Success == (RequestTask::RequestState)state) {
        bool openfileSuccess = false;
        {
            QString tempFile = getDownloadTempFile(mDownloadParam);
            QFile file(tempFile);
            if (file.open(QIODevice::WriteOnly | QIODevice::Append)) {
                openfileSuccess = true;
                file.seek(mDownloadParam.partArray[index].startPos);
                file.write(data);
                file.close();

                int dataLen = data.length();
                int startPos = mDownloadParam.partArray[index].startPos;
                mDownloadParam.partArray[index].startPos = startPos + dataLen - 1;
                mDownloadParam.downloadSize += dataLen;
                setDownloadParam(mDownloadParam, index);
                verifyFile();
            } else {
                qDebug() << index << " openfile fail";
            }
        }
        if (!openfileSuccess) {
            QTimer::singleShot(1, this, [=] { onSegmentDownloadFinished(state, code, data, opaque); });
        }
    } else {
        qDebug() << index << "download failed";
        mDownloadParam.fileInfo.transFail = true;
        emit downloadResult(false, mDownloadParam);
    }
}

bool OssWrapper::verifyFile()
{
    if (mDownloadParam.downloadSize >= mDownloadParam.fileInfo.fileSize) {
        // check crc
        QString tempFile = getDownloadTempFile(mDownloadParam);
        QFile file(tempFile);
        quint64 crc = 0;
        if (file.open(QIODevice::ReadOnly)) {
            QByteArray ba = file.readAll();
            crc = CalcCRC64(0, ba.data(), ba.size());
            file.close();
        }

        if (mDownloadParam.fileInfo.fileCrc != 0 && mDownloadParam.fileInfo.fileCrc != crc) {
            // tips failed
            if (QFile::exists(tempFile)) {
                QFile::remove(tempFile);
            }
            mDownloadParam.fileInfo.transFail = true;
            emit downloadResult(false, mDownloadParam);
            qDebug() << "verifyFile crc no eq " << mDownloadParam.fileInfo.fileCrc << " " << crc;
            return false;
        }
        emit downloadResult(true, mDownloadParam);
        emit finished();
        return true;
    }
    return false;
}

QString OssWrapper::getDownloadTempFile(const OssDownloadParam &param)
{
    QString fileIdPath = QDir::fromNativeSeparators(CloudPredefine::workSpaceDir() + QDir::separator() + QString::number(mDownloadParam.fileInfo.fileId));
    QString tempFile = QDir::fromNativeSeparators(fileIdPath + QDir::separator() + "temp.pdf");
    if (!param.saveDir.isEmpty()) {
        tempFile = QDir::fromNativeSeparators(param.saveDir + QDir::separator() + "temp.pdf");
    }
    return tempFile;
}

void OssWrapper::abort()
{
    mAbort = true;
    if (mType == Upload_File) {
        emit saveUploadParam(mUploadParam);// 先保存再退出
        mClient->DisableRequest();
    } else if (mType == Upload_Thumbnail){
        mClient->DisableRequest();
    } else if (mType == Download_File){
        emit saveDownloadParam(mDownloadParam);// 先保存再退出
        abortSegmentDownload();
    }
    emit finished();
}

void OssWrapper::abortSegmentDownload()
{
    QMutexLocker locker(&mTaskMutex);
    for (RequestTask *task : mRequestList) {
        task->abort();
    }
    mRequestList.clear();
}

void OssWrapper::setUploadParam(const OssUploadParam &param, int partIndex)
{
    mUploadParam = param;
    if (partIndex < 0 || mUploadParam.partArray.size() <= partIndex) {
        return;
    }
    if (mUploadParam.partArray[partIndex].startPos >= mUploadParam.partArray[partIndex].endPos) {
        emit saveUploadParam(mUploadParam);
    }
    if (mUploadParam.uploadSize >= mUploadParam.fileInfo.fileSize) {
        QMetaObject::invokeMethod(this, "uploadFileDone", Qt::QueuedConnection);
    }
}

OssUploadParam OssWrapper::getUploadParam()
{
    return mUploadParam;
}

void OssWrapper::setDownloadParam(const OssDownloadParam &param, int partIndex)
{
    mDownloadParam = param;
    if (partIndex < 0 || mDownloadParam.partArray.size() <= partIndex) {
        return;
    }
    if (mDownloadParam.partArray[partIndex].startPos >= mDownloadParam.partArray[partIndex].endPos) {
        emit saveDownloadParam(mDownloadParam);
    }
}

OssDownloadParam OssWrapper::getDownloadParam()
{
    return mDownloadParam;
}
