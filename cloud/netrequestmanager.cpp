#include "netrequestmanager.h"
#include "settingutil.h"
#include <QHostInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include "fileoperationtool.h"
#include "common/commondata.h"
#include "./src/usercenter/usercentermanager.h"
#include "./src/pageengine/pdfcore/PDFDocument.h"
#include "framework.h"
#include "./src/chatgpt/chatgpttypedefine.h"
#include "./src/util/loginutil.h"

NetRequestManager::NetRequestManager(QObject *parent)
    : QObject{parent}
{
}

QString NetRequestManager::serverHost()
{
    QString serverHost = RequestResource::instance()->cloudServerHost();
    if (UserCenterManager::instance()->userContext().mUserInfo.companyUser) {
        serverHost = RequestResource::instance()->cloudEnterpriseServerHost();
    }
    return serverHost;
}

RequestTask* NetRequestManager::preUpload(const QString &filePath, const FilePropInfo &filePropInfo, CatelogueType type, qint64 parentDirId, qint64 fileId)
{
    QJsonObject jsonObject;
//    jsonObject.insert("pid", parentDirId);
    jsonObject.insert("name", filePropInfo.name + "." + filePropInfo.suffix);
//    jsonObject.insert("size", double(filePropInfo.size));
    jsonObject.insert("dir_type", int(type));
    jsonObject.insert("file_hash", filePropInfo.hash);
    jsonObject.insert("device_location", filePropInfo.path);

    RequestTask *task  = new RequestTask(this);
    connect(task, &RequestTask::requestFinished, task, &RequestTask::deleteLater);
    connect(task, &RequestTask::finished, FrameWork::instance(), &FrameWork::onNetworkCheck);
    RequestTask::RequestStruct reqStruct;
    RequestResource::setSslConfig(&reqStruct.request);
    setCommonRawHeader(&reqStruct.request);
    reqStruct.request.setRawHeader("Referer", serverHost().toLatin1());
    reqStruct.request.setUrl(QUrl(serverHost() + "/file/upload"));
    reqStruct.type = RequestTask::RequestType::Post;
    QByteArray byteArray = QJsonDocument(jsonObject).toJson(QJsonDocument::Compact);
    QString replaceStr = QString("{\"pid\":%1,\"size\":%2,").arg(parentDirId).arg(filePropInfo.size);
    if (fileId > 0) {
        replaceStr += QString("\"id\":%1,").arg(fileId);
    }
    byteArray= byteArray.replace("{", replaceStr.toLatin1());
    reqStruct.postData = byteArray;

    FilePropInfo propInfo = filePropInfo;
    propInfo.fileId = fileId;
    propInfo.path = filePath;
    reqStruct.opaque.setValue(propInfo);
    task->setRequest(reqStruct);
    RequestManager::instance()->addTask(task, true);
    return task;
}

RequestTask* NetRequestManager::uploadDone(const OssUploadParam &uploadParam)
{
    RequestTask *task  = new RequestTask(this);
    connect(task, &RequestTask::requestFinished, task, &RequestTask::deleteLater);
    connect(task, &RequestTask::finished, FrameWork::instance(), &FrameWork::onNetworkCheck);
    QJsonObject jsonObject;
    jsonObject.insert("total_page", uploadParam.fileInfo.totalPage);
    jsonObject.insert("file_hash", uploadParam.fileInfo.fileHash);
    jsonObject.insert("has_pwd", PDFDocument::hasPassword(uploadParam.fileInfo.filePath.toStdWString()) == true ? 1 : 0);
    RequestTask::RequestStruct reqStruct;
    RequestResource::setSslConfig(&reqStruct.request);
    setCommonRawHeader(&reqStruct.request);

    reqStruct.request.setUrl(QUrl(serverHost() + "/file/upload-done"));
    reqStruct.type = RequestTask::RequestType::Post;
    QByteArray byteArray = QJsonDocument(jsonObject).toJson(QJsonDocument::Compact);
    byteArray = byteArray.replace("{", QString("{\"id\":%1, \"size\":%2,").arg(uploadParam.fileInfo.fileId).arg(uploadParam.fileInfo.fileSize).toLatin1());
    reqStruct.postData = byteArray;
    reqStruct.opaque.setValue(uploadParam);
    task->setRequest(reqStruct);
    RequestManager::instance()->addTask(task, true);
    return task;
}

RequestTask* NetRequestManager::getCloudList(CatelogueType type, qint64 parentDirId)
{
    RequestTask *task  = new RequestTask(this);
    connect(task, &RequestTask::requestFinished, task, &RequestTask::deleteLater);
    connect(task, &RequestTask::finished, FrameWork::instance(), &FrameWork::onNetworkCheck);
    RequestTask::RequestStruct reqStruct;
    RequestResource::setSslConfig(&reqStruct.request);

    setCommonRawHeader(&reqStruct.request);
    // request param
    QMap<QString, QString> reqParmMap;
    reqParmMap.insert("dir_type", QString::number(type));
    reqParmMap.insert("next_token", "0");
    reqParmMap.insert("dir_id", QString::number(parentDirId));
    reqParmMap.insert("stared", "0");
    reqParmMap.insert("limit", "-1");
    QString urlStr = RequestResource::formatUrl(serverHost() + "/list", reqParmMap);
    reqStruct.request.setUrl(urlStr);
    reqStruct.type = RequestTask::RequestType::Get;
    reqStruct.opaque.setValue(int(type));
    task->setRequest(reqStruct);
    RequestManager::instance()->addTask(task, true);
    return task;
}

RequestTask* NetRequestManager::getFileUrl(const OssDownloadParam &downloadParam)
{
    RequestTask *task  = new RequestTask(this);
    connect(task, &RequestTask::requestFinished, task, &RequestTask::deleteLater);
    connect(task, &RequestTask::finished, FrameWork::instance(), &FrameWork::onNetworkCheck);
    RequestTask::RequestStruct reqStruct;
    RequestResource::setSslConfig(&reqStruct.request);
    setCommonRawHeader(&reqStruct.request);

    reqStruct.request.setUrl(serverHost() + "/file/" + QString::number(downloadParam.fileInfo.fileId));
    reqStruct.type = RequestTask::RequestType::Get;
    reqStruct.opaque.setValue(downloadParam);
    task->setRequest(reqStruct);
    RequestManager::instance()->addTask(task, true);
    return task;
}

RequestTask *NetRequestManager::getFileInfo(qint64 fileId, GetInfoNextStep step)
{
    RequestTask *task  = new RequestTask(this);
    connect(task, &RequestTask::requestFinished, task, &RequestTask::deleteLater);
    connect(task, &RequestTask::finished, FrameWork::instance(), &FrameWork::onNetworkCheck);
    RequestTask::RequestStruct reqStruct;
    RequestResource::setSslConfig(&reqStruct.request);
    setCommonRawHeader(&reqStruct.request);

    reqStruct.request.setUrl(QUrl(serverHost() + "/file/info/" + QString::number(fileId)));
    reqStruct.type = RequestTask::RequestType::Get;
    reqStruct.opaque.setValue(QString::number(fileId) + "&" + QString::number(step));
    task->setRequest(reqStruct);
    RequestManager::instance()->addTask(task, true);
    return task;
}

RequestTask* NetRequestManager::getCloudDiskInfo()
{
    RequestTask *task  = new RequestTask(this);
    connect(task, &RequestTask::requestFinished, task, &RequestTask::deleteLater);
    //connect(task, &RequestTask::finished, FrameWork::instance(), &FrameWork::onNetworkCheck);
    RequestTask::RequestStruct reqStruct;
    RequestResource::setSslConfig(&reqStruct.request);
    setCommonRawHeader(&reqStruct.request);

    reqStruct.request.setUrl(QUrl(serverHost() + "/user/info"));
    reqStruct.type = RequestTask::RequestType::Get;
    task->setRequest(reqStruct);
    RequestManager::instance()->addTask(task, true);
    return task;
}

RequestTask* NetRequestManager::makeFolder(CatelogueType type, qint64 parentDirId, const QString &name)
{
    RequestTask *task  = new RequestTask(this);
    connect(task, &RequestTask::requestFinished, task, &RequestTask::deleteLater);
    connect(task, &RequestTask::finished, FrameWork::instance(), &FrameWork::onNetworkCheck);
    RequestTask::RequestStruct reqStruct;
    RequestResource::setSslConfig(&reqStruct.request);

    setCommonRawHeader(&reqStruct.request);
    reqStruct.request.setUrl(QUrl(serverHost() + "/dir"));
    reqStruct.type = RequestTask::RequestType::Post;
    reqStruct.postData = QString("{\"pid\":%1,\"name\":\"%2\",\"dir_type\":%3}").arg(parentDirId).arg(name).arg(int(type)).toLatin1();
    task->setRequest(reqStruct);
    RequestManager::instance()->addTask(task, true);
    return task;
}

RequestTask* NetRequestManager::renameFile(qint64 fileId, const QString &newName)
{
    QJsonObject jsonObject;
    jsonObject.insert("name", newName);

    RequestTask *task  = new RequestTask(this);
    connect(task, &RequestTask::requestFinished, task, &RequestTask::deleteLater);
    connect(task, &RequestTask::finished, FrameWork::instance(), &FrameWork::onNetworkCheck);
    RequestTask::RequestStruct reqStruct;
    RequestResource::setSslConfig(&reqStruct.request);
    setCommonRawHeader(&reqStruct.request);

    reqStruct.request.setUrl(QUrl(serverHost() + "/rename"));
    reqStruct.type = RequestTask::RequestType::Post;
    QByteArray byteArray = QJsonDocument(jsonObject).toJson(QJsonDocument::Compact);
    QString replaceStr = QString("{\"id\":%1,").arg(fileId);
    byteArray= byteArray.replace("{", replaceStr.toLatin1());
    reqStruct.postData = byteArray;
    RenameStruct renameStruct;
    renameStruct.fileId = fileId;
    renameStruct.newName = newName;
    reqStruct.opaque.setValue(renameStruct);
    task->setRequest(reqStruct);
    RequestManager::instance()->addTask(task, true);
    return task;
}

QString NetRequestManager::formatIdArray(const QList<qint64> &ids)
{
    QString strArray = "";
    int count = ids.size();
    for (int i = 0; i < count; ++i) {
        if (i == 0) {
            strArray += "[";
        }
        strArray.append("%" + QString::number(i + 1));
        if (i == count - 1) {
            strArray += "]";
        } else {
            strArray += ",";
        }
    }
    for (int i = 0; i < count; ++i) {
        strArray = strArray.arg(ids.at(i));
    }
    if (strArray.isEmpty()) {
        strArray = "[]";
    }
    return strArray;
}

RequestTask *NetRequestManager::moveFileAndDir(qint64 destDirId, const QList<qint64> &fileIds, const QList<qint64> &dirIds)
{
    QJsonObject jsonObject;
    QString fileIdArray = formatIdArray(fileIds);
    QString dirIdArray = formatIdArray(dirIds);
//    jsonObject.insert("dest_dir_id", destDirId);
    jsonObject.insert("file_ids", fileIdArray);
    jsonObject.insert("dir_ids", dirIdArray);

    RequestTask *task  = new RequestTask(this);
    connect(task, &RequestTask::requestFinished, task, &RequestTask::deleteLater);
    connect(task, &RequestTask::finished, FrameWork::instance(), &FrameWork::onNetworkCheck);
    RequestTask::RequestStruct reqStruct;
    RequestResource::setSslConfig(&reqStruct.request);

    setCommonRawHeader(&reqStruct.request);
    reqStruct.request.setUrl(QUrl(serverHost() + "/move"));
    reqStruct.type = RequestTask::RequestType::Post;
    QByteArray byteArray = QJsonDocument(jsonObject).toJson(QJsonDocument::Compact);
    byteArray.replace("\"[", "[");
    byteArray.replace("]\"", "]");
    byteArray.replace("{", QString("{\"dest_dir_id\":%1,").arg(destDirId).toLatin1());
    reqStruct.postData = byteArray;
    task->setRequest(reqStruct);
    RequestManager::instance()->addTask(task, true);
    return task;
}

RequestTask *NetRequestManager::moveToRecycle(const QList<qint64> &fileIds, const QList<qint64> &dirIds)
{
    QJsonObject jsonObject;
    QString fileIdArray = formatIdArray(fileIds);
    QString dirIdArray = formatIdArray(dirIds);
    jsonObject.insert("file_ids", fileIdArray);
    jsonObject.insert("dir_ids", dirIdArray);

    RequestTask *task  = new RequestTask(this);
    connect(task, &RequestTask::requestFinished, task, &RequestTask::deleteLater);
    connect(task, &RequestTask::finished, FrameWork::instance(), &FrameWork::onNetworkCheck);
    RequestTask::RequestStruct reqStruct;
    RequestResource::setSslConfig(&reqStruct.request);

    setCommonRawHeader(&reqStruct.request);
    reqStruct.request.setUrl(QUrl(serverHost() + "/recycle"));
    reqStruct.type = RequestTask::RequestType::Post;
    QByteArray byteArray = QJsonDocument(jsonObject).toJson(QJsonDocument::Compact);
    byteArray.replace("\"[", "[");
    byteArray.replace("]\"", "]");
    reqStruct.postData = byteArray;
    DeleteOprStruct deleteOpr;
    deleteOpr.fileIds = fileIds;
    deleteOpr.dirIds = dirIds;
    reqStruct.opaque.setValue(deleteOpr);
    task->setRequest(reqStruct);
    RequestManager::instance()->addTask(task, true);
    return task;
}

RequestTask *NetRequestManager::returnFromRecycle(const QList<qint64> &fileIds, const QList<qint64> &dirIds)
{
    QJsonObject jsonObject;
//    QJsonArray fileIdArray;
//    QJsonArray dirIdArray;
//    for (int fileId : fileIds) {
//        fileIdArray.append(fileId);
//    }
//    for (int dirId : dirIds) {
//        dirIdArray.append(dirId);
//    }
    QString fileIdArray = formatIdArray(fileIds);
    QString dirIdArray = formatIdArray(dirIds);
    jsonObject.insert("file_ids", fileIdArray);
    jsonObject.insert("dir_ids", dirIdArray);


    RequestTask *task  = new RequestTask(this);
    connect(task, &RequestTask::requestFinished, task, &RequestTask::deleteLater);
    connect(task, &RequestTask::finished, FrameWork::instance(), &FrameWork::onNetworkCheck);
    RequestTask::RequestStruct reqStruct;
    RequestResource::setSslConfig(&reqStruct.request);

    setCommonRawHeader(&reqStruct.request);
    reqStruct.request.setUrl(QUrl(serverHost() + "/un-recycle"));
    reqStruct.type = RequestTask::RequestType::Post;
    QByteArray byteArray = QJsonDocument(jsonObject).toJson(QJsonDocument::Compact);
    byteArray.replace("\"[", "[");
    byteArray.replace("]\"", "]");
    reqStruct.postData = byteArray;
    DeleteOprStruct deleteOpr;
    deleteOpr.fileIds = fileIds;
    deleteOpr.dirIds = dirIds;
    reqStruct.opaque.setValue(deleteOpr);
    task->setRequest(reqStruct);
    RequestManager::instance()->addTask(task, true);
    return task;
}

RequestTask *NetRequestManager::deleteFromRecycle(const QList<qint64> &fileIds, const QList<qint64> &dirIds, bool clearAll)
{
    QJsonObject jsonObject;
    QString fileIdArray = formatIdArray(fileIds);
    QString dirIdArray = formatIdArray(dirIds);

    jsonObject.insert("delete_all", clearAll ? 1 : 0);
    jsonObject.insert("file_ids", fileIdArray);
    jsonObject.insert("dir_ids", dirIdArray);

    RequestTask *task  = new RequestTask(this);
    connect(task, &RequestTask::requestFinished, task, &RequestTask::deleteLater);
    connect(task, &RequestTask::finished, FrameWork::instance(), &FrameWork::onNetworkCheck);
    RequestTask::RequestStruct reqStruct;
    RequestResource::setSslConfig(&reqStruct.request);

    setCommonRawHeader(&reqStruct.request);
    reqStruct.request.setUrl(QUrl(serverHost() + "/delete"));
    reqStruct.type = RequestTask::RequestType::Post;
    QByteArray byteArray = QJsonDocument(jsonObject).toJson(QJsonDocument::Compact);
    byteArray.replace("\"[", "[");
    byteArray.replace("]\"", "]");
    reqStruct.postData = byteArray;
    DeleteOprStruct deleteOpr;
    deleteOpr.fileIds = fileIds;
    deleteOpr.dirIds = dirIds;
    deleteOpr.clearAll = clearAll;
    reqStruct.opaque.setValue(deleteOpr);
    task->setRequest(reqStruct);
    RequestManager::instance()->addTask(task, true);
    return task;
}

RequestTask *NetRequestManager::visitReport(const LastVisitParam &lastVisitParam)
{
    RequestTask *task  = new RequestTask(this);
    connect(task, &RequestTask::requestFinished, task, &RequestTask::deleteLater);
    connect(task, &RequestTask::finished, FrameWork::instance(), &FrameWork::onNetworkCheck);
    RequestTask::RequestStruct reqStruct;
    RequestResource::setSslConfig(&reqStruct.request);
    setCommonRawHeader(&reqStruct.request);

    reqStruct.request.setUrl(QUrl(serverHost() + "/file/report"));
    reqStruct.type = RequestTask::RequestType::Post;
    reqStruct.postData = QString("{\"id\":%1,\"visit_time\":%2,\"visit_params\":\"%3\"}").arg(lastVisitParam.fileId)
            .arg(lastVisitParam.lastVisitTime.toSecsSinceEpoch()).arg(CloudPredefine::createLastVisit(lastVisitParam).replace("\"", "\\\"")).toLatin1();
    task->setRequest(reqStruct);
    RequestManager::instance()->addTask(task, true);
    return task;
}

RequestTask *NetRequestManager::removeVisitRecord(const QList<qint64> &recordIds)
{
    QJsonObject jsonObject;
    QString recordIdArray = formatIdArray(recordIds);
    jsonObject.insert("ids", recordIdArray);

    RequestTask *task  = new RequestTask(this);
    connect(task, &RequestTask::requestFinished, task, &RequestTask::deleteLater);
    connect(task, &RequestTask::finished, FrameWork::instance(), &FrameWork::onNetworkCheck);
    RequestTask::RequestStruct reqStruct;
    RequestResource::setSslConfig(&reqStruct.request);

    setCommonRawHeader(&reqStruct.request);
    reqStruct.request.setUrl(QUrl(serverHost() + "/file/remove-visit-record"));
    reqStruct.type = RequestTask::RequestType::Post;
    QByteArray byteArray = QJsonDocument(jsonObject).toJson(QJsonDocument::Compact);
    byteArray.replace("\"[", "[");
    byteArray.replace("]\"", "]");
    reqStruct.postData = byteArray;
    DeleteOprStruct deleteOpr;
    deleteOpr.fileIds = recordIds;
    reqStruct.opaque.setValue(deleteOpr);
    task->setRequest(reqStruct);
    RequestManager::instance()->addTask(task, true);
    return task;
}

RequestTask *NetRequestManager::getVisitRecord()
{
    RequestTask *task  = new RequestTask(this);
    connect(task, &RequestTask::requestFinished, task, &RequestTask::deleteLater);
    connect(task, &RequestTask::finished, FrameWork::instance(), &FrameWork::onNetworkCheck);
    RequestTask::RequestStruct reqStruct;
    RequestResource::setSslConfig(&reqStruct.request);
    setCommonRawHeader(&reqStruct.request);
    // request param
    QMap<QString, QString> reqParmMap;
    QString urlStr = RequestResource::formatUrl(serverHost() + "/file/recent-visit-record", reqParmMap);
    reqStruct.request.setUrl(urlStr);
    reqStruct.type = RequestTask::RequestType::Get;
    task->setRequest(reqStruct);
    RequestManager::instance()->addTask(task, true);
    return task;
}

RequestTask *NetRequestManager::cloudStarred(const QList<qint64> &fileIds, const QList<qint64> &dirIds)
{
    QJsonObject jsonObject;
    QString fileIdArray = formatIdArray(fileIds);
    QString dirIdArray = formatIdArray(dirIds);
    jsonObject.insert("file_ids", fileIdArray);
    jsonObject.insert("dir_ids", dirIdArray);

    RequestTask *task  = new RequestTask(this);
    connect(task, &RequestTask::requestFinished, task, &RequestTask::deleteLater);
    connect(task, &RequestTask::finished, FrameWork::instance(), &FrameWork::onNetworkCheck);
    RequestTask::RequestStruct reqStruct;
    RequestResource::setSslConfig(&reqStruct.request);

    setCommonRawHeader(&reqStruct.request);
    reqStruct.request.setUrl(QUrl(serverHost() + "/stared"));
    reqStruct.type = RequestTask::RequestType::Post;
    QByteArray byteArray = QJsonDocument(jsonObject).toJson(QJsonDocument::Compact);
    byteArray.replace("\"[", "[");
    byteArray.replace("]\"", "]");
    reqStruct.postData = byteArray;
    DeleteOprStruct deleteOpr;
    deleteOpr.fileIds = fileIds;
    deleteOpr.dirIds = dirIds;
    reqStruct.opaque.setValue(deleteOpr);
    task->setRequest(reqStruct);
    RequestManager::instance()->addTask(task, true);
    return task;
}

RequestTask *NetRequestManager::cloudUnstarred(const QList<qint64> &fileIds, const QList<qint64> &dirIds)
{
    QJsonObject jsonObject;
    QString fileIdArray = formatIdArray(fileIds);
    QString dirIdArray = formatIdArray(dirIds);
    jsonObject.insert("file_ids", fileIdArray);
    jsonObject.insert("dir_ids", dirIdArray);

    RequestTask *task  = new RequestTask(this);
    connect(task, &RequestTask::requestFinished, task, &RequestTask::deleteLater);
    connect(task, &RequestTask::finished, FrameWork::instance(), &FrameWork::onNetworkCheck);
    RequestTask::RequestStruct reqStruct;
    RequestResource::setSslConfig(&reqStruct.request);

    setCommonRawHeader(&reqStruct.request);
    reqStruct.request.setUrl(QUrl(serverHost() + "/un-star"));
    reqStruct.type = RequestTask::RequestType::Post;
    QByteArray byteArray = QJsonDocument(jsonObject).toJson(QJsonDocument::Compact);
    byteArray.replace("\"[", "[");
    byteArray.replace("]\"", "]");
    reqStruct.postData = byteArray;
    DeleteOprStruct deleteOpr;
    deleteOpr.fileIds = fileIds;
    deleteOpr.dirIds = dirIds;
    reqStruct.opaque.setValue(deleteOpr);
    task->setRequest(reqStruct);
    RequestManager::instance()->addTask(task, true);
    return task;
}

RequestTask *NetRequestManager::searchFile(const QString &keyword)
{
    RequestTask *task  = new RequestTask(this);
    connect(task, &RequestTask::requestFinished, task, &RequestTask::deleteLater);
    connect(task, &RequestTask::finished, FrameWork::instance(), &FrameWork::onNetworkCheck);
    QJsonObject jsonObject;
    jsonObject.insert("keyword", keyword);
    jsonObject.insert("page", 0);
    jsonObject.insert("size", 20);

    RequestTask::RequestStruct reqStruct;
    RequestResource::setSslConfig(&reqStruct.request);
    setCommonRawHeader(&reqStruct.request);

    reqStruct.request.setUrl(QUrl(serverHost() + "/search"));
    reqStruct.type = RequestTask::RequestType::Post;
    QByteArray byteArray = QJsonDocument(jsonObject).toJson(QJsonDocument::Compact);
    reqStruct.postData = byteArray;
    task->setRequest(reqStruct);
    RequestManager::instance()->addTask(task, true);
    return task;
}
