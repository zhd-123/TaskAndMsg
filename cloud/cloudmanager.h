#ifndef CLOUDMANAGER_H
#define CLOUDMANAGER_H

#include <QObject>
#include <QString>
#include <QVariant>
#include <QPointer>
#include <QMutex>
#include "cloudpredefine.h"


class MainWindow;
class LoadingWidgetWithCancel;
class CloudManager : public QObject
{
    Q_OBJECT
public:
    explicit CloudManager(MainWindow *mainWindow, QObject *parent = nullptr);
    FilePropInfo getFileProp(const QString &filePath, bool hash= true);
    // 续传未完成任务
    void resumeTransportTask();
    void resumeUpload(const QString &configPath, bool dealFail=false);
    void resumeDownload(const QString &configPath, bool dealFail=false);
    void getCloudList(CatelogueType type, int parentDirId);
    // 下载文件，先获取文件url
    void preDownloadFile(qint64 fileId, const QString &fileSaveDir="");
    void refreshDownloadUrl(qint64 fileId);
    void getDiskInfo();
    // 覆盖上传
    void reuploadFile(const QString &filePath, CatelogueType type, qint64 parentDirId, qint64 fileId);
    void refreshOssToken(const QString &filePath, CatelogueType type, qint64 parentDirId, qint64 fileId);

    void renameFile(qint64 fileId, const QString &newName);
    void moveToRecycle(const QList<qint64> &fileIds, const QList<qint64> &dirIds);
    void returnFromRecycle(const QList<qint64> &fileIds, const QList<qint64> &dirIds);
    void deleteFromRecycle(const QList<qint64> &fileIds, const QList<qint64> &dirIds, bool clearAll);

    void checkFileInfo(qint64 fileId, GetInfoNextStep step);
    void visitReport(const LastVisitParam &lastVisitParam);
    void removeVisitRecord(const QList<qint64> &recordIds);
    void getVisitRecord();
    void searchFile(const QString &keyword);

    void cloudStarred(const QList<qint64> &fileIds, const QList<qint64> &dirIds);
    void cloudUnstarred(const QList<qint64> &fileIds, const QList<qint64> &dirIds);

    FilePropInfo getCloudFileProp(const CloudFilePropInfo &fileInfo);
    CloudFilePropInfo getLocalCloudFileInfo(const CloudFilePropInfo &fileInfo);

    CloudFilePropInfo findFileInfoFromCache(qint64 fileId);
    CloudFilePropInfo findFileInfoFromCache(const QString &fileHash);
    void updateFileInfoForCache(const CloudFilePropInfo &fileInfo);
    bool checkIfNeedUpload(const CloudFilePropInfo &fileInfo);
    bool checkIfNeedDownload(const CloudFilePropInfo &fileInfo, bool userAccess = false);
    void clearFileInfoCache();

    void showLoadingWidget(qint64 fileId);
    void closeLoadingWidget();

    void cacheUploadWaitForDownload(const CloudFilePropInfo &fileInfo);
    void cancelUploadFileVersionCheck(const CloudFilePropInfo &fileInfo);
    void addWaitUploadListView(const QList<QString> &fileList, bool toCloudList=true);
    QList<QString> getWaitForUploadFileList();
    void removeWaitForUploadFileList(const QString &filePath);
    void finishUploadTask();
    bool cloudDataLoaded();
private:
    void startUploadTask();
    void loadDocInfo(const QString &filePath, const QString imagePath, int *pageCount);
    void appendFileInfoToCache(const CloudFilePropInfo &fileInfo);
    CloudFilePropInfo parseFileInfoFronJson(const QJsonObject &fileObj);
    void updateFileUploadFlag(const OssUploadParam &param);
    void loadWaitUploadFile();
signals:
    void removeListItem(const QString &filePath);
    void updateListItem(const CloudFilePropInfo &fileInfo);
    void getCloudDocumentFileList(const CloudFilePropInfos &fileInfos);
    void getCloudSpaceFileList(const CloudFilePropInfos &fileInfos);
    void getRecycleBinFileList(const CloudFilePropInfos &fileInfos);
    void fileUploadSuccess(const CloudFilePropInfo &fileInfo);
    void fileOssUploadResult(bool success, const OssUploadParam &param);
    void fileDownloadResult(bool success, const OssDownloadParam &param);
    void updateDiskInfo(const CloudDiskInfo &diskInfo);

    void renameFileFinished(const RenameStruct &renameStruct);
    void moveToRecycleFinished(const DeleteOprStruct &deleteOpr);
    void returnFromRecycleFinished(const DeleteOprStruct &deleteOpr);
    void deleteFromRecycleFinished(const DeleteOprStruct &deleteOpr);
    void removeVisitRecordFinished(const DeleteOprStruct &deleteOpr);
    void getVisitRecordFinished(const LastVisitRecordInfos &recordInfos);

    void checkDownloadFileInfo(const CloudFilePropInfo &fileInfo);
    void checkVersionFileInfo(const CloudFilePropInfo &fileInfo);
    void checkUploadFileInfo(const CloudFilePropInfo &fileInfo);
    void checkResumeUploadFileInfo(const CloudFilePropInfo &fileInfo);
    void checkResumeDownloadFileInfo(const CloudFilePropInfo &fileInfo);

    void cloudStarredFinished(const DeleteOprStruct &deleteOpr);
    void cloudUnstarredFinished(const DeleteOprStruct &deleteOpr);

    void insertListView(const CloudFilePropInfo &fileInfo);
    void insertToUploadListView(const CloudFilePropInfo &fileInfo);

    void localMatchFiles(const CloudFilePropInfos &fileInfos);
    void cloudMatchFiles(const CloudFilePropInfos &fileInfos);
    int cloudFileError(qint64 fileId, int code);

    void uploadWidthNewCloudVersion(const CloudFilePropInfo &fileInfo, bool *continueUpload);
    void downloadWidthNewCloudVersion(const CloudFilePropInfo &fileInfo, bool *continueDownload);

public slots:
    void onUploadFiles(const QStringList& fileList); // 准备上传的文件list
    void onPreUploadFinished(int state, int code,const QByteArray &data, QVariant opaque);
    void onRefreshOssTokenFinished(int state, int code,const QByteArray &data, QVariant opaque);
    void onUploadDoneFinished(int state, int code,const QByteArray &data, QVariant opaque);
    void onOssUploadResult(bool success, const OssUploadParam &param);
    void onOssDownloadResult(bool success, const OssDownloadParam &param);
    void onRefreshDownloadUrlFinished(int state, int code,const QByteArray &data, QVariant opaque);
    void onPreDownloadFileFinished(int state, int code,const QByteArray &data, QVariant opaque); // 获取文件URL
    void onCheckResumeUploadFileInfo(const CloudFilePropInfo &fileInfo);
    void onCheckResumeDownloadFileInfo(const CloudFilePropInfo &fileInfo);

    void onGetCloudListFinished(int state, int code,const QByteArray &data, QVariant opaque);
    void onGetDiskInfoFinished(int state, int code,const QByteArray &data, QVariant opaque);
    void onCheckFileInfoFinished(int state, int code,const QByteArray &data, QVariant opaque);
    void onRenameFileFinished(int state, int code,const QByteArray &data, QVariant opaque);
    void onSearchFileFinished(int state, int code,const QByteArray &data, QVariant opaque);

    void onMoveToRecycleFinished(int state, int code,const QByteArray &data, QVariant opaque);
    void onReturnFromRecycleFinished(int state, int code,const QByteArray &data, QVariant opaque);
    void onDeleteFromRecycleFinished(int state, int code,const QByteArray &data, QVariant opaque);

    void onVisitReportFinished(int state, int code,const QByteArray &data, QVariant opaque);
    void onRemoveVisitRecordFinished(int state, int code,const QByteArray &data, QVariant opaque);
    void onGetVisitRecordFinished(int state, int code,const QByteArray &data, QVariant opaque);

    void onCloudStarredFinished(int state, int code,const QByteArray &data, QVariant opaque);
    void onCloudUnstarredFinished(int state, int code,const QByteArray &data, QVariant opaque);

    void onLogin();
    void onLogout();
private:
    MainWindow *mMainWindow;
    CloudFilePropInfos mCloudSpaceFileInfos;
    CloudFilePropInfos mCloudDocumentFileInfos;
    CloudFilePropInfos mRecycleBinFileInfos;

    LastVisitRecordInfos mLastVisitRecordInfos;
    LoadingWidgetWithCancel *mLoadingWidget;

    QMap<qint64, QPair<CloudFilePropInfo, bool>> mWaitForDownloadToUploadMap;
    CloudDiskInfo mCacheCloudDiskInfo;
    qint64 mIgnoreErrorFileId;
    int mUploadTaskNum;
    QList<QString> mWaitForUploadFileList;
    QMutex mWaitUploadMutex;
    static bool mResumeTrans;
    bool mDataLoaded;
};

#endif // CLOUDMANAGER_H
