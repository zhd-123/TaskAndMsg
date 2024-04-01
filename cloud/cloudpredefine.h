#ifndef CLOUDPREDEFINE_H
#define CLOUDPREDEFINE_H

#include <QObject>
#include <QString>
#include <QDateTime>
#include <QList>
#include <QVector>

#define Cloud_Work_Dir "cloud"
#define SaveAs_Config_File "saveAsFileConfig.ini" // 另存成功的记录
#define TransDone_Config_File "transDoneFileConfig.ini" // 已传输完的文件配置
#define OSS_UPLOAD_Config_File "uploadConfig.ini"
#define OSS_DOWNLOAD_Config_File "downloadConfig.ini"
#define OSS_TRANSPORT_TASK_File "transport.ini"
#define WAIT_UPLOAD_File "waitUploadFile.ini"

#define Upload_File_Prefix "upload_file"
#define Download_File_Prefix "download_file"
#define Upload_Thumbnail_Prefix "upload_thumbnail"
#define Download_Thumbnail_Prefix "upload_thumbnail"

#define Save_Upload_Delay 1000*60*1
#define Check_Version_Time 1000*60*10

#define Upload_MaxParallel_Task 5
#define Download_MaxParallel_Task 5

#define PDF_FILE_SUFFIX ".pdf"
#define PDF_FILE_UPSUFFIX ".PDF"
enum CatelogueType {
    CloudDocument = 1,
    CloudSpace = 2,
    RecycleBin = -1,
};

enum GetInfoNextStep {
    Resume_Upload = 0,
    Resume_Download,
    Upload,
    Download,
    CheckVersion,
};
Q_DECLARE_METATYPE(GetInfoNextStep)

struct FilePropInfo {
    QString path;
    QString name;
    QString hash;
    QString suffix;
    QDateTime createTime;
    QDateTime modifyTime;
    QDateTime visitTime;
    quint64 size;
    qint64 fileId;
    FilePropInfo() {
        size = 0;
        fileId = 0;
    }
};
Q_DECLARE_METATYPE(FilePropInfo)

struct CloudFilePropInfo {
    qint64 fileId;
    qint64 fileVersion; // 文件版本，递增的
    quint64 fileCrc;    // 用于oss验证传输正确
    QString fileHash;   // 用于比较文件是否修改
    int currentPageIndex; // 当前页索引
    double scale; // 页面缩放
    int totalPage; // 总页数
    quint64 fileSize;
    QString filePath;
    QString fileShowName;
    QString logoPath;
    QString logoName;
    QString fileRemotePath; // 相对目录
    QString logoRemotePath;  
    QString fileUrl; // 完整资源地址
    QDateTime urlCurrentTime;
    QDateTime urlExpiration;
    QString logoUrl;
    bool isStarred;
    bool isShared;
    bool inRecycle;
    QDateTime createTime;
    QDateTime updateTime;
    QDateTime recycleExpireTime;
    QDateTime uploadTime;
    QString lastVisit; // 文件最后一次访问时，文件的状态
    QDateTime lastVisitTime;
    QString transPrefix; // 传输前缀
    qint64 parentDirId;
    CatelogueType catelogue;
    bool transFail; // 传输失败
    bool havePwd;
    CloudFilePropInfo() {
        fileId = 0;
        parentDirId = 0;
        fileVersion = 0;
        currentPageIndex = 0;
        scale = 0;
        totalPage = 0;
        fileSize = 0;
        fileCrc = 0;
        isStarred = false;
        isShared = false;
        inRecycle = false;
        transFail = false;
        havePwd = false;

        catelogue = CloudSpace;
    }
};
Q_DECLARE_METATYPE(CloudFilePropInfo)


typedef QList<CloudFilePropInfo> CloudFilePropInfos;
Q_DECLARE_METATYPE(CloudFilePropInfos)

struct CredentialsInfo {
    QString securityToken;
    QString accessKeyId;
    QDateTime currentTime; // 当前时间(换算到本地）
    QDateTime expiration; // oss的密钥过期时间(换算到本地）
    QString accessKeySecret;
    QString endpoint;
    QString bucketName;
    CredentialsInfo() {
    }
};


struct OssUploadPartParam {
    QString eTag;
    quint64 startPos;
    quint64 endPos;
    int partIndex; // 0~SEGMENT_COUNT-1
    OssUploadPartParam() {
        startPos = 0;
        endPos = 0;
        partIndex = 0;
    }
};

struct OssUploadParam {
    QString uploadId;
    quint64 uploadSize;
    int partTotalNum;
    QVector<OssUploadPartParam> partArray;
    CloudFilePropInfo fileInfo;
    CredentialsInfo credentials;
    OssUploadParam() {
        uploadSize = 0;
        partTotalNum = 0;
    }
};
Q_DECLARE_METATYPE(OssUploadParam)


struct OssDownloadPartParam {
    quint64 startPos;
    quint64 endPos;
    int partIndex; // 0~SEGMENT_COUNT-1
    OssDownloadPartParam() {
        startPos = 0;
        endPos = 0;
        partIndex = 0;
    }
};

struct OssDownloadParam {
    quint64 downloadSize;
    CloudFilePropInfo fileInfo;
    QString saveDir;// 另存目录
    QString saveTempName;
    QVector<OssDownloadPartParam> partArray;
    int partTotalNum;
    OssDownloadParam() {
        downloadSize = 0;
        partTotalNum = 0;
    }
};
Q_DECLARE_METATYPE(OssDownloadParam)

struct LastVisitParam {
    qint64 fileId;
    QDateTime lastVisitTime;
    double scale;
    int pageIndex;
    int scaleType;
    LastVisitParam() {
        fileId = 0;
        scale = 0;
        pageIndex = 0;
        scaleType = 0;
    }
};
Q_DECLARE_METATYPE(LastVisitParam)

struct LastVisitRecordInfo {
    qint64 visitRecordId;
    CloudFilePropInfo fileInfo;
    LastVisitRecordInfo() {
        visitRecordId = 0;
    }
};
Q_DECLARE_METATYPE(LastVisitRecordInfo)

typedef QList<LastVisitRecordInfo> LastVisitRecordInfos;
Q_DECLARE_METATYPE(LastVisitRecordInfos)

struct CloudDiskInfo {
    quint64 usedSize;
    quint64 totalSize;

};
Q_DECLARE_METATYPE(CloudDiskInfo)

struct RenameStruct {
    qint64 fileId;
    QString newName;
    RenameStruct() {
        fileId = 0;
    }
};
Q_DECLARE_METATYPE(RenameStruct)

struct DeleteOprStruct {
    QList<qint64> fileIds;
    QList<qint64> dirIds;
    bool clearAll;// 清空回收站
    DeleteOprStruct() {
        clearAll = false;
    }
};
Q_DECLARE_METATYPE(DeleteOprStruct)


class CloudPredefine : public QObject
{
    Q_OBJECT
public:
    explicit CloudPredefine(QObject *parent = nullptr);
    static QString workSpaceDir();
    static QString getUploadConfigFile(qint64 fileId);
    static QString getDownloadConfigFile(qint64 fileId);

    static LastVisitParam parseLastVisit(const QString &jsonStr);
    static QString createLastVisit(const LastVisitParam &lastVisitParam);

signals:

};

#endif // CLOUDPREDEFINE_H
