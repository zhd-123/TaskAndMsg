#include "serializeconfig.h"
#include "commonutils.h"
#include <QDir>
#include <QSettings>

SerializeConfig::SerializeConfig(QObject *parent)
    : QObject{parent}
{
}

CloudFilePropInfos SerializeConfig::loadSaveAsRecord()
{
    CloudFilePropInfos infos;
    QSettings settings(CloudPredefine::workSpaceDir() + QDir::separator() + SaveAs_Config_File, QSettings::IniFormat);
    QList<QString> fileIds = settings.childGroups();
    for (QString key : fileIds) {
        CloudFilePropInfo propInfo;
        propInfo.fileId = settings.value(key + "/" + "id").toLongLong();
        propInfo.fileHash = settings.value(key + "/" + "fileHash").toString();
        propInfo.filePath = settings.value(key + "/" + "filePath").toString();
        propInfo.fileVersion = settings.value(key + "/" + "fileVersion").toLongLong();
        propInfo.fileShowName = settings.value(key + "/" + "fileShowName").toString();
        propInfo.fileSize = settings.value(key + "/" + "fileSize").toULongLong();
        propInfo.totalPage = settings.value(key + "/" + "totalPage").toInt();
        propInfo.fileCrc = settings.value(key + "/" + "fileCrc").toULongLong();
        propInfo.createTime = QDateTime::fromSecsSinceEpoch(settings.value(key + "/" + "createTime").toDouble());

        if (propInfo.fileShowName.contains(PDF_FILE_SUFFIX)) {
            propInfo.fileShowName.replace(PDF_FILE_SUFFIX, "");
        }
        if (propInfo.fileShowName.contains(PDF_FILE_UPSUFFIX)) {
            propInfo.fileShowName.replace(PDF_FILE_UPSUFFIX, "");
        }
        infos.append(propInfo);
    }
    return infos;
}

void SerializeConfig::addSaveAsRecord(const CloudFilePropInfo &propInfo)
{
    QSettings settings(CloudPredefine::workSpaceDir() + QDir::separator() + SaveAs_Config_File, QSettings::IniFormat);
    QString path = propInfo.filePath;
    QString key = path.replace("/", "&").replace(":", "") + QString::number(propInfo.fileId);
    settings.setValue(key + "/" + "id", propInfo.fileId);
    settings.setValue(key + "/" + "fileHash", propInfo.fileHash);
    settings.setValue(key + "/" + "fileShowName", propInfo.fileShowName);
    settings.setValue(key + "/" + "fileVersion", propInfo.fileVersion);
    settings.setValue(key + "/" + "filePath", propInfo.filePath);
    settings.setValue(key + "/" + "fileSize", propInfo.fileSize);
    settings.setValue(key + "/" + "totalPage", propInfo.totalPage);
    settings.setValue(key + "/" + "fileCrc", propInfo.fileCrc);
    settings.setValue(key + "/" + "createTime", propInfo.createTime.toSecsSinceEpoch());

    settings.sync();
}

void SerializeConfig::removeSaveAsRecord(qint64 fileId, const QString &filePath)
{
    QString path = filePath;
    QString key = path.replace("/", "&").replace(":", "") + QString::number(fileId);
    QSettings settings(CloudPredefine::workSpaceDir() + QDir::separator() + SaveAs_Config_File, QSettings::IniFormat);
    settings.remove(key + "/" + "id");
    settings.remove(key + "/" + "fileHash");
    settings.remove(key + "/" + "filePath");
    settings.remove(key + "/" + "fileShowName");
    settings.remove(key + "/" + "fileVersion");
    settings.remove(key + "/" + "fileSize");
    settings.remove(key + "/" + "totalPage");
    settings.remove(key + "/" + "fileCrc");
    settings.remove(key + "/" + "createTime");
}

void SerializeConfig::clearSaveAsRecord()
{
    QFile(CloudPredefine::workSpaceDir() + QDir::separator() + SaveAs_Config_File).remove();
}
///////////////

CloudFilePropInfos SerializeConfig::loadTransDoneFileConfig()
{
    CloudFilePropInfos infos;
    QSettings settings(CloudPredefine::workSpaceDir() + QDir::separator() + TransDone_Config_File, QSettings::IniFormat);
    QList<QString> fileIds = settings.childGroups();
    for (QString id : fileIds) {
        CloudFilePropInfo propInfo;
        propInfo.fileId = settings.value(id + "/" + "id").toLongLong();
        propInfo.fileHash = settings.value(id + "/" + "fileHash").toString();
        propInfo.fileVersion = settings.value(id + "/" + "fileVersion").toLongLong();
        propInfo.fileShowName = settings.value(id + "/" + "fileShowName").toString();
        propInfo.transPrefix = settings.value(id + "/" + "transPrefix").toString();
        if (propInfo.fileShowName.contains(PDF_FILE_SUFFIX)) {
            propInfo.fileShowName.replace(PDF_FILE_SUFFIX, "");
        }
        if (propInfo.fileShowName.contains(PDF_FILE_UPSUFFIX)) {
            propInfo.fileShowName.replace(PDF_FILE_UPSUFFIX, "");
        }
        infos.append(propInfo);
    }
    return infos;
}

CloudFilePropInfo SerializeConfig::checkTransDoneFileInfo(const QString &fileId, const QString &prefix)
{
    CloudFilePropInfo propInfo;
    QSettings settings(CloudPredefine::workSpaceDir() + QDir::separator() + TransDone_Config_File, QSettings::IniFormat);
    QList<QString> fileIds = settings.childGroups();
    QString id = prefix + fileId;
    if (fileIds.contains(id)) {
        propInfo.fileId = settings.value(id + "/" + "id").toLongLong();
        propInfo.fileHash = settings.value(id + "/" + "fileHash").toString();
        propInfo.fileVersion = settings.value(id + "/" + "fileVersion").toLongLong();
        propInfo.fileShowName = settings.value(id + "/" + "fileShowName").toString();
        propInfo.transPrefix = settings.value(id + "/" + "transPrefix").toString();
        if (propInfo.fileShowName.contains(PDF_FILE_SUFFIX)) {
            propInfo.fileShowName.replace(PDF_FILE_SUFFIX, "");
        }
        if (propInfo.fileShowName.contains(PDF_FILE_UPSUFFIX)) {
            propInfo.fileShowName.replace(PDF_FILE_UPSUFFIX, "");
        }
    }
    return propInfo;
}

void SerializeConfig::updateTransDoneFileConfig(const CloudFilePropInfo &propInfo, const QString &prefix)
{
    QSettings settings(CloudPredefine::workSpaceDir() + QDir::separator() + TransDone_Config_File, QSettings::IniFormat);
    settings.setValue(prefix + QString::number(propInfo.fileId) + "/" + "id", propInfo.fileId);
    settings.setValue(prefix + QString::number(propInfo.fileId) + "/" + "fileHash", propInfo.fileHash);
    settings.setValue(prefix + QString::number(propInfo.fileId) + "/" + "fileShowName", propInfo.fileShowName);
    settings.setValue(prefix + QString::number(propInfo.fileId) + "/" + "fileVersion", propInfo.fileVersion);
    settings.setValue(prefix + QString::number(propInfo.fileId) + "/" + "transPrefix", prefix);

    settings.sync();
}

void SerializeConfig::removeTransDoneFileConfig(const QString &fileId, const QString &prefix)
{
    QSettings settings(CloudPredefine::workSpaceDir() + QDir::separator() + TransDone_Config_File, QSettings::IniFormat);
    settings.remove(prefix + fileId + "/" + "id");
    settings.remove(prefix + fileId + "/" + "fileHash");
    settings.remove(prefix + fileId + "/" + "fileShowName");
    settings.remove(prefix + fileId + "/" + "fileVersion");
    settings.remove(prefix + fileId + "/" + "transPrefix");
}

void SerializeConfig::clearTransDoneFileConfig()
{
    QFile(CloudPredefine::workSpaceDir() + QDir::separator() + TransDone_Config_File).remove();
}


OssUploadParam SerializeConfig::readUploadParam(const QString &configFile)
{
    OssUploadParam param;
    if (!QFile::exists(configFile)) {
        return param;
    }
    QSettings settings(configFile, QSettings::IniFormat);
    param.fileInfo.filePath = settings.value("filePath").toString();
    param.fileInfo.fileShowName = settings.value("fileShowName").toString();
    param.fileInfo.fileRemotePath = settings.value("fileRemotePath").toString();
    param.fileInfo.logoPath = settings.value("logoPath").toString();
    param.fileInfo.logoRemotePath = settings.value("logoRemotePath").toString();
    param.fileInfo.fileVersion = settings.value("fileVersion").toLongLong();
    param.fileInfo.fileCrc = settings.value("fileCrc").toULongLong();
    param.fileInfo.fileHash = settings.value("fileHash").toString();
    param.fileInfo.fileId = settings.value("fileId").toLongLong();
    param.fileInfo.fileSize = settings.value("fileSize").toULongLong();
    param.fileInfo.totalPage = settings.value("totalPage").toLongLong();
    param.uploadId = settings.value("uploadId").toString();
    param.uploadSize = settings.value("uploadSize").toULongLong();
    param.partTotalNum = settings.value("partTotalNum").toInt();
    param.fileInfo.transFail = settings.value("transFail").toBool();

    param.credentials.accessKeyId = settings.value("accessKeyId").toString();
    param.credentials.accessKeySecret = settings.value("accessKeySecret").toString();
    param.credentials.bucketName = settings.value("bucketName").toString();
    param.credentials.endpoint = settings.value("endpoint").toString();
    param.credentials.securityToken = settings.value("securityToken").toString();
    param.credentials.expiration = QDateTime::fromSecsSinceEpoch(settings.value("expiration").toDouble());
    param.credentials.currentTime = QDateTime::fromSecsSinceEpoch(settings.value("currentTime").toDouble());
    param.partArray.clear();
    for (int i = 0; i < param.partTotalNum; ++i) {
        OssUploadPartParam partParam;
        partParam.partIndex = settings.value("part" + QString::number(i) + "/partIndex").toInt();
        partParam.startPos = settings.value("part" + QString::number(i) + "/startPos").toULongLong();
        partParam.endPos = settings.value("part" + QString::number(i) + "/endPos").toULongLong();
        partParam.eTag = settings.value("part" + QString::number(i) + "/eTag").toString();
        param.partArray.append(partParam);
    }
    return param;
}

void SerializeConfig::writeUploadParam(const QString &configFile, const OssUploadParam &param)
{
    if (configFile.isEmpty()) {
        return;
    }
    QString filePath = configFile;
    QString dir = filePath.left(filePath.lastIndexOf("/"));
    CommonUtils::mkDirRecursive(dir);

    QSettings settings(filePath, QSettings::IniFormat);
    settings.setValue("filePath", param.fileInfo.filePath);
    settings.setValue("fileRemotePath", param.fileInfo.fileRemotePath);
    settings.setValue("logoPath", param.fileInfo.logoPath);
    settings.setValue("logoRemotePath", param.fileInfo.logoRemotePath);
    settings.setValue("fileVersion", param.fileInfo.fileVersion);
    settings.setValue("fileCrc", param.fileInfo.fileCrc);
    settings.setValue("fileHash", param.fileInfo.fileHash);
    settings.setValue("fileId", param.fileInfo.fileId);
    settings.setValue("fileShowName", param.fileInfo.fileShowName);
    settings.setValue("fileSize", param.fileInfo.fileSize);
    settings.setValue("totalPage", param.fileInfo.totalPage);
    settings.setValue("uploadId", param.uploadId);
    settings.setValue("uploadSize", param.uploadSize);
    settings.setValue("partTotalNum", param.partTotalNum);
    settings.setValue("transFail", param.fileInfo.transFail);

    settings.setValue("accessKeyId", param.credentials.accessKeyId);
    settings.setValue("accessKeySecret", param.credentials.accessKeySecret);
    settings.setValue("bucketName", param.credentials.bucketName);
    settings.setValue("endpoint", param.credentials.endpoint);
    settings.setValue("securityToken", param.credentials.securityToken);
    settings.setValue("expiration", param.credentials.expiration.toSecsSinceEpoch());
    settings.setValue("currentTime", param.credentials.currentTime.toSecsSinceEpoch());
    for (int i = 0; i < param.partTotalNum; ++i) {
        settings.setValue("part" + QString::number(i) + "/partIndex", param.partArray[i].partIndex);
        settings.setValue("part" + QString::number(i) + "/startPos", param.partArray[i].startPos);
        settings.setValue("part" + QString::number(i) + "/endPos", param.partArray[i].endPos);
        settings.setValue("part" + QString::number(i) + "/eTag", param.partArray[i].eTag);
    }
    settings.sync();
}


OssDownloadParam SerializeConfig::readDownloadParam(const QString &configFile)
{
    OssDownloadParam param;
    if (!QFile::exists(configFile)) {
        return param;
    }
    QSettings settings(configFile, QSettings::IniFormat);
    param.saveDir = settings.value("saveDir").toString();
    param.saveTempName = settings.value("saveTempName").toString();
    param.fileInfo.filePath = settings.value("filePath").toString();
    param.fileInfo.fileRemotePath = settings.value("fileRemotePath").toString();
    param.fileInfo.logoPath = settings.value("logoPath").toString();
    param.fileInfo.logoRemotePath = settings.value("logoRemotePath").toString();
    param.fileInfo.fileVersion = settings.value("fileVersion").toLongLong();
    param.fileInfo.fileCrc = settings.value("fileCrc").toULongLong();
    param.fileInfo.fileHash = settings.value("fileHash").toString();
    param.fileInfo.fileId = settings.value("fileId").toLongLong();
    param.fileInfo.fileSize = settings.value("fileSize").toULongLong();
    param.fileInfo.totalPage = settings.value("totalPage").toLongLong();
    param.fileInfo.fileShowName = settings.value("fileShowName").toString();
    param.fileInfo.fileUrl = settings.value("fileUrl").toString();
    param.fileInfo.urlCurrentTime = QDateTime::fromSecsSinceEpoch(settings.value("urlCurrentTime").toDouble());
    param.fileInfo.urlExpiration = QDateTime::fromSecsSinceEpoch(settings.value("urlExpiration").toDouble());
    param.downloadSize = settings.value("downloadSize").toULongLong();
    param.partTotalNum = settings.value("partTotalNum").toInt();
    param.fileInfo.transFail = settings.value("transFail").toBool();

    if (param.fileInfo.fileShowName.contains(PDF_FILE_SUFFIX)) {
        param.fileInfo.fileShowName.replace(PDF_FILE_SUFFIX, "");
    }
    if (param.fileInfo.fileShowName.contains(PDF_FILE_UPSUFFIX)) {
        param.fileInfo.fileShowName.replace(PDF_FILE_UPSUFFIX, "");
    }
    param.partArray.clear();
    for (int i = 0; i < param.partTotalNum; ++i) {
        OssDownloadPartParam partParam;
        partParam.partIndex = settings.value("part" + QString::number(i) + "/partIndex").toInt();
        partParam.startPos = settings.value("part" + QString::number(i) + "/startPos").toULongLong();
        partParam.endPos = settings.value("part" + QString::number(i) + "/endPos").toULongLong();
        param.partArray.append(partParam);
    }
    return param;
}

void SerializeConfig::writeDownloadParam(const QString &configFile, const OssDownloadParam &param)
{
    if (configFile.isEmpty()) {
        return;
    }
    QString filePath = configFile;
    QString dir = filePath.left(filePath.lastIndexOf("/"));
    CommonUtils::mkDirRecursive(dir);
    QSettings settings(filePath, QSettings::IniFormat);
    settings.setValue("saveDir", param.saveDir);
    settings.setValue("saveTempName", param.saveTempName);
    settings.setValue("filePath", param.fileInfo.filePath);
    settings.setValue("fileRemotePath", param.fileInfo.fileRemotePath);
    settings.setValue("fileVersion", param.fileInfo.fileVersion);
    settings.setValue("fileCrc", param.fileInfo.fileCrc);
    settings.setValue("fileHash", param.fileInfo.fileHash);
    settings.setValue("fileId", param.fileInfo.fileId);
    settings.setValue("fileShowName", param.fileInfo.fileShowName);
    settings.setValue("fileSize", param.fileInfo.fileSize);
    settings.setValue("totalPage", param.fileInfo.totalPage);
    settings.setValue("fileUrl", param.fileInfo.fileUrl);
    settings.setValue("urlCurrentTime", param.fileInfo.urlCurrentTime.toSecsSinceEpoch());
    settings.setValue("urlExpiration", param.fileInfo.urlExpiration.toSecsSinceEpoch());
    settings.setValue("downloadSize", param.downloadSize);
    settings.setValue("partTotalNum", param.partTotalNum);
    settings.setValue("transFail", param.fileInfo.transFail);
    for (int i = 0; i < param.partTotalNum; ++i) {
        settings.setValue("part" + QString::number(i) + "/partIndex", param.partArray[i].partIndex);
        settings.setValue("part" + QString::number(i) + "/startPos", param.partArray[i].startPos);
        settings.setValue("part" + QString::number(i) + "/endPos", param.partArray[i].endPos);
    }
    settings.sync();
}

void SerializeConfig::recordTransportTask(qint64 fileId, bool upload)
{
    QString filePath = "";
    Q_ASSERT(fileId);
    if (fileId <= 0) {
        return;
    }
    QString key = "";
    if (upload) {
        key = Upload_File_Prefix + QString::number(fileId);
        filePath = CloudPredefine::getUploadConfigFile(fileId);
    } else {
        key = Download_File_Prefix + QString::number(fileId);
        filePath = CloudPredefine::getDownloadConfigFile(fileId);
    }
    QString configFilePath = CloudPredefine::workSpaceDir() + QDir::separator() + OSS_TRANSPORT_TASK_File;
    QSettings settings(configFilePath, QSettings::IniFormat);
    settings.setValue(key, filePath);
    settings.sync();

    int uploadTask = 0;
    for (QString key : settings.allKeys()) {
        if (key.contains(Upload_File_Prefix)) {
            OssUploadParam uploadParam = readUploadParam(settings.value(key).toString());
            QString fileIdPath = QDir::fromNativeSeparators(CloudPredefine::workSpaceDir() + QDir::separator() + QString::number(uploadParam.fileInfo.fileId));
            if (uploadParam.fileInfo.filePath.contains(fileIdPath)) {
                continue; // 云同步任务
            }
            uploadTask += 1;
        }
    }
    QString configFilePath1 = CloudPredefine::workSpaceDir() + QDir::separator() + WAIT_UPLOAD_File;
    QSettings settings1(configFilePath1, QSettings::IniFormat);
    uploadTask += settings1.allKeys().size();
    emit transTaskNumber(uploadTask);
}

void SerializeConfig::removeTransportTask(qint64 fileId, bool upload)
{
    QString key = "";
    if (upload) {
        key = Upload_File_Prefix + QString::number(fileId);
    } else {
        key = Download_File_Prefix + QString::number(fileId);
    }
    QString configFilePath = CloudPredefine::workSpaceDir() + QDir::separator() + OSS_TRANSPORT_TASK_File;
    QSettings settings(configFilePath, QSettings::IniFormat);
    settings.remove(key);
    settings.sync();
    int uploadTask = 0;
    for (QString key : settings.allKeys()) {
        if (key.contains(Upload_File_Prefix)) {
            OssUploadParam uploadParam = readUploadParam(settings.value(key).toString());
            QString fileIdPath = QDir::fromNativeSeparators(CloudPredefine::workSpaceDir() + QDir::separator() + QString::number(uploadParam.fileInfo.fileId));
            if (uploadParam.fileInfo.filePath.contains(fileIdPath)) {
                continue; // 云同步任务
            }
            uploadTask += 1;
        }
    }
    QString configFilePath1 = CloudPredefine::workSpaceDir() + QDir::separator() + WAIT_UPLOAD_File;
    QSettings settings1(configFilePath1, QSettings::IniFormat);
    uploadTask += settings1.allKeys().size();
    emit transTaskNumber(uploadTask);
}

QMap<QString, QString> SerializeConfig::loadPauseTask()
{
    QMap<QString, QString> map;
    QString configFilePath = CloudPredefine::workSpaceDir() + QDir::separator() + OSS_TRANSPORT_TASK_File;
    QSettings settings(configFilePath, QSettings::IniFormat);
    QStringList keyList = settings.allKeys();

    int uploadTask = 0;
    for (QString key : keyList) {
        map.insert(key, settings.value(key).toString());
        if (key.contains(Upload_File_Prefix)) {
            OssUploadParam uploadParam = readUploadParam(settings.value(key).toString());
            QString fileIdPath = QDir::fromNativeSeparators(CloudPredefine::workSpaceDir() + QDir::separator() + QString::number(uploadParam.fileInfo.fileId));
            if (uploadParam.fileInfo.filePath.contains(fileIdPath)) {
                continue; // 云同步任务
            }
            uploadTask += 1;
        }
    }

    QString configFilePath1 = CloudPredefine::workSpaceDir() + QDir::separator() + WAIT_UPLOAD_File;
    QSettings settings1(configFilePath1, QSettings::IniFormat);
    uploadTask += settings1.allKeys().size();
    emit transTaskNumber(uploadTask);
    return map;
}

bool SerializeConfig::haveTransFailTask(qint64 fileId, QString *configPath)
{
    bool haveFail = false;
    QString uploadKey,downloadKey;
    uploadKey = Upload_File_Prefix + QString::number(fileId);
    downloadKey = Download_File_Prefix + QString::number(fileId);

    QString configFilePath = CloudPredefine::workSpaceDir() + QDir::separator() + OSS_TRANSPORT_TASK_File;
    QSettings settings(configFilePath, QSettings::IniFormat);
    QStringList keyList = settings.allKeys();
    if (keyList.contains(uploadKey)) {
        *configPath = settings.value(uploadKey).toString();
        OssUploadParam uploadParam = readUploadParam(*configPath);
        if (uploadParam.fileInfo.transFail) {
            haveFail = true;
        }
    }
    if (!haveFail && keyList.contains(downloadKey)) {
        *configPath = settings.value(downloadKey).toString();
        OssDownloadParam downloadParam = readDownloadParam(*configPath);
        if (downloadParam.fileInfo.transFail && downloadParam.saveDir.isEmpty()) {
            haveFail = true;
        }
    }
    return haveFail;
}

void SerializeConfig::recordWaitUploadFile(const QString &filePath)
{
    QString configFilePath1 = CloudPredefine::workSpaceDir() + QDir::separator() + WAIT_UPLOAD_File;
    QSettings settings1(configFilePath1, QSettings::IniFormat);
    settings1.setValue(QString::number(settings1.allKeys().size()), filePath);
    settings1.sync();

    QString configFilePath = CloudPredefine::workSpaceDir() + QDir::separator() + OSS_TRANSPORT_TASK_File;
    QSettings settings(configFilePath, QSettings::IniFormat);
    int uploadTask = 0;
    for (QString key : settings.allKeys()) {
        if (key.contains(Upload_File_Prefix)) {
            OssUploadParam uploadParam = readUploadParam(settings.value(key).toString());
            QString fileIdPath = QDir::fromNativeSeparators(CloudPredefine::workSpaceDir() + QDir::separator() + QString::number(uploadParam.fileInfo.fileId));
            if (uploadParam.fileInfo.filePath.contains(fileIdPath)) {
                continue; // 云同步任务
            }
            uploadTask += 1;
        }
    }
    uploadTask += settings1.allKeys().size();
    emit transTaskNumber(uploadTask);
}

void SerializeConfig::removeWaitUploadFile(const QString &filePath)
{
    QString configFilePath1 = CloudPredefine::workSpaceDir() + QDir::separator() + WAIT_UPLOAD_File;
    QSettings settings1(configFilePath1, QSettings::IniFormat);
    for (QString key : settings1.allKeys()) {
        if (settings1.value(key).toString() == filePath) {
            settings1.remove(key);
            break;
        }
    }
    settings1.sync();

    QString configFilePath = CloudPredefine::workSpaceDir() + QDir::separator() + OSS_TRANSPORT_TASK_File;
    QSettings settings(configFilePath, QSettings::IniFormat);
    int uploadTask = 0;
    for (QString key : settings.allKeys()) {
        if (key.contains(Upload_File_Prefix)) {
            OssUploadParam uploadParam = readUploadParam(settings.value(key).toString());
            QString fileIdPath = QDir::fromNativeSeparators(CloudPredefine::workSpaceDir() + QDir::separator() + QString::number(uploadParam.fileInfo.fileId));
            if (uploadParam.fileInfo.filePath.contains(fileIdPath)) {
                continue; // 云同步任务
            }
            uploadTask += 1;
        }
    }
    uploadTask += settings1.allKeys().size();
    emit transTaskNumber(uploadTask);
}
QList<QString> SerializeConfig::loadWaitUploadFile()
{
    QString configFilePath1 = CloudPredefine::workSpaceDir() + QDir::separator() + WAIT_UPLOAD_File;
    QSettings settings1(configFilePath1, QSettings::IniFormat);
    QList<QString> waitUploadFiles;
    for (QString key : settings1.allKeys()) {
        waitUploadFiles.append(settings1.value(key).toString());
    }
    return waitUploadFiles;
}
