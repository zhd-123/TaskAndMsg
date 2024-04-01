#include "transportcontroller.h"
#include <QMetaObject>
#include <QDir>
#include <QDebug>
#include "serializeconfig.h"
#include "fileoperationtool.h"
#include "./src/mainwindow.h"
#include "cloudmanager.h"

TransportController::TransportController(MainWindow*mainWindow, QObject *parent)
    : QObject{parent}
    , mMainWindow(mainWindow)
{
    qRegisterMetaType<OssDownloadParam>("OssDownloadParam");
}

TransportController::~TransportController()
{
}

void TransportController::additionUploadParam(OssUploadParam *uploadParam)
{
    QString configFile = CloudPredefine::getUploadConfigFile(uploadParam->fileInfo.fileId);
    if (!QFileInfo(configFile).exists()) {
        quint64 fileSize = QFileInfo(uploadParam->fileInfo.filePath).size();
        quint64 partSize = DefaultPartSize * 5;
        int partCount = static_cast<int>(fileSize / partSize);
        // Calculate how many parts to be divided
        if (fileSize % partSize != 0) {
            partCount++;
        }
        uploadParam->partArray.clear();
        uploadParam->partTotalNum = partCount;
        uploadParam->fileInfo.fileSize = fileSize;
        for (int i = 0; i < partCount; i++) {
            OssUploadPartParam partParam;
            quint64 skipBytes = partSize * i;
            quint64 size = (partSize < fileSize - skipBytes) ? partSize : (fileSize - skipBytes);
            partParam.partIndex = i;
            partParam.startPos = skipBytes;
            partParam.endPos = skipBytes + size - 1;
            uploadParam->partArray.append(partParam);
        }
        SerializeConfig::instance()->writeUploadParam(configFile, *uploadParam);
    } else {
        *uploadParam = SerializeConfig::instance()->readUploadParam(configFile);
    }
}

void TransportController::additionDownloadParam(OssDownloadParam *downloadParam)
{
    QString configFile = CloudPredefine::getDownloadConfigFile(downloadParam->fileInfo.fileId);
    QString fileIdPath = QDir::fromNativeSeparators(CloudPredefine::workSpaceDir() + QDir::separator() + QString::number(downloadParam->fileInfo.fileId));
    QString tempFile = fileIdPath + QDir::separator() + "temp.pdf";
    if (QFileInfo(configFile).exists() && !QFileInfo(tempFile).exists()) {
        // 配置文件没有删除的情况
        QFile::remove(configFile);
        (*downloadParam).downloadSize = 0;
    }
    if (!QFileInfo(configFile).exists()) {
        quint64 fileSize = downloadParam->fileInfo.fileSize;
        quint64 partSize = DefaultPartSize * 5;
        int partCount = static_cast<int>(fileSize / partSize);
        // Calculate how many parts to be divided
        if (fileSize % partSize != 0) {
            partCount++;
        }
        downloadParam->partArray.clear();
        downloadParam->partTotalNum = partCount;
        for (int i = 0; i < partCount; i++) {
            OssDownloadPartParam partParam;
            quint64 skipBytes = partSize * i;
            quint64 size = (partSize < fileSize - skipBytes) ? partSize : (fileSize - skipBytes);
            partParam.partIndex = i;
            partParam.startPos = skipBytes;
            partParam.endPos = skipBytes + size - 1;
            downloadParam->partArray.append(partParam);
        }
        SerializeConfig::instance()->writeDownloadParam(configFile, *downloadParam);
    } else {
        *downloadParam = SerializeConfig::instance()->readDownloadParam(configFile);
    }
}

void TransportController::uploadFile(const OssUploadParam& uploadParam)
{
    OssUploadParam param = uploadParam;
    additionUploadParam(&param);
    if (param.fileInfo.transFail || param.uploadSize >= param.fileInfo.fileSize) {
        QString configFile = CloudPredefine::getUploadConfigFile(param.fileInfo.fileId);
        QFile::remove(configFile); // 重新生成参数
        param = uploadParam;
        param.fileInfo.transFail = false;
        param.uploadSize = 0;
        additionUploadParam(&param);
    }
    SerializeConfig::instance()->recordTransportTask(param.fileInfo.fileId, true);
    SerializeConfig::instance()->removeWaitUploadFile(param.fileInfo.filePath);
    if (runningTaskNum(Upload_File_Prefix) >= Upload_MaxParallel_Task) {
        mCacheUploadTaskList.append(param);
        return;
    }
    uploadTask(param);
}
void TransportController::uploadTask(const OssUploadParam &param)
{
    emit startUploadTask(param);
    OssWrapper *ossInstance = new OssWrapper(OssWrapper::Upload_File);
    connect(ossInstance, &OssWrapper::finished, this, [=] {
        OssWrapper *ossInstance = qobject_cast<OssWrapper *>(sender());
        if (!ossInstance) {
            return;
        }
        ossInstance->deleteLater();
    });
    connect(ossInstance, &OssWrapper::uploadProgress, this, &TransportController::uploadProgress);
    connect(ossInstance, &OssWrapper::saveUploadParam, this, &TransportController::onSaveUploadParam);
    connect(ossInstance, &OssWrapper::uploadResult, this, &TransportController::onUploadResult);
    mOssInstanceMap.insert(Upload_File_Prefix + QString::number(param.fileInfo.fileId), ossInstance);
    QMetaObject::invokeMethod(ossInstance, "uploadFile", Qt::QueuedConnection, Q_ARG(OssUploadParam, param));
}

int TransportController::runningTaskNum(const QString &type)
{
    int count = 0;
    for (QString key : mOssInstanceMap.keys()) {
        if (key.contains(type)) {
           count++;
        }
    }
    return count;
}

void TransportController::onUploadResult(bool success, const OssUploadParam &param)
{
    if (!mOssInstanceMap.contains(Upload_File_Prefix + QString::number(param.fileInfo.fileId))) {
        return;
    }
    mOssInstanceMap.remove(Upload_File_Prefix + QString::number(param.fileInfo.fileId));
    OssWrapper *ossInstance = qobject_cast<OssWrapper *>(sender());
    if (!ossInstance) {
        return;
    }
    emit ossUploadResult(success, param);
    if (success) {
        clearUploadParam(param.fileInfo.fileId);
        SerializeConfig::instance()->removeTransportTask(param.fileInfo.fileId, true);
    } else {
        disconnect(ossInstance, &OssWrapper::uploadResult, this, &TransportController::onUploadResult);
        disconnect(ossInstance, &OssWrapper::uploadProgress, this, &TransportController::uploadProgress);
//        QMetaObject::invokeMethod(ossInstance, "abort", Qt::QueuedConnection);
        ossInstance->abort();
    }
    if (mCacheUploadTaskList.size() > 0) {
        uploadTask(mCacheUploadTaskList.takeAt(0));
    }
}

void TransportController::onDownloadResult(bool success, const OssDownloadParam &param)
{
    if (!mOssInstanceMap.contains(Download_File_Prefix + QString::number(param.fileInfo.fileId))) {
        return;
    }
    mOssInstanceMap.remove(Download_File_Prefix + QString::number(param.fileInfo.fileId));
    OssWrapper *ossInstance = qobject_cast<OssWrapper *>(sender());
    if (!ossInstance) {
        return;
    }
    emit ossDownloadResult(success, param);
    if (success) {
        clearDownloadParam(param.fileInfo.fileId);
        SerializeConfig::instance()->removeTransportTask(param.fileInfo.fileId, false);
    } else {
        disconnect(ossInstance, &OssWrapper::downloadResult, this, &TransportController::onDownloadResult);
        disconnect(ossInstance, &OssWrapper::downloadProgress, this, &TransportController::downloadProgress);
        ossInstance->abort();
    }
    if (mCacheDownloadTaskList.size() > 0) {
        downloadTask(mCacheDownloadTaskList.takeAt(0));
    }
}

void TransportController::uploadThumbnail(const OssUploadParam& uploadParam)
{
    OssWrapper *ossInstance = new OssWrapper(OssWrapper::Upload_Thumbnail);
    connect(ossInstance, &OssWrapper::finished, this, [=] {
        OssWrapper *ossInstance = qobject_cast<OssWrapper *>(sender());
        if (!ossInstance) {
            return;
        }
        mOssInstanceMap.remove(Upload_Thumbnail_Prefix + QString::number(ossInstance->getUploadParam().fileInfo.fileId));
        ossInstance->deleteLater();
    });
    mOssInstanceMap.insert(Upload_Thumbnail_Prefix + QString::number(uploadParam.fileInfo.fileId),ossInstance);
    QMetaObject::invokeMethod(ossInstance, "uploadThumbnail", Qt::QueuedConnection, Q_ARG(OssUploadParam, uploadParam));
}

void TransportController::downloadFile(const OssDownloadParam &downloadParam)
{
    OssDownloadParam param = downloadParam;
    additionDownloadParam(&param);
    if (param.fileInfo.transFail || param.downloadSize >= param.fileInfo.fileSize) {
        QString configFile = CloudPredefine::getDownloadConfigFile(param.fileInfo.fileId);
        QFile::remove(configFile); // 重新生成参数
        QString fileIdPath = QDir::fromNativeSeparators(CloudPredefine::workSpaceDir() + QDir::separator() + QString::number(param.fileInfo.fileId));
        QString tempFile = fileIdPath + QDir::separator() + "temp.pdf";
        if (!param.saveDir.isEmpty()) {
            tempFile = param.saveDir + QDir::separator() + "temp.pdf";
        }
        if (QFile::exists(tempFile)) {
            QFile::remove(tempFile);
        }
        param = downloadParam;
        param.downloadSize = 0;
        param.fileInfo.transFail = false;
        additionDownloadParam(&param);
    }
    SerializeConfig::instance()->recordTransportTask(param.fileInfo.fileId, false);
    if (runningTaskNum(Download_File_Prefix) >= Download_MaxParallel_Task) {
        mCacheDownloadTaskList.append(param);
        return;
    }
    downloadTask(param);
}
void TransportController::downloadTask(const OssDownloadParam &param)
{
    OssWrapper *ossInstance = new OssWrapper(OssWrapper::Download_File);
    connect(ossInstance, &OssWrapper::finished, this, [=] {
        OssWrapper *ossInstance = qobject_cast<OssWrapper *>(sender());
        if (!ossInstance) {
            return;
        }
        ossInstance->deleteLater();
    });
    connect(ossInstance, &OssWrapper::downloadProgress, this, &TransportController::downloadProgress);
    connect(ossInstance, &OssWrapper::saveDownloadParam, this, &TransportController::onSaveDownloadParam);
    connect(ossInstance, &OssWrapper::downloadResult, this, &TransportController::onDownloadResult);

    mOssInstanceMap.insert(Download_File_Prefix + QString::number(param.fileInfo.fileId),ossInstance);
    ossInstance->downloadFile(param);
}

void TransportController::downloadThumbnail(const OssDownloadParam &downloadParam)
{
    OssWrapper *ossInstance = new OssWrapper(OssWrapper::Download_Thumbnail);
    connect(ossInstance, &OssWrapper::finished, this, [=] {
        OssWrapper *ossInstance = qobject_cast<OssWrapper *>(sender());
        if (!ossInstance) {
            return;
        }
        mOssInstanceMap.remove(Download_Thumbnail_Prefix + QString::number(ossInstance->getDownloadParam().fileInfo.fileId));
        ossInstance->deleteLater();
    });
    connect(ossInstance, &OssWrapper::downloadThumbnailDone, this, &TransportController::downloadThumbnailDone);
    ossInstance->downloadThumbnail(downloadParam);
    mOssInstanceMap.insert(Download_Thumbnail_Prefix + QString::number(downloadParam.fileInfo.fileId),ossInstance);
}

void TransportController::clearUploadParam(qint64 fileId)
{
    QFile::remove(CloudPredefine::getUploadConfigFile(fileId));
}

void TransportController::clearDownloadParam(qint64 fileId)
{
    QFile::remove(CloudPredefine::getDownloadConfigFile(fileId));
}

void TransportController::onSaveUploadParam(const OssUploadParam &param)
{
    QString configFile = CloudPredefine::getUploadConfigFile(param.fileInfo.fileId);
    SerializeConfig::instance()->writeUploadParam(configFile, param);
}

void TransportController::onSaveDownloadParam(const OssDownloadParam &param)
{
    QString configFile = CloudPredefine::getDownloadConfigFile(param.fileInfo.fileId);
    SerializeConfig::instance()->writeDownloadParam(configFile, param);
}

void TransportController::abort()
{
    for (OssWrapper *ossInstance : mOssInstanceMap.values()) {
        OssWrapper::TransferType type = ossInstance->getType();
        if (OssWrapper::Upload_File == type) {
            disconnect(ossInstance, &OssWrapper::uploadResult, this, &TransportController::onUploadResult);
            disconnect(ossInstance, &OssWrapper::uploadProgress, this, &TransportController::uploadProgress);
        } else if (OssWrapper::Download_File == type) {
            disconnect(ossInstance, &OssWrapper::downloadResult, this, &TransportController::onDownloadResult);
            disconnect(ossInstance, &OssWrapper::downloadProgress, this, &TransportController::downloadProgress);
        }
        ossInstance->abort();
    }
    mOssInstanceMap.clear();
}

void TransportController::abortFileUpload(qint64 fileId)
{
    QString key = Upload_File_Prefix + QString::number(fileId);
    if (mOssInstanceMap.contains(key)) {
        OssWrapper *ossInstance = mOssInstanceMap.take(key);
        OssUploadParam param = ossInstance->getUploadParam();
        param.fileInfo.transFail = true;
        ossInstance->setUploadParam(param, -1);
        disconnect(ossInstance, &OssWrapper::uploadResult, this, &TransportController::onUploadResult);
        disconnect(ossInstance, &OssWrapper::uploadProgress, this, &TransportController::uploadProgress);
        ossInstance->abort();
        emit ossUploadResult(false, param);
    }
}
void TransportController::abortFileDownload(qint64 fileId)
{
    QString key = Download_File_Prefix + QString::number(fileId);
    if (mOssInstanceMap.contains(key)) {
        OssWrapper *ossInstance = mOssInstanceMap.take(key);
        OssDownloadParam param = ossInstance->getDownloadParam();
        param.fileInfo.transFail = true;
        ossInstance->setDownloadParam(param, -1);
        disconnect(ossInstance, &OssWrapper::downloadResult, this, &TransportController::onDownloadResult);
        disconnect(ossInstance, &OssWrapper::downloadProgress, this, &TransportController::downloadProgress);
        ossInstance->abort();
        emit ossDownloadResult(false, param);
    }
}
bool TransportController::checkFileDownloading(qint64 fileId)
{
    QString key = Download_File_Prefix + QString::number(fileId);
    return mOssInstanceMap.contains(key);
}

bool TransportController::haveUploadTask()
{
    bool haveTask = false;
    for (QString key : mOssInstanceMap.keys()) {
        if (key.contains(Upload_File_Prefix)) {
            haveTask = true;
            break;
        }
    }

    return haveTask||(SerializeConfig::instance()->loadWaitUploadFile().size() > 0);
}
