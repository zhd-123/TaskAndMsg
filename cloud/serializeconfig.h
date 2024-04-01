#ifndef SERIALIZECONFIG_H
#define SERIALIZECONFIG_H

#include <QObject>
#include "../util/singleton.h"
#include "cloudpredefine.h"


class SerializeConfig : public QObject
{
    Q_OBJECT
    DECLARE_SINGLETON(SerializeConfig)
public:
    explicit SerializeConfig(QObject *parent = nullptr);
    CloudFilePropInfos loadSaveAsRecord();
    void addSaveAsRecord(const CloudFilePropInfo &propInfo);
    void removeSaveAsRecord(qint64 fileId, const QString &filePath);
    void clearSaveAsRecord();

    CloudFilePropInfos loadTransDoneFileConfig();
    CloudFilePropInfo checkTransDoneFileInfo(const QString &fileId, const QString &prefix);
    void updateTransDoneFileConfig(const CloudFilePropInfo &propInfo, const QString &prefix);
    void removeTransDoneFileConfig(const QString &fileId, const QString &prefix);
    void clearTransDoneFileConfig();

    // 断点续传配置
    OssUploadParam readUploadParam(const QString &configFile);
    void writeUploadParam(const QString &configFile, const OssUploadParam &param);
    OssDownloadParam readDownloadParam(const QString &configFile);
    void writeDownloadParam(const QString &configFile, const OssDownloadParam &param);

    // 记录所有未完成任务
    void recordTransportTask(qint64 fileId, bool upload);
    void removeTransportTask(qint64 fileId, bool upload);
    QMap<QString, QString> loadPauseTask();
    bool haveTransFailTask(qint64 fileId, QString *configPath);
    // 待上传文件
    void recordWaitUploadFile(const QString &filePath);
    void removeWaitUploadFile(const QString &filePath);
    QList<QString> loadWaitUploadFile();
signals:
    void transTaskNumber(int task);
};

#endif // SERIALIZECONFIG_H
