#ifndef NETREQUESTMANAGER_H
#define NETREQUESTMANAGER_H

#include <QObject>
#include "cloudpredefine.h"
#include "../util/singleton.h"
#include "requestutil.h"
#include "./src/chatgpt/chatgpttypedefine.h"

class NetRequestManager : public QObject
{
    Q_OBJECT
    DECLARE_SINGLETON(NetRequestManager)
public:
    explicit NetRequestManager(QObject *parent = nullptr);
    // 覆盖上传需要fileId
    RequestTask* preUpload(const QString &filePath, const FilePropInfo &filePropInfo, CatelogueType type, qint64 parentDirId, qint64 fileId=0);
    RequestTask* uploadDone(const OssUploadParam &uploadParam);
    RequestTask* getCloudList(CatelogueType type, qint64 parentDirId);
    RequestTask* getFileUrl(const OssDownloadParam &downloadParam);
    RequestTask* getFileInfo(qint64 fileId, GetInfoNextStep step);

    RequestTask* getCloudDiskInfo();
    RequestTask* makeFolder(CatelogueType type, qint64 parentDirId, const QString &name);
    RequestTask* renameFile(qint64 fileId, const QString &newName);
    RequestTask* moveFileAndDir(qint64 destDirId, const QList<qint64> &fileIds, const QList<qint64> &dirIds);

    RequestTask* moveToRecycle(const QList<qint64> &fileIds, const QList<qint64> &dirIds);
    RequestTask* returnFromRecycle(const QList<qint64> &fileIds, const QList<qint64> &dirIds);
    RequestTask* deleteFromRecycle(const QList<qint64> &fileIds, const QList<qint64> &dirIds, bool clearAll);

    RequestTask* visitReport(const LastVisitParam &lastVisitParam);
    RequestTask* removeVisitRecord(const QList<qint64> &recordIds);
    RequestTask* getVisitRecord();
    RequestTask* cloudStarred(const QList<qint64> &fileIds, const QList<qint64> &dirIds);
    RequestTask* cloudUnstarred(const QList<qint64> &fileIds, const QList<qint64> &dirIds);

    RequestTask* searchFile(const QString &keyword);
private:
    QString serverHost();
    QString formatIdArray(const QList<qint64> &ids);
signals:

};

#endif // NETREQUESTMANAGER_H
