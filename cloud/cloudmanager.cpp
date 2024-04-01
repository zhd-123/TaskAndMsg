#include "cloudmanager.h"
#include "transportcontroller.h"
#include "signalcenter.h"
#include "commonutils.h"
#include "netrequestmanager.h"
#include "./src/util/historyfilesutil.h"
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>
#include <QFileDialog>
#include <QtConcurrent>
#include "serializeconfig.h"
#include "fileoperationtool.h"
#include "./src/util/guiutil.h"
#include "../pageengine/pdfcore/PDFDocument.h"
#include "../pageengine/pdfcore/PDFPageManager.h"
#include "../pageengine/pdfcore/PDFPage.h"
#include "./editor/pdfcore/headers/spdfview.h"
#include "../editor/widgets/loadingwidgetwithcancel.h"
#include "./src/usercenter/usercentermanager.h"
#include "./src/util/loginutil.h"
#include "./utils/commonutils.h"
#include "./src/mainwindow.h"
#include "./src/usercenter/limituse/limitusemgr.h"
#include "./src/usercenter/limituse/limituse_enterprise/enterpriselimituse.h"
#include "./src/basicwidget/messagewidget.h"

CloudManager::CloudManager(MainWindow *mainWindow,QObject *parent)
    : QObject{parent}
    , mMainWindow(mainWindow)
    , mLoadingWidget(nullptr)
    , mIgnoreErrorFileId(0)
    , mUploadTaskNum(0)
    , mDataLoaded(false)
{
    connect(mMainWindow->getTransportController(), &TransportController::ossUploadResult, this, &CloudManager::onOssUploadResult);
    connect(mMainWindow->getTransportController(), &TransportController::ossDownloadResult, this, &CloudManager::onOssDownloadResult);
    connect(this, &CloudManager::checkResumeUploadFileInfo, this, &CloudManager::onCheckResumeUploadFileInfo);
    connect(this, &CloudManager::checkResumeDownloadFileInfo, this, &CloudManager::onCheckResumeDownloadFileInfo);

    connect(Loginutil::getInstance(),&Loginutil::loginSignal, this, &CloudManager::onLogin);
    connect(UserCenterManager::instance(),&UserCenterManager::logout, this, &CloudManager::onLogout);

}
FilePropInfo CloudManager::getFileProp(const QString &filePath, bool hash)
{
    FilePropInfo propInfo;
    QFileInfo fileInfo(filePath);
    propInfo.size = fileInfo.size();
    propInfo.name = fileInfo.completeBaseName();
    propInfo.suffix = fileInfo.suffix();
    propInfo.path = QDir::fromNativeSeparators(filePath);
    propInfo.createTime = fileInfo.birthTime();
    propInfo.modifyTime = fileInfo.lastModified();
    propInfo.visitTime = fileInfo.lastRead();
    if (hash && !mMainWindow->isWindowRelease()) {
        QEventLoop eventLoop;
        auto future = QtConcurrent::run(&FileOperationTool::getFileHash, filePath);
        QFutureWatcher<QString> *watcher = new QFutureWatcher<QString>();
        connect(watcher, &QFutureWatcher<QString>::finished, &eventLoop, &QEventLoop::quit);
        connect(mMainWindow, &MainWindow::windowRelease, &eventLoop, &QEventLoop::quit);
        watcher->setFuture(future);
        eventLoop.exec();
        propInfo.hash = future.result();
    }
    return propInfo;
}

void CloudManager::onUploadFiles(const QStringList &fileList)
{   
    if(LimitUseMGR->isLimit(LimitUseMgr::OP_UCloud)){
        if(LimitUseMGR->takeUCloudLimit() != LimitUseMgr::Continue){
            return ;
        }
    }

    // 判断文件大小是否超限
    quint64 totalSize = 0;
    QStringList filterFileList;
    QStringList fileListOver10m;
    quint64 overFileSize = LimitUseMGR->limitCloudFileSize();
    bool hasPower = UserCenterManager::instance()->haveCloudPrivalege();
	for (QString path : fileList) {
        if (QFileInfo(path).size() <= 0) {
            MessageWidget win(QStringList{GuiUtil::getFileName1(path,true) + " " + tr("size is zero")},QStringList{tr("OK"),"",""});
            win.resetIconImage(MessageWidget::IT_Warn);
            win.exec();
            continue;
        }
        if (mWaitForUploadFileList.contains(path)) {
            continue;
        }

        auto fileSize = QFileInfo(path).size();

        if(!hasPower){
            if(fileSize > overFileSize){
                fileListOver10m.push_back(path);
                continue;
            }
        }
        filterFileList.append(path);
        totalSize += fileSize;
    }
    bool isOverCloudSpace = totalSize > (mCacheCloudDiskInfo.totalSize - mCacheCloudDiskInfo.usedSize);
    if (isOverCloudSpace || !fileListOver10m.isEmpty()) {
        QStringList msg;
        QStringList btnList = QStringList{tr("OK"),"",""};
        if (hasPower && fileListOver10m.isEmpty()) {
            msg = QStringList{tr("There is not enough storage available."),tr("You can free up space by managing your storage in UPDF Cloud.")};
            MessageWidget win(msg,btnList);
            win.resetIconImage(MessageWidget::IT_Delete);
            win.exec();
            return ;
        } else {
            if(LimitUseMGR->takeLoginLimit(LimitUseMgr::OP_UCloud) != LimitUseMgr::Later){
                return ;
            }
        }
    }
    for (QString path : filterFileList) {
        SerializeConfig::instance()->recordWaitUploadFile(path);
    }
    {
        QMutexLocker locker(&mWaitUploadMutex);
        mWaitForUploadFileList.append(filterFileList);
    }
    addWaitUploadListView(filterFileList);
    startUploadTask();
}

void CloudManager::addWaitUploadListView(const QList<QString> &fileList, bool toCloudList)
{
    for (QString path : fileList) {
        // placeholder
        QString filePath = QDir::fromNativeSeparators(path);
        FilePropInfo filePropInfo =  getFileProp(filePath, false);
        if (mMainWindow->isWindowRelease()) {
            return;
        }
        CloudFilePropInfo fileInfo;
        fileInfo.fileShowName = filePropInfo.name;
        fileInfo.filePath = filePropInfo.path;
        fileInfo.fileSize = filePropInfo.size;
        fileInfo.uploadTime = filePropInfo.visitTime;

        BaseFileDetailData detailData = HISTFileRecordsUtil::getInstance()->getHistoryRecord(filePath);
        fileInfo.totalPage = detailData.totalPage;
        fileInfo.logoPath = detailData.imgPath;
        fileInfo.havePwd = detailData.havePwd;
        if (toCloudList) {
            emit insertListView(fileInfo);
        } else {
            emit insertToUploadListView(fileInfo);
        }
    }
}

QList<QString> CloudManager::getWaitForUploadFileList()
{
    QMutexLocker locker(&mWaitUploadMutex);
    return mWaitForUploadFileList;
}

void CloudManager::removeWaitForUploadFileList(const QString &filePath)
{
    QMutexLocker locker(&mWaitUploadMutex);
    mWaitForUploadFileList.removeOne(filePath);
}

void CloudManager::startUploadTask()
{
    if (mWaitForUploadFileList.isEmpty()) {
        return;
    }
    for (;;) {
        if (mUploadTaskNum < Upload_MaxParallel_Task) {
            if (mWaitForUploadFileList.size() > 0) {
                QString filePath = "";
                {
                    QMutexLocker locker(&mWaitUploadMutex);
                    if (mWaitForUploadFileList.size() > 0) {
                        filePath = mWaitForUploadFileList.takeAt(0);
                    }
                }
                if (!filePath.isEmpty()) {
                    FilePropInfo filePropInfo = getFileProp(filePath);
                    if (mMainWindow->isWindowRelease()) {
                        break;
                    }
                    // 是否被删除
                    if (!SerializeConfig::instance()->loadWaitUploadFile().contains(filePath)) {
                        emit removeListItem(filePath);
                        continue;
                    }
                    mUploadTaskNum++;
                    RequestTask *task = NetRequestManager::instance()->preUpload(filePath, filePropInfo, CatelogueType::CloudSpace, 0);
                    connect(task, &RequestTask::finished, this, &CloudManager::onPreUploadFinished);

                }
            } else {
                break;
            }
        } else {
            break;
        }
    }
}

void CloudManager::finishUploadTask()
{
    if (--mUploadTaskNum < 0) {
        mUploadTaskNum = 0;
    }
    QTimer::singleShot(10, this, [=] {
        startUploadTask();
    });
}

bool CloudManager::cloudDataLoaded()
{
    return mDataLoaded;
}

void CloudManager::onPreUploadFinished(int state, int code,const QByteArray &data, QVariant opaque)
{
    FilePropInfo filePropInfo = opaque.value<FilePropInfo>();
    QString filePath = filePropInfo.path;
    qint64 fileId = filePropInfo.fileId;
    QString fileHash = filePropInfo.hash;
    auto removeItemFun = [&](const QString &filePath) {
        emit removeListItem(filePath);
        SerializeConfig::instance()->removeWaitUploadFile(filePath);
        finishUploadTask();
    };
    if (RequestTask::RequestState::Success == (RequestTask::RequestState)state) {
        QJsonObject jsonObj, jsonObject;
        if (!data.isEmpty() && CommonUtils::bufferToJson(data, &jsonObj)) {
            int rspCode = jsonObj["code"].toInt();
            if (rspCode != 200) {
                // error code
                QStringList msg;
                QStringList btnList = QStringList{tr("OK"),"",""};
                bool showLimit = false;
                if (rspCode == 5003) {
                    if (UserCenterManager::instance()->haveCloudPrivalege() || UserCenterManager::instance()->userContext().mUserInfo.companyUser) {
                        msg = QStringList{tr("There is not enough storage available."),tr("You can free up space by managing your storage in UPDF Cloud.")};
                    } else {
                        msg = QStringList{tr("There is not enough storage available. Upgrade to UPDF Pro to get more storage.")};
                        btnList = QStringList{tr("Upgrade"),tr("Later"),""};
                        showLimit = true;
                    }
                } else if (rspCode == 5001) {
                    msg = QStringList{tr("parent dir unexist")};
                } else if (rspCode == 5007) {
                    if (UserCenterManager::instance()->haveCloudPrivalege()) {
                        msg = QStringList{tr("The file you uploaded has exceeded the maximum file limit of %1 GB.").arg(2)};
                    } else {
                        if (UserCenterManager::instance()->userContext().mUserInfo.companyUser) {
                            msg = QStringList{tr("The file you uploaded has exceeded the maximum file limit of %1 GB.").arg(0.01)};
                        } else {
                            msg = QStringList{tr("The file is larger than %1 MB. Upgrade to UPDF Pro to upload files up to %2 GB.").arg(10).arg(2)};
                            btnList = QStringList{tr("Upgrade"),tr("Later"),""};
                            showLimit = true;
                        }
                    }
                } else if (rspCode == 5009) {
                    msg = QStringList{tr("Filename is illegal")};
                } else if (rspCode == 3404) {
                    msg = QStringList{tr("File unexist")};
                } else if (rspCode == 500) {
                    msg = QStringList{tr("Internal error")};
                } else {
                    msg = QStringList{tr("Error code:") + QString::number(jsonObj["code"].toInt()),jsonObj["msg"].toString()};
                }
                if(showLimit){
                    LimitUseMGR->takeLoginLimit(LimitUseMgr::OP_UCloud);
                }
                else{
                    MessageWidget win(msg,btnList);
                    win.resetIconImage(MessageWidget::IT_Delete);
                    if (win.exec() == MessageWidget::Accepted) {
                        UserCenterManager::instance()->buy();
                    }
                }
                removeItemFun(filePath);
                return;
            }
            if (jsonObj["data"].isObject()) {
                jsonObject = jsonObj["data"].toObject();
                bool fastUpload = jsonObject["fast_upload"].toBool();
                OssUploadParam uploadParam;
                if (jsonObject["file"].isObject()) {
                    QJsonObject fileObj = jsonObject["file"].toObject();
                    CloudFilePropInfo fileInfo = parseFileInfoFronJson(fileObj);
                    fileInfo.filePath = filePath;// 使用本地路径
                    uploadParam.fileInfo = fileInfo;
                } else {
                    removeItemFun(filePath);
                    return;
                }
                QString fileIdPath = QDir::fromNativeSeparators(CloudPredefine::workSpaceDir() + QDir::separator() + QString::number(uploadParam.fileInfo.fileId));
                QDir dir(fileIdPath);
                if (!dir.exists()) {
                    dir.mkdir(fileIdPath);
                }
                BaseFileDetailData detailData = HISTFileRecordsUtil::getInstance()->getHistoryRecord(uploadParam.fileInfo.filePath);
                uploadParam.fileInfo.totalPage = detailData.totalPage;
                QString logoPath = QDir::fromNativeSeparators(fileIdPath + QDir::separator() + "logo.png");
                if (detailData.imgPath.isEmpty() || !QFileInfo(detailData.imgPath).exists()) {
                    // 生成缩略图和页数
                    int pageCount = 0;
                    if (!mMainWindow->isWindowRelease()) {
                        QEventLoop eventLoop;
                        auto future = QtConcurrent::run(this, &CloudManager::loadDocInfo, uploadParam.fileInfo.filePath, QDir::toNativeSeparators(logoPath), &pageCount);
                        QFutureWatcher<void> *watcher = new QFutureWatcher<void>();
                        connect(watcher, &QFutureWatcher<void>::finished, &eventLoop, &QEventLoop::quit);
                        connect(mMainWindow, &MainWindow::windowRelease, &eventLoop, &QEventLoop::quit);
                        watcher->setFuture(future);
                        eventLoop.exec();
                        if (mMainWindow->isWindowRelease()) {
                            return;
                        }
                        // 是否被删除
                        if (!SerializeConfig::instance()->loadWaitUploadFile().contains(filePath)) {
                            removeItemFun(filePath);
                            return;
                        }
                    }
                    uploadParam.fileInfo.totalPage = pageCount;
                } else {
                    // 文件copy
                    QFile(detailData.imgPath).copy(logoPath);
                }
                uploadParam.fileInfo.logoPath = logoPath;
                if (!uploadParam.fileInfo.filePath.contains(fileIdPath)) { // 本地第一次上传的文件
                    // 更新参数
                    emit updateListItem(uploadParam.fileInfo);
                }
                if (fastUpload) {// 文件秒传了
                    SerializeConfig::instance()->updateTransDoneFileConfig(uploadParam.fileInfo, Upload_File_Prefix);
                    // 更新到ui
                    emit fileUploadSuccess(uploadParam.fileInfo);
                    appendFileInfoToCache(uploadParam.fileInfo);
                    emit mMainWindow->getTransportController()->transferStateSignal(uploadParam.fileInfo.fileId, TransferSuccess);
                    SerializeConfig::instance()->removeWaitUploadFile(uploadParam.fileInfo.filePath);
                    finishUploadTask();
                    return;
                }
                if (jsonObject["credentials"].isObject()) {
                    QJsonObject credentialsObj = jsonObject["credentials"].toObject();
                    uploadParam.credentials.accessKeyId = credentialsObj["accessKeyId"].toString();
                    uploadParam.credentials.accessKeySecret = credentialsObj["accessKeySecret"].toString();
                    uploadParam.credentials.securityToken = credentialsObj["securityToken"].toString();

                    qint64 serverCurTime = credentialsObj["now"].toVariant().toLongLong();
                    qint64 serverExpirateTime = credentialsObj["expiration"].toVariant().toLongLong();
                    // 转换成本地的过期时间
                    qint64 offsetSeconds = QDateTime::fromSecsSinceEpoch(serverCurTime).secsTo(QDateTime::fromSecsSinceEpoch(serverExpirateTime));
                    uploadParam.credentials.currentTime = QDateTime::currentDateTime();
                    uploadParam.credentials.expiration = QDateTime::currentDateTime().addSecs(offsetSeconds);
                    uploadParam.credentials.endpoint = credentialsObj["endpoint"].toString();
                    uploadParam.credentials.bucketName = credentialsObj["bucketName"].toString();
                    uploadParam.fileInfo.fileRemotePath = credentialsObj["file_path"].toString();
                    uploadParam.fileInfo.logoRemotePath = credentialsObj["logo_path"].toString();
                } else {
                    removeItemFun(filePath);
                    return;
                }
                bool ignoreCheck = false;
                if (!mWaitForDownloadToUploadMap.isEmpty() && mWaitForDownloadToUploadMap.contains(uploadParam.fileInfo.fileId)) {
                    if (mWaitForDownloadToUploadMap.value(uploadParam.fileInfo.fileId).second) {
                        ignoreCheck = true;
                        mWaitForDownloadToUploadMap.remove(uploadParam.fileInfo.fileId);
                    }
                }
                if (!ignoreCheck && !checkIfNeedUpload(uploadParam.fileInfo)) {
                    removeItemFun(filePath);
                    return;
                }
                emit mMainWindow->getTransportController()->transferStateSignal(uploadParam.fileInfo.fileId, WaitTransfer);
                // 上传前先停止之前的
                mMainWindow->getTransportController()->abortFileUpload(uploadParam.fileInfo.fileId);
                // update file_hash
                uploadParam.fileInfo.fileHash = fileHash;
                mMainWindow->getTransportController()->uploadFile(uploadParam);
                mMainWindow->getTransportController()->uploadThumbnail(uploadParam);
            }
        }
    } else {
        removeItemFun(filePath);
    }
}
void CloudManager::refreshOssToken(const QString &filePath, CatelogueType type, qint64 parentDirId, qint64 fileId)
{
    FilePropInfo filePropInfo = getFileProp(filePath);
    if (mMainWindow->isWindowRelease()) {
        return;
    }
    RequestTask *task = NetRequestManager::instance()->preUpload(filePath, filePropInfo, type, parentDirId, fileId);
    connect(task, &RequestTask::finished, this, &CloudManager::onRefreshOssTokenFinished);
}
void CloudManager::onRefreshOssTokenFinished(int state, int code,const QByteArray &data, QVariant opaque)
{
    FilePropInfo filePropInfo = opaque.value<FilePropInfo>();
    QString filePath = filePropInfo.path;
    qint64 fileId = filePropInfo.fileId;
    QString fileHash = filePropInfo.hash;
    if (RequestTask::RequestState::Success == (RequestTask::RequestState)state) {
        QJsonObject jsonObj, jsonObject;
        if (!data.isEmpty() && CommonUtils::bufferToJson(data, &jsonObj)) {
            int rspCode = jsonObj["code"].toInt();
            if (rspCode != 200) {
                // error code
                return;
            }
            if (jsonObj["data"].isObject()) {
                jsonObject = jsonObj["data"].toObject();
                if (jsonObject["credentials"].isObject()) {
                    QJsonObject credentialsObj = jsonObject["credentials"].toObject();
                    QString configPath = CloudPredefine::getUploadConfigFile(fileId);
                    if (QFileInfo(configPath).exists()) {
                        OssUploadParam uploadParam = SerializeConfig::instance()->readUploadParam(configPath);
                        uploadParam.credentials.accessKeyId = credentialsObj["accessKeyId"].toString();
                        uploadParam.credentials.accessKeySecret = credentialsObj["accessKeySecret"].toString();
                        uploadParam.credentials.securityToken = credentialsObj["securityToken"].toString();

                        qint64 serverCurTime = credentialsObj["now"].toVariant().toLongLong();
                        qint64 serverExpirateTime = credentialsObj["expiration"].toVariant().toLongLong();
                        // 转换成本地的过期时间
                        qint64 offsetSeconds = QDateTime::fromSecsSinceEpoch(serverCurTime).secsTo(QDateTime::fromSecsSinceEpoch(serverExpirateTime));
                        uploadParam.credentials.currentTime = QDateTime::currentDateTime();
                        uploadParam.credentials.expiration = QDateTime::currentDateTime().addSecs(offsetSeconds);
                        uploadParam.credentials.endpoint = credentialsObj["endpoint"].toString();
                        uploadParam.credentials.bucketName = credentialsObj["bucketName"].toString();
                        uploadParam.fileInfo.fileRemotePath = credentialsObj["file_path"].toString();
                        uploadParam.fileInfo.logoRemotePath = credentialsObj["logo_path"].toString();

                        // 上传前先停止之前的
                        mMainWindow->getTransportController()->abortFileUpload(uploadParam.fileInfo.fileId);
                        mMainWindow->getTransportController()->uploadFile(uploadParam);
                    }
                }
            }
        }
    }
}

void CloudManager::onOssUploadResult(bool success, const OssUploadParam &param)
{
    finishUploadTask();
    if (success) {
        updateFileUploadFlag(param);
    } else {
        // 文件上传出错
        emit mMainWindow->getTransportController()->transferStateSignal(param.fileInfo.fileId, TransferFail);
    }
    emit fileOssUploadResult(success, param);
}

void CloudManager::onOssDownloadResult(bool success, const OssDownloadParam &param)
{
    closeLoadingWidget();
    emit fileDownloadResult(success, param);
    if (!success) {
       if (mIgnoreErrorFileId != 0 && mIgnoreErrorFileId == param.fileInfo.fileId) {
           mIgnoreErrorFileId = 0;
       } else {
           MessageWidget win({QString(tr("Failed to download %1")).arg(param.fileInfo.fileShowName)},{tr("OK"),"",""});
           win.resetIconImage(MessageWidget::IT_Delete);
           win.exec();
       }
    }
    if (!mWaitForDownloadToUploadMap.isEmpty() && mWaitForDownloadToUploadMap.contains(param.fileInfo.fileId)) {
        CloudFilePropInfo fileInfo = mWaitForDownloadToUploadMap.value(param.fileInfo.fileId).first;
        if (mWaitForDownloadToUploadMap.value(param.fileInfo.fileId).second) {
            return;
        }
        QString fileIdPath = QDir::fromNativeSeparators(CloudPredefine::workSpaceDir() + QDir::separator() + QString::number(fileInfo.fileId));
        QString filePath = QDir::fromNativeSeparators(fileIdPath + QDir::separator() + fileInfo.fileShowName + PDF_FILE_SUFFIX);
        cancelUploadFileVersionCheck(fileInfo);
        reuploadFile(filePath, fileInfo.catelogue, fileInfo.parentDirId, fileInfo.fileId);
    }
}

void CloudManager::onUploadDoneFinished(int state, int code,const QByteArray &data, QVariant opaque)
{
    OssUploadParam uploadParam = opaque.value<OssUploadParam>();
    if (RequestTask::RequestState::Success == (RequestTask::RequestState)state) {
        QJsonObject jsonObj, jsonObject;
        if (!data.isEmpty() && CommonUtils::bufferToJson(data, &jsonObj)) {  
            int rspCode = jsonObj["code"].toInt();
            if (rspCode != 200) {
                // error code
                QStringList msg;
                QStringList btnList = QStringList{tr("OK"),"",""};
                if (rspCode == 5003) {
                    if (UserCenterManager::instance()->haveCloudPrivalege() || UserCenterManager::instance()->userContext().mUserInfo.companyUser) {
                        msg = QStringList{tr("There is not enough storage available."),tr("You can free up space by managing your storage in UPDF Cloud.")};
                    } else {
                        msg = QStringList{tr("There is not enough storage available. Upgrade to UPDF Pro to get more storage.")};
                        btnList = QStringList{tr("Upgrade"),tr("Later"),""};
                    }
                } else {
                    msg = QStringList{tr("Error code:") + QString::number(jsonObj["code"].toInt()),jsonObj["msg"].toString()};
                }
                MessageWidget win(msg,btnList);
                win.resetIconImage(MessageWidget::IT_Delete);
                win.exec();
                return;
            }
            if (jsonObj["data"].isObject()) {
                jsonObject = jsonObj["data"].toObject();
                if (jsonObject["file"].isObject()) {
                    QJsonObject fileObj = jsonObject["file"].toObject();

                    CloudFilePropInfo fileInfo = parseFileInfoFronJson(fileObj);
                    SerializeConfig::instance()->updateTransDoneFileConfig(fileInfo, Upload_File_Prefix);
                    // 更新到ui
                    emit fileUploadSuccess(fileInfo);
                    appendFileInfoToCache(fileInfo);
                }
            }
        }
    } else {
        emit mMainWindow->getTransportController()->transferStateSignal(uploadParam.fileInfo.fileId, TransferFail);
    }
}
void CloudManager::loadWaitUploadFile()
{
    mWaitForUploadFileList = SerializeConfig::instance()->loadWaitUploadFile();
    if (mWaitForUploadFileList.size() > 0) {
        startUploadTask();
    }
}
// 续传
void CloudManager::resumeTransportTask()
{
    QMap<QString, QString> taskMap = SerializeConfig::instance()->loadPauseTask();
    for (QString key : taskMap.keys()) {
        if (key.contains(Upload_File_Prefix)) {
            resumeUpload(taskMap.value(key));
        } else {
            resumeDownload(taskMap.value(key));
        }
    }
}

void CloudManager::resumeUpload(const QString &configPath, bool dealFail)
{
    OssUploadParam uploadParam = SerializeConfig::instance()->readUploadParam(configPath);
    if (uploadParam.fileInfo.transFail && !dealFail) {
        // 传输失败不主动重传
        return;
    }

    QString fileIdPath = QDir::fromNativeSeparators(CloudPredefine::workSpaceDir() + QDir::separator() + QString::number(uploadParam.fileInfo.fileId));
    if (!uploadParam.fileInfo.filePath.contains(fileIdPath)) { // 本地第一次上传的文件
        // 判断是否token过期
        if (uploadParam.credentials.currentTime < QDateTime::currentDateTime()
                || uploadParam.credentials.expiration > QDateTime::currentDateTime().addSecs(-60)) // 一分钟缓冲
        {
            QString filePath = uploadParam.fileInfo.filePath;
            refreshOssToken(filePath, CloudSpace, 0, uploadParam.fileInfo.fileId);
            return;
        }
        // 上传前先停止之前的
        mMainWindow->getTransportController()->abortFileUpload(uploadParam.fileInfo.fileId);
        mMainWindow->getTransportController()->uploadFile(uploadParam);
        return;
    }
    checkFileInfo(uploadParam.fileInfo.fileId, Resume_Upload);
}
void CloudManager::onCheckResumeUploadFileInfo(const CloudFilePropInfo &fileInfo)
{
    if (fileInfo.fileId <= 0) {
        return;
    }
    if (!checkIfNeedUpload(fileInfo)) {
        return;
    }

    QString configPath = CloudPredefine::getUploadConfigFile(fileInfo.fileId);
    if (QFileInfo(configPath).exists()) {
        OssUploadParam uploadParam = SerializeConfig::instance()->readUploadParam(configPath);
        // 判断是否token过期
        if (uploadParam.credentials.currentTime < QDateTime::currentDateTime()
                || uploadParam.credentials.expiration > QDateTime::currentDateTime().addSecs(-60)) // 一分钟缓冲
        {
            QString filePath = fileInfo.filePath;
            if (!QFileInfo(fileInfo.filePath).exists()) {// 云同步
                QString fileIdPath = QDir::fromNativeSeparators(CloudPredefine::workSpaceDir() + QDir::separator() + QString::number(fileInfo.fileId));
                filePath = QDir::fromNativeSeparators(fileIdPath + QDir::separator() + fileInfo.fileShowName + PDF_FILE_SUFFIX);
            }
            refreshOssToken(filePath, fileInfo.catelogue, fileInfo.parentDirId, fileInfo.fileId);
            return;
        }
        // 上传前先停止之前的
        mMainWindow->getTransportController()->abortFileUpload(uploadParam.fileInfo.fileId);
        mMainWindow->getTransportController()->uploadFile(uploadParam);
    }
}

void CloudManager::resumeDownload(const QString &configPath, bool dealFail)
{
    OssDownloadParam downloadParam = SerializeConfig::instance()->readDownloadParam(configPath);
    if (downloadParam.fileInfo.transFail && !dealFail) {
        // 传输失败不主动重传
        return;
    }
    checkFileInfo(downloadParam.fileInfo.fileId, Resume_Download);
}
void CloudManager::onCheckResumeDownloadFileInfo(const CloudFilePropInfo &fileInfo)
{
    if (!checkIfNeedDownload(fileInfo)) {
        return;
    }
    QString configPath = CloudPredefine::getDownloadConfigFile(fileInfo.fileId);
    OssDownloadParam downloadParam = SerializeConfig::instance()->readDownloadParam(configPath);
    // url是否过期
    if (downloadParam.fileInfo.urlCurrentTime < QDateTime::currentDateTime()
            || downloadParam.fileInfo.urlExpiration > QDateTime::currentDateTime().addSecs(-60)) // 一分钟缓冲
    {
        refreshDownloadUrl(fileInfo.fileId);
        return;
    }

    if (!mMainWindow->getTransportController()->checkFileDownloading(downloadParam.fileInfo.fileId)) {
        mMainWindow->getTransportController()->downloadFile(downloadParam);
    }
}

void CloudManager::getCloudList(CatelogueType type, int parentDirId)
{
    if (!UserCenterManager::instance()->checkLoginStatus() ||
            EnterpriseMGR->isLimitUseUCloud() || isPrivateDeploy()) {
        return;
    }

    RequestTask *task = NetRequestManager::instance()->getCloudList(type, parentDirId);
    task->setNetworkTip(false);
    connect(task, &RequestTask::finished, this, &CloudManager::onGetCloudListFinished);
}

void CloudManager::onGetCloudListFinished(int state, int code,const QByteArray &data, QVariant opaque)
{
    CatelogueType type = (CatelogueType)opaque.value<int>();
    if (type == CloudDocument) {
       mCloudDocumentFileInfos.clear();
    } else if (type == CloudSpace) {
       mCloudSpaceFileInfos.clear();
    } else if (type == RecycleBin) {
       mRecycleBinFileInfos.clear();
    }

    if (RequestTask::RequestState::Success == (RequestTask::RequestState)state) {
        QJsonObject jsonObj, jsonObject;
        if (!data.isEmpty() && CommonUtils::bufferToJson(data, &jsonObj)) {
            if (jsonObj["data"].isObject()) {
                jsonObject = jsonObj["data"].toObject();

                if (jsonObject["list"].isArray()) {
                    QJsonArray fileList = jsonObject["list"].toArray();
                    for (int i = 0; i < fileList.size(); ++i) {
                        QJsonObject fileObj = fileList[i].toObject();
                        CloudFilePropInfo fileInfo = parseFileInfoFronJson(fileObj);
                        if (type == CloudDocument) {
                           mCloudDocumentFileInfos.append(fileInfo);
                        } else if (type == CloudSpace) {
                           mCloudSpaceFileInfos.append(fileInfo);
                        } else if (type == RecycleBin) {
                           mRecycleBinFileInfos.append(fileInfo);
                        }
                    }
                }
            }
            if (type == CloudDocument) {
               emit getCloudDocumentFileList(mCloudDocumentFileInfos);
            } else if (type == CloudSpace) {
               emit getCloudSpaceFileList(mCloudSpaceFileInfos);
            } else if (type == RecycleBin) {
               emit getRecycleBinFileList(mRecycleBinFileInfos);
            }
            mDataLoaded = true;
        }
    }
}

CloudFilePropInfo CloudManager::findFileInfoFromCache(qint64 fileId)
{
    CloudFilePropInfo filePropInfo;
    bool find = false;
    for (LastVisitRecordInfo fileRecordInfo : mLastVisitRecordInfos) {
        if (fileRecordInfo.fileInfo.fileId == fileId) {
            filePropInfo = fileRecordInfo.fileInfo;
            find = true;
            break;
        }
    }
    if (!find) {
        for (CloudFilePropInfo fileInfo : mCloudSpaceFileInfos) {
            if (fileInfo.fileId == fileId) {
                filePropInfo = fileInfo;
                find = true;
                break;
            }
        }
    }
    if (!find) {
        for (CloudFilePropInfo fileInfo : mCloudDocumentFileInfos) {
            if (fileInfo.fileId == fileId) {
                filePropInfo = fileInfo;
                find = true;
                break;
            }
        }
    }
    return filePropInfo;
}

CloudFilePropInfo CloudManager::findFileInfoFromCache(const QString &fileHash)
{
    CloudFilePropInfo filePropInfo;
    bool find = false;
    for (LastVisitRecordInfo fileRecordInfo : mLastVisitRecordInfos) {
        if (fileRecordInfo.fileInfo.fileHash == fileHash) {
            filePropInfo = fileRecordInfo.fileInfo;
            find = true;
            break;
        }
    }
    if (!find) {
        for (CloudFilePropInfo fileInfo : mCloudSpaceFileInfos) {
            if (fileInfo.fileHash == fileHash) {
                filePropInfo = fileInfo;
                find = true;
                break;
            }
        }
    }
    if (!find) {
        for (CloudFilePropInfo fileInfo : mCloudDocumentFileInfos) {
            if (fileInfo.fileHash == fileHash) {
                filePropInfo = fileInfo;
                find = true;
                break;
            }
        }
    }
    return filePropInfo;
}

void CloudManager::appendFileInfoToCache(const CloudFilePropInfo &fileInfo)
{
    bool find = false;
    for (int i = 0; i < mCloudSpaceFileInfos.size(); ++i) {
        CloudFilePropInfo cacheFileInfo = mCloudSpaceFileInfos.at(i);
        if (cacheFileInfo.fileId == fileInfo.fileId) {
            mCloudSpaceFileInfos.replace(i, fileInfo);
            find = true;
            break;
        }
    }
    if (!find) {
        mCloudSpaceFileInfos.append(fileInfo);
    }
}

void CloudManager::clearFileInfoCache()
{
    mLastVisitRecordInfos.clear();
    mCloudSpaceFileInfos.clear();
    mRecycleBinFileInfos.clear();
    mCloudDocumentFileInfos.clear();
}

void CloudManager::updateFileInfoForCache(const CloudFilePropInfo &fileInfo)
{
    bool find = false;
    for (int i = 0; i < mLastVisitRecordInfos.size(); ++i) {
        LastVisitRecordInfo fileRecordInfo = mLastVisitRecordInfos.at(i);
        if (fileRecordInfo.fileInfo.fileId == fileInfo.fileId) {
            fileRecordInfo.fileInfo = fileInfo;
            mLastVisitRecordInfos.replace(i, fileRecordInfo);
            find = true;
            break;
        }
    }
    if (!find) {
        for (int i = 0; i < mCloudSpaceFileInfos.size(); ++i) {
            CloudFilePropInfo cacheFileInfo = mCloudSpaceFileInfos.at(i);
            if (cacheFileInfo.fileId == fileInfo.fileId) {
                mCloudSpaceFileInfos.replace(i, fileInfo);
                find = true;
                break;
            }
        }
        if (!find) {
            for (int i = 0; i < mCloudDocumentFileInfos.size(); ++i) {
                CloudFilePropInfo cacheFileInfo = mCloudDocumentFileInfos.at(i);
                if (cacheFileInfo.fileId == fileInfo.fileId) {
                    mCloudDocumentFileInfos.replace(i, fileInfo);
                    find = true;
                    break;
                }
            }
        }
    }
}

bool CloudManager::checkIfNeedUpload(const CloudFilePropInfo &fileInfo)
{
    // 比较文件hash和version
    CloudFilePropInfo downloadPropInfo = getLocalCloudFileInfo(fileInfo);
    if (fileInfo.fileId != 0 && downloadPropInfo.fileId == fileInfo.fileId) {
        if (fileInfo.fileVersion != 0 && fileInfo.fileVersion > downloadPropInfo.fileVersion) {
            bool continueUpload;
            emit uploadWidthNewCloudVersion(fileInfo, &continueUpload);
            return continueUpload;
        }
        FilePropInfo filePropInfo = getFileProp(fileInfo.filePath);
        if (fileInfo.fileHash != "" && filePropInfo.hash == fileInfo.fileHash) {
            return false; // 文件相同
        }
    }
    return true;
}

bool CloudManager::checkIfNeedDownload(const CloudFilePropInfo &fileInfo, bool userAccess)
{
    QString fileIdPath = QDir::fromNativeSeparators(CloudPredefine::workSpaceDir() + QDir::separator() + QString::number(fileInfo.fileId));
    QString filePath = QDir::fromNativeSeparators(fileIdPath + QDir::separator() + fileInfo.fileShowName + PDF_FILE_SUFFIX);
    if (!QFileInfo(filePath).exists()) {
        return true;
    }
    // 比较文件hash和version
    CloudFilePropInfo downloadPropInfo = getLocalCloudFileInfo(fileInfo);
    if (fileInfo.fileId != 0 && downloadPropInfo.fileId == fileInfo.fileId) {
        if (fileInfo.fileVersion != 0 && fileInfo.fileVersion <= downloadPropInfo.fileVersion) {
            return false; // 云文件较旧
        }
        FilePropInfo filePropInfo = getCloudFileProp(fileInfo);
        if (fileInfo.fileHash != "" && filePropInfo.hash == fileInfo.fileHash) {
            return false; // 文件相同
        }
    }
    if (userAccess) {
        bool continueDownload;
        emit downloadWidthNewCloudVersion(fileInfo, &continueDownload);
        return continueDownload;
    }
    return true;
}

FilePropInfo CloudManager::getCloudFileProp(const CloudFilePropInfo &fileInfo)
{
    QString fileIdPath = QDir::fromNativeSeparators(CloudPredefine::workSpaceDir() + QDir::separator() + QString::number(fileInfo.fileId));
    QString filePath = QDir::fromNativeSeparators(fileIdPath + QDir::separator() + fileInfo.fileShowName + PDF_FILE_SUFFIX);
    return getFileProp(filePath);
}

CloudFilePropInfo CloudManager::getLocalCloudFileInfo(const CloudFilePropInfo &fileInfo)
{
    CloudFilePropInfo downloadPropInfo = SerializeConfig::instance()->checkTransDoneFileInfo(QString::number(fileInfo.fileId), Download_File_Prefix);
    CloudFilePropInfo uploadPropInfo = SerializeConfig::instance()->checkTransDoneFileInfo(QString::number(fileInfo.fileId), Upload_File_Prefix);
    // 取大的版本
    if (downloadPropInfo.fileVersion > uploadPropInfo.fileVersion) {
        return downloadPropInfo;
    } else {
        return uploadPropInfo;
    }
}

void CloudManager::showLoadingWidget(qint64 fileId)
{
    if (mLoadingWidget) {
        mLoadingWidget->closeWin();
    }
    mLoadingWidget = new LoadingWidgetWithCancel(fileId, (QWidget*)GuiUtil::getTopLevelWind());
    connect(mLoadingWidget, &LoadingWidgetWithCancel::winClosed, this, [=] {
        mLoadingWidget->deleteLater();
        mLoadingWidget = nullptr;
    });
    connect(mLoadingWidget, &LoadingWidgetWithCancel::cancelClicked, this, [=](qint64 fileId) {
        mIgnoreErrorFileId = fileId;
        emit mMainWindow->getTransportController()->transferStateSignal(fileId, TransferSuccess);
        mMainWindow->getTransportController()->abortFileDownload(fileId);
    });
    connect(mMainWindow->getTransportController(), &TransportController::downloadProgress, mLoadingWidget, [=](const OssDownloadParam &param) {
        double progress = param.downloadSize * 1.0 / param.fileInfo.fileSize * 100;
        progress = qMax(1.0, progress);
        if (mLoadingWidget) {
            mLoadingWidget->setLoadingProgress(progress);
        }
    });
    mLoadingWidget->setLoadingProgress(1);
    mLoadingWidget->show();
}

void CloudManager::closeLoadingWidget()
{
    if (mLoadingWidget) {
        mLoadingWidget->closeWin();
    }
}

void CloudManager::cacheUploadWaitForDownload(const CloudFilePropInfo &fileInfo)
{
    mWaitForDownloadToUploadMap.insert(fileInfo.fileId, qMakePair(fileInfo, false));
}

void CloudManager::cancelUploadFileVersionCheck(const CloudFilePropInfo &fileInfo)
{
    // 为了不再检测版本
    mWaitForDownloadToUploadMap.insert(fileInfo.fileId, qMakePair(fileInfo, true));
}

void CloudManager::preDownloadFile(qint64 fileId, const QString &fileSaveDir)
{
    if (fileId <= 0) {
        return;
    }
    if (!UserCenterManager::instance()->checkLoginStatus()) {
        return;
    }
    if (fileSaveDir.isEmpty()) {
        showLoadingWidget(fileId);
    }
    emit mMainWindow->getTransportController()->transferStateSignal(fileId, WaitTransfer);

    OssDownloadParam downloadParam;
    downloadParam.fileInfo.fileId = fileId;
    downloadParam.saveDir = fileSaveDir;
    RequestTask *task = NetRequestManager::instance()->getFileUrl(downloadParam);
    connect(task, &RequestTask::finished, this, &CloudManager::onPreDownloadFileFinished);

}

void CloudManager::onPreDownloadFileFinished(int state, int code,const QByteArray &data, QVariant opaque)
{
    OssDownloadParam downloadParam = opaque.value<OssDownloadParam>();
    if (RequestTask::RequestState::Success == (RequestTask::RequestState)state) {
        QJsonObject jsonObj, jsonObject;
        if (!data.isEmpty() && CommonUtils::bufferToJson(data, &jsonObj)) {
            int rspCode = jsonObj["code"].toInt();
            if (rspCode != 200) {
                // error code
                closeLoadingWidget();
                emit mMainWindow->getTransportController()->transferStateSignal(downloadParam.fileInfo.fileId, TransferSuccess);
                emit cloudFileError(downloadParam.fileInfo.fileId, rspCode);
                return;
            }
            if (jsonObj["data"].isObject()) {
                jsonObject = jsonObj["data"].toObject();
                QString url = jsonObject["url"].toString();
                qint64 serverCurTime = jsonObject["now"].toVariant().toLongLong();
                qint64 serverExpirateTime = jsonObject["expiration"].toVariant().toLongLong();

                CloudFilePropInfo fileInfo = findFileInfoFromCache(downloadParam.fileInfo.fileId);
                fileInfo.fileUrl = url;
                // 转换成本地的过期时间
                qint64 offsetSeconds = QDateTime::fromSecsSinceEpoch(serverCurTime).secsTo(QDateTime::fromSecsSinceEpoch(serverExpirateTime));
                fileInfo.urlCurrentTime = QDateTime::currentDateTime();
                fileInfo.urlExpiration = QDateTime::currentDateTime().addSecs(offsetSeconds);
                downloadParam.fileInfo = fileInfo;

                if (fileInfo.fileUrl == "" || fileInfo.fileId <= 0 || (downloadParam.saveDir.isEmpty() && !checkIfNeedDownload(fileInfo))) {
                    closeLoadingWidget();
                    emit mMainWindow->getTransportController()->transferStateSignal(downloadParam.fileInfo.fileId, TransferSuccess);
                    return;
                }
                if (!downloadParam.saveDir.isEmpty() && !checkIfNeedDownload(fileInfo)) {
                    // 另存已在缓存的文件
                    QString fileIdPath = QDir::fromNativeSeparators(CloudPredefine::workSpaceDir() + QDir::separator() + QString::number(downloadParam.fileInfo.fileId));
                    QString filePath = QDir::fromNativeSeparators(fileIdPath + QDir::separator() + downloadParam.fileInfo.fileShowName + PDF_FILE_SUFFIX);
                    QString saveToFilePath = QDir::fromNativeSeparators(downloadParam.saveDir + QDir::separator() + downloadParam.fileInfo.fileShowName + PDF_FILE_SUFFIX);
                    if (QFile::exists(saveToFilePath)) {
                        QFile::remove(saveToFilePath);
                    }
                    QFile(filePath).copy(saveToFilePath);
                    GuiUtil::openFilePath(saveToFilePath);
                    // 记录
                    CloudFilePropInfo propInfo = downloadParam.fileInfo;
                    propInfo.filePath = saveToFilePath;
                    propInfo.createTime = QDateTime::currentDateTime();
                    SerializeConfig::instance()->addSaveAsRecord(propInfo);

                    emit mMainWindow->getTransportController()->transferStateSignal(downloadParam.fileInfo.fileId, TransferSuccess);
                    return;
                }
                if (!mMainWindow->getTransportController()->checkFileDownloading(downloadParam.fileInfo.fileId)) {
                    mMainWindow->getTransportController()->downloadFile(downloadParam);
                }
            }
        }
    } else {
        closeLoadingWidget();
        emit mMainWindow->getTransportController()->transferStateSignal(downloadParam.fileInfo.fileId, TransferSuccess);
    }
}

void CloudManager::refreshDownloadUrl(qint64 fileId)
{
    OssDownloadParam downloadParam;
    downloadParam.fileInfo.fileId = fileId;
    RequestTask *task = NetRequestManager::instance()->getFileUrl(downloadParam);
    connect(task, &RequestTask::finished, this, &CloudManager::onRefreshDownloadUrlFinished);
}
void CloudManager::onRefreshDownloadUrlFinished(int state, int code,const QByteArray &data, QVariant opaque)
{
    OssDownloadParam downloadParam = opaque.value<OssDownloadParam>();
    if (RequestTask::RequestState::Success == (RequestTask::RequestState)state) {
        QJsonObject jsonObj, jsonObject;
        if (!data.isEmpty() && CommonUtils::bufferToJson(data, &jsonObj)) {
            int rspCode = jsonObj["code"].toInt();
            if (rspCode != 200) {
                // error code
                return;
            }
            if (jsonObj["data"].isObject()) {
                jsonObject = jsonObj["data"].toObject();
                QString url = jsonObject["url"].toString();
                qint64 serverCurTime = jsonObject["now"].toVariant().toLongLong();
                qint64 serverExpirateTime = jsonObject["expiration"].toVariant().toLongLong();
                // 转换成本地的过期时间
                qint64 offsetSeconds = QDateTime::fromSecsSinceEpoch(serverCurTime).secsTo(QDateTime::fromSecsSinceEpoch(serverExpirateTime));

                QString configPath = CloudPredefine::getDownloadConfigFile(downloadParam.fileInfo.fileId);
                OssDownloadParam downloadParam = SerializeConfig::instance()->readDownloadParam(configPath);
                downloadParam.fileInfo.fileUrl = url;
                downloadParam.fileInfo.urlCurrentTime = QDateTime::currentDateTime();
                downloadParam.fileInfo.urlExpiration = QDateTime::currentDateTime().addSecs(offsetSeconds);
                if (!mMainWindow->getTransportController()->checkFileDownloading(downloadParam.fileInfo.fileId)) {
                    mMainWindow->getTransportController()->downloadFile(downloadParam);
                }
            }
        }
    }
}

void CloudManager::renameFile(qint64 fileId, const QString &newName)
{
    if (!UserCenterManager::instance()->checkLoginStatus()) {
        return;
    }
    RequestTask *task = NetRequestManager::instance()->renameFile(fileId, newName);
    connect(task, &RequestTask::finished, this, &CloudManager::onRenameFileFinished);

}

void CloudManager::onRenameFileFinished(int state, int code,const QByteArray &data, QVariant opaque)
{
    RenameStruct renameStruct = opaque.value<RenameStruct>();
    if (RequestTask::RequestState::Success == (RequestTask::RequestState)state) {
        QJsonObject jsonObj, jsonObject;
        if (!data.isEmpty() && CommonUtils::bufferToJson(data, &jsonObj)) {
            int rspCode = jsonObj["code"].toInt();
            if (rspCode != 200) {
                // error code
                QStringList msg;
                if (rspCode == 5005 || rspCode == 5004) {//5004	文件已经被回收
                    msg = QStringList{tr("File unexist")};
                    emit cloudFileError(renameStruct.fileId, rspCode);
                    return; // 不需要提示
                } else if (rspCode == 5006) {
                    msg = QStringList{tr("File name already exists")};
                } else if (rspCode == 5009) {
                    msg = QStringList{tr("Filename is illegal")};
                } else if (rspCode == 500) {
                    msg = QStringList{tr("Internal error")};
                } else {
                    msg = QStringList{tr("Error code:") + QString::number(jsonObj["code"].toInt()),jsonObj["msg"].toString()};
                }
                MessageWidget win(msg,{tr("OK"),"",""});
                win.resetIconImage(MessageWidget::IT_Delete);
                win.exec();
                return;
            }
            if (jsonObj["data"].isObject()) {
                jsonObject = jsonObj["data"].toObject();
            }
        }
        emit renameFileFinished(renameStruct);
    }
}

void CloudManager::moveToRecycle(const QList<qint64> &fileIds, const QList<qint64> &dirIds)
{
    if (!UserCenterManager::instance()->checkLoginStatus()) {
        return;
    }
    RequestTask *task = NetRequestManager::instance()->moveToRecycle(fileIds, dirIds);
    connect(task, &RequestTask::finished, this, &CloudManager::onMoveToRecycleFinished);

}
void CloudManager::onMoveToRecycleFinished(int state, int code,const QByteArray &data, QVariant opaque)
{
    DeleteOprStruct deleteOpr = opaque.value<DeleteOprStruct>();
    if (RequestTask::RequestState::Success == (RequestTask::RequestState)state) {
        QJsonObject jsonObj, jsonObject;
        if (!data.isEmpty() && CommonUtils::bufferToJson(data, &jsonObj)) {
            int rspCode = jsonObj["code"].toInt();
            if (rspCode != 200) {
                // error code
                QStringList msg;
                if (rspCode == 5005 || rspCode == 5004) {
                    msg = QStringList{tr("File unexist")};
                    emit cloudFileError(deleteOpr.fileIds.at(0), rspCode);
                    return; // 不需要提示
                } else if (rspCode == 500) {
                    msg = QStringList{tr("Internal error")};
                } else {
                    msg = QStringList{tr("Error code:") + QString::number(jsonObj["code"].toInt()),jsonObj["msg"].toString()};
                }
                MessageWidget win(msg,{tr("OK"),"",""});
                win.resetIconImage(MessageWidget::IT_Delete);
                win.exec();
                return;
            }
            if (jsonObj["data"].isObject()) {
                jsonObject = jsonObj["data"].toObject();
            }
            emit moveToRecycleFinished(deleteOpr);
        }
    }
}
void CloudManager::returnFromRecycle(const QList<qint64> &fileIds, const QList<qint64> &dirIds)
{
    if (!UserCenterManager::instance()->checkLoginStatus()) {
        return;
    }
    RequestTask *task = NetRequestManager::instance()->returnFromRecycle(fileIds, dirIds);
    connect(task, &RequestTask::finished, this, &CloudManager::onReturnFromRecycleFinished);

}
void CloudManager::onReturnFromRecycleFinished(int state, int code,const QByteArray &data, QVariant opaque)
{
    DeleteOprStruct deleteOpr = opaque.value<DeleteOprStruct>();
    if (RequestTask::RequestState::Success == (RequestTask::RequestState)state) {
        QJsonObject jsonObj, jsonObject;
        if (!data.isEmpty() && CommonUtils::bufferToJson(data, &jsonObj)) {
            int rspCode = jsonObj["code"].toInt();
            if (rspCode != 200) {
                // error code
                QStringList msg;
                if (rspCode == 5005 || rspCode == 5004) {
                    msg = QStringList{tr("File unexist")};
                    emit cloudFileError(deleteOpr.fileIds.at(0), rspCode);
                    return; // 不需要提示
                } else if (rspCode == 500) {
                    msg = QStringList{tr("Internal error")};
                } else {
                    msg = QStringList{tr("Error code:") + QString::number(jsonObj["code"].toInt()),jsonObj["msg"].toString()};
                }
                MessageWidget win(msg,{tr("OK"),"",""});
                win.resetIconImage(MessageWidget::IT_Delete);
                win.exec();
                return;
            }
            if (jsonObj["data"].isObject()) {
                jsonObject = jsonObj["data"].toObject();
            }
            emit returnFromRecycleFinished(deleteOpr);
        }
    }
}
void CloudManager::deleteFromRecycle(const QList<qint64> &fileIds, const QList<qint64> &dirIds, bool clearAll)
{
    if (!UserCenterManager::instance()->checkLoginStatus()) {
        return;
    }
    RequestTask *task = NetRequestManager::instance()->deleteFromRecycle(fileIds, dirIds, clearAll);
    connect(task, &RequestTask::finished, this, &CloudManager::onDeleteFromRecycleFinished);

}

void CloudManager::onDeleteFromRecycleFinished(int state, int code,const QByteArray &data, QVariant opaque)
{
    DeleteOprStruct deleteOpr = opaque.value<DeleteOprStruct>();
    if (RequestTask::RequestState::Success == (RequestTask::RequestState)state) {
        QJsonObject jsonObj, jsonObject;
        if (!data.isEmpty() && CommonUtils::bufferToJson(data, &jsonObj)) {
            int rspCode = jsonObj["code"].toInt();
            if (rspCode != 200) {
                // error code
                QStringList msg;
                if (rspCode == 5005 || rspCode == 5004) {
                    msg = QStringList{tr("File unexist")};
                    emit cloudFileError(deleteOpr.fileIds.at(0), rspCode);
                    return; // 不需要提示
                } else if (rspCode == 500) {
                    msg = QStringList{tr("Internal error")};
                } else {
                    msg = QStringList{tr("Error code:") + QString::number(jsonObj["code"].toInt()),jsonObj["msg"].toString()};
                }
                MessageWidget win(msg,{tr("OK"),"",""});
                win.resetIconImage(MessageWidget::IT_Delete);
                win.exec();
                return;
            }
            if (jsonObj["data"].isObject()) {
                jsonObject = jsonObj["data"].toObject();
            }
            emit deleteFromRecycleFinished(deleteOpr);
            for (qint64 fileId : deleteOpr.fileIds) {
                emit cloudFileError(fileId, 0);
            }
        }
    }
}

void CloudManager::checkFileInfo(qint64 fileId, GetInfoNextStep step)
{
    if (!UserCenterManager::instance()->checkLoginStatus()) {
        return;
    }
    RequestTask *task = NetRequestManager::instance()->getFileInfo(fileId, step);
    connect(task, &RequestTask::finished, this, &CloudManager::onCheckFileInfoFinished);

}

void CloudManager::onCheckFileInfoFinished(int state, int code,const QByteArray &data, QVariant opaque)
{
    QString opaqueStr = opaque.value<QString>();
    qint64 fileId = 0;
    GetInfoNextStep step;
    if (opaqueStr.contains("&")) {
        fileId = QString(opaqueStr.split("&").at(0)).toLongLong();
        step = (GetInfoNextStep)QString(opaqueStr.split("&").at(1)).toInt();
    }
    if (RequestTask::RequestState::Success == (RequestTask::RequestState)state) {
        QJsonObject jsonObj, jsonObject;
        if (!data.isEmpty() && CommonUtils::bufferToJson(data, &jsonObj)) {
            int rspCode = jsonObj["code"].toInt();
            if (rspCode != 200) {
                // error code
                QStringList msg;
                if (rspCode == 5005 || rspCode == 5004) {
                    msg = QStringList{tr("File unexist")};
                    emit cloudFileError(fileId, rspCode);
                    return; // 不需要提示
                } else if (rspCode == 500) {
                    msg = QStringList{tr("Internal error")};
                } else {
                    msg = QStringList{tr("Error code:") + QString::number(jsonObj["code"].toInt()),jsonObj["msg"].toString()};
                }
                MessageWidget win(msg,{tr("OK"),"",""});
                win.resetIconImage(MessageWidget::IT_Delete);
                win.exec();
                return;
            }
            if (jsonObj["data"].isObject()) {
                jsonObject = jsonObj["data"].toObject();

                CloudFilePropInfo fileInfo;
                if (jsonObject["file"].isObject()) {
                    QJsonObject fileObj = jsonObject["file"].toObject();
                    fileInfo = parseFileInfoFronJson(fileObj);
                    updateFileInfoForCache(fileInfo);

                    if (step == Resume_Upload) {
                        emit checkResumeUploadFileInfo(fileInfo);
                    } else if (step == Resume_Download) {
                        emit checkResumeDownloadFileInfo(fileInfo);
                    } else if (step == Upload) {
                        emit checkUploadFileInfo(fileInfo);
                    } else if (step == Download) {
                        emit checkDownloadFileInfo(fileInfo);// 统一到最近列表处理
                    } else if (step == CheckVersion) {
                        emit checkVersionFileInfo(fileInfo);// 统一到最近列表处理
                    }
                }
            }
        }
    } else {
        emit mMainWindow->getTransportController()->transferStateSignal(fileId, TransferFail);
    }
}

CloudFilePropInfo CloudManager::parseFileInfoFronJson(const QJsonObject &fileObj)
{
    CloudFilePropInfo fileInfo;
    int type = fileObj["type"].toInt(); // 类型(1:目录，2:文件)
    fileInfo.fileId = fileObj["id"].toVariant().toLongLong();
    fileInfo.parentDirId = fileObj["pid"].toVariant().toLongLong();
    fileInfo.fileVersion = fileObj["file_version"].toVariant().toLongLong();
    fileInfo.fileSize = fileObj["size"].toInt();
    int dirType = fileObj["dir_type"].toInt();
    if (dirType <= 0 || dirType > CloudSpace) {
       dirType = CloudSpace;
    }
    fileInfo.catelogue = (CatelogueType)dirType;
    fileInfo.totalPage = fileObj["total_page"].toInt();
    fileInfo.isStarred = fileObj["stared"].toInt() == 1;
    fileInfo.isShared = fileObj["share"].toInt() == 1;
    fileInfo.inRecycle = fileObj["recycle"].toInt() == 1;
    fileInfo.havePwd = fileObj["has_pwd"].toInt() == 1;
    fileInfo.filePath = fileObj["device_location"].toString();
    fileInfo.fileShowName = fileObj["show_name"].toString();
    if (fileInfo.fileShowName.contains(PDF_FILE_SUFFIX)) {
        fileInfo.fileShowName.replace(PDF_FILE_SUFFIX, "");
    }
    if (fileInfo.fileShowName.contains(PDF_FILE_UPSUFFIX)) {
        fileInfo.fileShowName.replace(PDF_FILE_UPSUFFIX, "");
    }
    if (fileInfo.fileShowName.isEmpty()) {
        fileInfo.fileShowName = QFileInfo(fileInfo.filePath).completeBaseName();
    }

    fileInfo.createTime = QDateTime::fromSecsSinceEpoch(fileObj["created_at"].toDouble());
    fileInfo.updateTime = QDateTime::fromSecsSinceEpoch(fileObj["updated_at"].toDouble());
    fileInfo.uploadTime = QDateTime::fromSecsSinceEpoch(fileObj["uploaded_at"].toDouble());
    fileInfo.recycleExpireTime = QDateTime::fromSecsSinceEpoch(fileObj["recycle_expired_at"].toDouble());
    fileInfo.fileRemotePath = fileObj["node_path"].toString();
    fileInfo.lastVisit = fileObj["last_visit"].toString();
    fileInfo.lastVisitTime = QDateTime::fromSecsSinceEpoch(fileObj["latest_visited_at"].toDouble());
    fileInfo.logoUrl = fileObj["logo_url"].toString();
    fileInfo.fileHash = fileObj["file_hash"].toString();
    fileInfo.fileCrc = fileObj["file_crc64"].toString().toULongLong();

    LastVisitParam visitParam = CloudPredefine::parseLastVisit(fileInfo.lastVisit);
    fileInfo.currentPageIndex = visitParam.pageIndex;
    fileInfo.scale = visitParam.scale;
    return fileInfo;
}

void CloudManager::updateFileUploadFlag(const OssUploadParam &param)
{
    if (!UserCenterManager::instance()->checkLoginStatus()) {
        return;
    }
    RequestTask *task = NetRequestManager::instance()->uploadDone(param);
    connect(task, &RequestTask::finished, this, &CloudManager::onUploadDoneFinished);

}

void CloudManager::visitReport(const LastVisitParam &lastVisitParam)
{
    if (!UserCenterManager::instance()->checkLoginStatus() || isPrivateDeploy()) {
        return;
    }
    RequestTask *task = NetRequestManager::instance()->visitReport(lastVisitParam);
    task->setNetworkTip(false);
    connect(task, &RequestTask::finished, this, &CloudManager::onVisitReportFinished);
}

void CloudManager::onVisitReportFinished(int state, int code,const QByteArray &data, QVariant opaque)
{
    if (RequestTask::RequestState::Success == (RequestTask::RequestState)state) {
        QJsonObject jsonObj, jsonObject;
        if (!data.isEmpty() && CommonUtils::bufferToJson(data, &jsonObj)) {
            if (jsonObj["data"].isObject()) {
                jsonObject = jsonObj["data"].toObject();
            }
        }
    }
}

void CloudManager::removeVisitRecord(const QList<qint64> &recordIds)
{
    if (!UserCenterManager::instance()->checkLoginStatus()) {
        return;
    }
    RequestTask *task = NetRequestManager::instance()->removeVisitRecord(recordIds);
    connect(task, &RequestTask::finished, this, &CloudManager::onRemoveVisitRecordFinished);

}

void CloudManager::onRemoveVisitRecordFinished(int state, int code,const QByteArray &data, QVariant opaque)
{
    DeleteOprStruct deleteOpr = opaque.value<DeleteOprStruct>();
    if (RequestTask::RequestState::Success == (RequestTask::RequestState)state) {
        QJsonObject jsonObj, jsonObject;
        if (!data.isEmpty() && CommonUtils::bufferToJson(data, &jsonObj)) {
            if (jsonObj["data"].isObject()) {
                jsonObject = jsonObj["data"].toObject();
            }
            emit removeVisitRecordFinished(deleteOpr);
        }
    }
}

void CloudManager::getVisitRecord()
{
    if (!UserCenterManager::instance()->checkLoginStatus() ||
            EnterpriseMGR->isLimitUseUCloud() || isPrivateDeploy()) {
        return;
    }
    RequestTask *task = NetRequestManager::instance()->getVisitRecord();
    connect(task, &RequestTask::finished, this, &CloudManager::onGetVisitRecordFinished);
}

void CloudManager::onGetVisitRecordFinished(int state, int code,const QByteArray &data, QVariant opaque)
{
    if (RequestTask::RequestState::Success == (RequestTask::RequestState)state) {
        QJsonObject jsonObj, jsonObject;
        if (!data.isEmpty() && CommonUtils::bufferToJson(data, &jsonObj)) {
            if (jsonObj["data"].isObject()) {
                jsonObject = jsonObj["data"].toObject();
                if (jsonObject["list"].isArray()) {
                    QJsonArray fileList = jsonObject["list"].toArray();
                    LastVisitRecordInfos recordInfos;
                    for (int i = 0; i < fileList.size(); ++i) {
                        QJsonObject recordObj = fileList[i].toObject();
                        LastVisitRecordInfo recordInfo;
                        recordInfo.visitRecordId = recordObj["id"].toVariant().toLongLong();
                        QJsonObject fileObj = recordObj["file"].toObject();
                        CloudFilePropInfo fileInfo = parseFileInfoFronJson(fileObj);
                        recordInfo.fileInfo = fileInfo;
                        recordInfos.append(recordInfo);
                    }
                    mLastVisitRecordInfos = recordInfos;
                    emit getVisitRecordFinished(recordInfos);
                }
            }
        }
    }
}

void CloudManager::searchFile(const QString &keyword)
{
    // cloud
    RequestTask *task = NetRequestManager::instance()->searchFile(keyword);
    connect(task, &RequestTask::finished, this, &CloudManager::onSearchFileFinished);

    // local
    QList<BaseFileDetailData> baseDetailList = HISTFileRecordsUtil::getInstance()->matchHistoryRecord(keyword);
    CloudFilePropInfos fileInfos;
    for (BaseFileDetailData detailData : baseDetailList) {
        CloudFilePropInfo fileInfo;
        fileInfo.filePath = detailData.filePath;
        fileInfo.fileShowName = detailData.fileName;
        fileInfo.fileSize = detailData.fileSize;
        fileInfo.totalPage = detailData.totalPage;
        fileInfos.append(fileInfo);
    }
    emit localMatchFiles(fileInfos);
}

void CloudManager::onSearchFileFinished(int state, int code,const QByteArray &data, QVariant opaque)
{
    CloudFilePropInfos fileInfos;
    if (RequestTask::RequestState::Success == (RequestTask::RequestState)state) {
        QJsonObject jsonObj, jsonObject;
        if (!data.isEmpty() && CommonUtils::bufferToJson(data, &jsonObj)) {
            if (jsonObj["data"].isObject()) {
                jsonObject = jsonObj["data"].toObject();
                if (jsonObject["list"].isArray()) {
                    QJsonArray fileList = jsonObject["list"].toArray();
                    for (int i = 0; i < fileList.size(); ++i) {
                        QJsonObject fileObj = fileList[i].toObject();
                        fileInfos.append(parseFileInfoFronJson(fileObj));
                    }
                }
            }
        }
    }
    emit cloudMatchFiles(fileInfos);
}

void CloudManager::getDiskInfo()
{
    if (!UserCenterManager::instance()->checkLoginStatus() || isPrivateDeploy()) {
        return;
    }
    RequestTask *task = NetRequestManager::instance()->getCloudDiskInfo();
    task->setNetworkTip(false);
    connect(task, &RequestTask::finished, this, &CloudManager::onGetDiskInfoFinished);
}

void CloudManager::reuploadFile(const QString &filePath, CatelogueType type, qint64 parentDirId, qint64 fileId)
{
    if (!UserCenterManager::instance()->checkLoginStatus() || isPrivateDeploy()) {
        return;
    }
    FilePropInfo filePropInfo = getFileProp(filePath);
    if (mMainWindow->isWindowRelease()) {
        return;
    }
    emit mMainWindow->getTransportController()->transferStateSignal(fileId, WaitTransfer);
    RequestTask *task = NetRequestManager::instance()->preUpload(filePath, filePropInfo, type, parentDirId, fileId);
    connect(task, &RequestTask::finished, this, &CloudManager::onPreUploadFinished);

}

void CloudManager::onGetDiskInfoFinished(int state, int code,const QByteArray &data, QVariant opaque)
{
    if (RequestTask::RequestState::Success == (RequestTask::RequestState)state) {
        QJsonObject jsonObj, jsonObject;
        if (!data.isEmpty() && CommonUtils::bufferToJson(data, &jsonObj)) {
            if (jsonObj["data"].isObject()) {
                jsonObject = jsonObj["data"].toObject();
                if (jsonObject["base_info"].isObject()) {
                    QJsonObject infoObject = jsonObject["base_info"].toObject();
                    mCacheCloudDiskInfo.usedSize = infoObject["used_size"].toVariant().toLongLong();
                    mCacheCloudDiskInfo.totalSize = infoObject["total_size"].toVariant().toLongLong();
                    emit updateDiskInfo(mCacheCloudDiskInfo);
                }
            }
        }
    }
}

void CloudManager::cloudStarred(const QList<qint64> &fileIds, const QList<qint64> &dirIds)
{
    if (!UserCenterManager::instance()->checkLoginStatus()) {
        return;
    }
    RequestTask *task = NetRequestManager::instance()->cloudStarred(fileIds, dirIds);
    connect(task, &RequestTask::finished, this, &CloudManager::onCloudStarredFinished);

}

void CloudManager::onCloudStarredFinished(int state, int code,const QByteArray &data, QVariant opaque)
{
    DeleteOprStruct deleteOpr = opaque.value<DeleteOprStruct>();
    if (RequestTask::RequestState::Success == (RequestTask::RequestState)state) {
        QJsonObject jsonObj, jsonObject;
        if (!data.isEmpty() && CommonUtils::bufferToJson(data, &jsonObj)) {
            int rspCode = jsonObj["code"].toInt();
            if (rspCode != 200) {
                // error code
                QStringList msg;
                if (rspCode == 5005 || rspCode == 5004) {
                    msg = QStringList{tr("File unexist")};
                    emit cloudFileError(deleteOpr.fileIds.at(0), rspCode);
                    return;
                } else if (rspCode == 5006) {
                    msg = QStringList{tr("File name already exists")};
                } else if (rspCode == 5009) {
                    msg = QStringList{tr("Filename is illegal")};
                } else if (rspCode == 500) {
                    msg = QStringList{tr("Internal error")};
                } else {
                    msg = QStringList{tr("Error code:") + QString::number(jsonObj["code"].toInt()),jsonObj["msg"].toString()};
                }
                MessageWidget win(msg,{tr("OK"),"",""});
                win.resetIconImage(MessageWidget::IT_Delete);
                win.exec();
                return;
            }
            if (jsonObj["data"].isObject()) {
                jsonObject = jsonObj["data"].toObject();
            }
            emit cloudStarredFinished(deleteOpr);
        }
    }
}
void CloudManager::cloudUnstarred(const QList<qint64> &fileIds, const QList<qint64> &dirIds)
{
    if (!UserCenterManager::instance()->checkLoginStatus()) {
        return;
    }
    RequestTask *task = NetRequestManager::instance()->cloudUnstarred(fileIds, dirIds);
    connect(task, &RequestTask::finished, this, &CloudManager::onCloudUnstarredFinished);

}

void CloudManager::onCloudUnstarredFinished(int state, int code,const QByteArray &data, QVariant opaque)
{
    DeleteOprStruct deleteOpr = opaque.value<DeleteOprStruct>();
    if (RequestTask::RequestState::Success == (RequestTask::RequestState)state) {
        QJsonObject jsonObj, jsonObject;
        if (!data.isEmpty() && CommonUtils::bufferToJson(data, &jsonObj)) {
            int rspCode = jsonObj["code"].toInt();
            if (rspCode != 200) {
                // error code
                QStringList msg;
                if (rspCode == 5005 || rspCode == 5004) {
                    msg = QStringList{tr("File unexist")};
                    emit cloudFileError(deleteOpr.fileIds.at(0), rspCode);
                    return;
                } else if (rspCode == 5006) {
                    msg = QStringList{tr("File name already exists")};
                } else if (rspCode == 5009) {
                    msg = QStringList{tr("Filename is illegal")};
                } else if (rspCode == 500) {
                    msg = QStringList{tr("Internal error")};
                } else {
                    msg = QStringList{tr("Error code:") + QString::number(jsonObj["code"].toInt()),jsonObj["msg"].toString()};
                }
                MessageWidget win(msg,{tr("OK"),"",""});
                win.resetIconImage(MessageWidget::IT_Delete);
                win.exec();
                return;
            }
            if (jsonObj["data"].isObject()) {
                jsonObject = jsonObj["data"].toObject();
            }
            emit cloudUnstarredFinished(deleteOpr);
        }
}
}

bool CloudManager::mResumeTrans = false;
void CloudManager::onLogin()
{
    clearFileInfoCache();
    getDiskInfo();
    getVisitRecord();
    getCloudList(CloudSpace, 0);
    getCloudList(RecycleBin, 0);

    if (!mResumeTrans) {
        mResumeTrans = true;
        loadWaitUploadFile();
        resumeTransportTask();
    }
}
void CloudManager::onLogout()
{
    mResumeTrans = false;
    mDataLoaded = false;
}

void CloudManager::loadDocInfo(const QString &filePath, const QString imagePath, int *pageCount)
{
    PDF_DOCUMENT pdfDoc = nullptr;
    int openRetCode = PDFDocument::openDocument(filePath.toStdWString(), "", &pdfDoc);
    QImage image;
    if (!pdfDoc) {
        if (PDF_ERR_PASSWORD == openRetCode) {
            image = QImage(":/updf-win-icon/ui2/2x/batch/lock.png");
        } else {
            image = QImage(":/updf-win-icon/icon2x/24icon/historyfileImg.png");
        }
        image.save(imagePath);
        *pageCount = 0;
        return;
    }
    int pageIndex = 0;
    PDF_PAGE page = (PDF_PAGE)PDFDocument::loadPage(pdfDoc, pageIndex);
    if (!page) { // 异常文件
        image = QImage(":/updf-win-icon/icon2x/24icon/historyfileImg.png");
        image.save(imagePath);
        *pageCount = 0;
        return;
    }
    QSize size = PDFDocument::getPageSizeByIndex(pdfDoc, pageIndex);
    *pageCount = PDFDocument::pageCount(pdfDoc);
    double scale = 240 * 1.0 / size.width();
    int final_width = std::ceil(scale * size.width());
    int final_height = std::ceil(scale * size.height());
    int rotation = PDFDocument::pageRotation(pdfDoc, pageIndex);

    PDF_BITMAP bitmap = PDFBitmap_Create(final_width, final_height, 1);
    PDFBitmap_FillRect(bitmap, 0, 0, final_width, final_height, 0xFFFFFFFF);
    PDF_RenderPageBitmap(bitmap, page, nullptr, 0, 0, final_width, final_height, rotation, 0);
    void *data = PDFBitmap_GetBuffer(bitmap);
    image = QImage((uchar*)data, final_width, final_height, QImage::Format_ARGB32);
    image.save(imagePath);
    PDFBitmap_Destroy(bitmap);

    PDFDocument::closePage(page);
    PDFDocument::closeDocument(pdfDoc);
}

