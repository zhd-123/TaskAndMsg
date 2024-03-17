#ifndef DOWNLOADCONFIG_H
#define DOWNLOADCONFIG_H

#include <QObject>
#include "util_global.h"

#define EACH_SEGMENT_SIZE 10485760
#define MAX_RETRY_TIMES 3

struct SegmentParam {
    qint64 startPos;
    qint64 endPos;
    int index; // 0~SEGMENT_COUNT-1
    SegmentParam() {
        startPos = 0;
        endPos = 0;
        index = 0;
    }
};

struct ConfigParam {
    quint64 crc;
    qint64 totalSize;
    qint64 downloadSize;
    QString url;
    int segmentCount;
    QVector<SegmentParam> segmentArray;
    ConfigParam() {
        crc = 0;
        totalSize = 0;
        downloadSize = 0;
        segmentCount = 0;
    }
};

quint32 UTIL_EXPORT getCrc32(const QByteArray &data);
uint64_t  UTIL_EXPORT CalcCRC64(uint64_t crc, void *buf, size_t len, bool little = true);


class UTIL_EXPORT DownloadConfig : public QObject
{
    Q_OBJECT
public:
    static DownloadConfig* instance();
    void setConfigFile(const QString &filePath);
    ConfigParam readDownloadConfig();
    void writeDownloadConfig(const ConfigParam &configParam);
private:
    explicit DownloadConfig(QObject *parent = nullptr);
signals:
private:
    QString mConfigFilePath;
    ConfigParam mConfigParam;
};

#endif // DOWNLOADCONFIG_H
