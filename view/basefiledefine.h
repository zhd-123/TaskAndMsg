#ifndef BASEFILEDEFINE_H
#define BASEFILEDEFINE_H

#include <QObject>
#include <QDateTime>
#include <QString>

struct BaseFileDetailData{
    qint64 fileId;
    QString fileName;
    QString filePath;
    QDateTime openTime;
    QDateTime pinTime;
    int currentPageIndex;
    int totalPage;
    quint64 fileSize;
    QString imgPath; // 封面目录
    bool isStarred;
    bool isPin; // 置顶
    bool isRemoveRecent; // 是否从最近列表删除
    bool highlight;
    bool havePwd;
    BaseFileDetailData(){
        currentPageIndex = 0;
        totalPage = 0;
        fileSize = 0;
        isStarred = false;
        isPin = false;
        isRemoveRecent = false;
        highlight = false;
        havePwd = false;
        fileId = 0;
    }
    virtual ~BaseFileDetailData() {

    }
    BaseFileDetailData(qint64 fileId_,
                       const QString& fileName_,
                          const QString& filePath_,
                          const QString& imgPath_,
                          const QDateTime& openTime_,
                          const QDateTime& pinTime_,
                          int currentPageIndex_,
                          int totalPage_,
                          quint64 fileSize_,
                          bool starred,
                          bool pin,
                          bool removeRecent,
                          bool havePwd_){
        fileId = fileId_;
        fileName = fileName_;
        filePath = filePath_;
        imgPath = imgPath_;
        openTime = openTime_;
        pinTime = pinTime_;
        currentPageIndex = currentPageIndex_;
        totalPage = totalPage_;
        fileSize = fileSize_;
        isStarred = starred;
        isPin = pin;
        isRemoveRecent = removeRecent;
        havePwd = havePwd_;
        highlight = false;
    }

    bool operator==(BaseFileDetailData other)
    {
        return filePath == other.filePath;
    }
    void setHighlight(bool highlight) {
        this->highlight = highlight;
    }
    bool getHighlight()
    {
        return highlight;
    }

};
Q_DECLARE_METATYPE(BaseFileDetailData)
Q_DECLARE_METATYPE(BaseFileDetailData*)

enum TransferState {
    WaitTransfer = 0,
    Transferring,
    TransferFail,
    TransferSuccess,
};
Q_DECLARE_METATYPE(TransferState)

struct CloudFileDetailData : public BaseFileDetailData
{
    qint64 visitRecordId;
    double progress; //上传、下载进度
    TransferState transferState;
    bool fileWriteDown;// 文件写入
    CloudFileDetailData()
        : BaseFileDetailData(){
        visitRecordId = 0;
        progress = 0;
        fileWriteDown = false;
    }

    CloudFileDetailData(qint64 fileId, qint64 visitRecordId,
                        const QString& fileName_,
                        const QString& filePath_,
                        const QString& imgPath_,
                        const QDateTime& openTime_,
                        const QDateTime& pinTime_,
                        int currentPageIndex_,
                        int totalPage_,
                        quint64 fileSize_,
                        bool starred,
                        bool pin,
                        bool removeRecent,
                        bool havePwd_
                        ) : BaseFileDetailData(fileId, fileName_, filePath_, imgPath_,
                                             openTime_, pinTime_, currentPageIndex_,
                                             totalPage_, fileSize_, starred, pin, removeRecent, havePwd_) {
        this->visitRecordId = visitRecordId;
        progress = 0;
        fileWriteDown = false;
    }
    void setProgress(double progress) {
        if (transferState == TransferFail) {
            return;
        }
        if (transferState != Transferring) {
            transferState = Transferring;
        }
        if (progress >= 100) {
            transferState = TransferSuccess;
        }
        this->progress = progress;
    }
    double getProgress() {
        return progress;
    }
    void setTransFerState(TransferState state) {
        this->transferState = state;
    }
    TransferState getTransFerState() {
        return transferState;
    }
    void setFileWriteDown(bool fileWriteDown) {
        this->fileWriteDown = fileWriteDown;
    }
    bool getFileWriteDown() {
        return fileWriteDown;
    }
};
Q_DECLARE_METATYPE(CloudFileDetailData)
Q_DECLARE_METATYPE(CloudFileDetailData*)

enum SortType {
    TimeDesc   = 0,
    TimeAsc,
    FileNameAsc,
    FileNameDesc,
    FileSizeAsc,
    FileSizeDesc,
};

enum SortNameType {
    Time   = 0,
    FileName = 2,
    FileSize = 4,
    Desc   = 11,
    Asc    = 10,
};

class BaseFileDefine : public QObject
{
    Q_OBJECT
public:
    explicit BaseFileDefine(QObject *parent = nullptr);
    static BaseFileDetailData parseFileDetail(const QVariantMap &varMap);
    static bool sortNewestOpenTime(const QVariantMap &v1, const QVariantMap &v2);
    static bool sortOldestOpenTime(const QVariantMap &v1, const QVariantMap &v2);
    static bool sortNewestPinTime(const QVariantMap &v1, const QVariantMap &v2);
    static bool sortOldestPinTime(const QVariantMap &v1, const QVariantMap &v2);

    static QString formatShowStorage(quint64 size, int decimal = 2);
    static QString formatPassTimeStr(const QDateTime &dateTime);
signals:

};

#endif // BASEFILEDEFINE_H
