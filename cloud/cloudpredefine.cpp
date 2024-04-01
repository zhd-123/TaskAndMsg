#include "cloudpredefine.h"
#include "commonutils.h"
#include <QDir>
#include "./src/usercenter/usercentermanager.h"

CloudPredefine::CloudPredefine(QObject *parent)
    : QObject{parent}
{

}

QString CloudPredefine::workSpaceDir()
{
    QString workPath = CommonUtils::appWritableDataDir() + QDir::separator() + Cloud_Work_Dir;
    QDir dir(workPath);
    if (!dir.exists(workPath)) {
        dir.mkdir(workPath);
    }
    QString userWorkPath = workPath + QDir::separator() + UserCenterManager::instance()->userContext().mUserInfo.uid;
    QDir userDir(userWorkPath);
    if (!userDir.exists(userWorkPath)) {
        userDir.mkdir(userWorkPath);
    }
    return userWorkPath;
}

QString CloudPredefine::getUploadConfigFile(qint64 fileId)
{
    QString fileIdPath = QDir::fromNativeSeparators(CloudPredefine::workSpaceDir() + QDir::separator() + QString::number(fileId));
    return QDir::fromNativeSeparators(fileIdPath + QDir::separator() + OSS_UPLOAD_Config_File);
}

QString CloudPredefine::getDownloadConfigFile(qint64 fileId)
{
    QString fileIdPath = QDir::fromNativeSeparators(CloudPredefine::workSpaceDir() + QDir::separator() + QString::number(fileId));
    return QDir::fromNativeSeparators(fileIdPath + QDir::separator() + OSS_DOWNLOAD_Config_File);
}

LastVisitParam CloudPredefine::parseLastVisit(const QString &jsonStr)
{
    LastVisitParam param;
    QJsonObject jsonObject = QJsonDocument::fromJson(jsonStr.toLatin1()).object();
    param.pageIndex = jsonObject["page_index"].toInt();
    if (jsonObject.contains("scale")) {
        param.scale = jsonObject["scale"].toDouble();
    } else {
        param.scale = 1.0;
    }
    param.scaleType = jsonObject["scale_fit_type"].toInt();
    return param;
}

QString CloudPredefine::createLastVisit(const LastVisitParam &lastVisitParam)
{
    QJsonObject jsonObject;
    jsonObject.insert("page_index", lastVisitParam.pageIndex);
    jsonObject.insert("scale", lastVisitParam.scale);
    jsonObject.insert("scale_fit_type", lastVisitParam.scaleType);
    return QJsonDocument(jsonObject).toJson(QJsonDocument::Compact);
}
