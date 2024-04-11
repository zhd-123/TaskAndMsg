#include "basefiledefine.h"
#include <QVariantMap>
#include "./src/util/guiutil.h"
#include "./src/util/configutil.h"

BaseFileDefine::BaseFileDefine(QObject *parent)
    : QObject{parent}
{

}

BaseFileDetailData BaseFileDefine::parseFileDetail(const QVariantMap &varMap)
{
    qint64 fileId = varMap["fileId"].toString().toULongLong();
    const QString filePath = varMap["path"].toString();
    const QString fileName = GuiUtil::getFileName(filePath);
    const QDateTime openTime = QDateTime::fromTime_t(varMap["opentime"].toUInt());
    const QDateTime pinTime = QDateTime::fromTime_t(varMap["pintime"].toUInt());
    int currentPageIndex = ConfigureUtil::getHisPageIndex(fileName);
    int totalPage = varMap["count"].toInt();
    const QString imgPath = varMap["imgPath"].toString();
    const bool starred = varMap["starred"].toBool();
    const bool pin = varMap["pin"].toBool();
    const bool removeRecent = varMap["removeRecent"].toBool();
    quint64 fileSize = varMap["fileSize"].toULongLong();
    bool havePwd = varMap["havePwd"].toString() == "1";
    return BaseFileDetailData(fileId, fileName, filePath, imgPath, openTime, pinTime, currentPageIndex, totalPage,
                              fileSize, starred, pin, removeRecent, havePwd);
}

bool BaseFileDefine::sortNewestOpenTime(const QVariantMap &v1, const QVariantMap &v2)
{
    return v1["opentime"].toUInt() > v2["opentime"].toUInt();
}

bool BaseFileDefine::sortOldestOpenTime(const QVariantMap &v1, const QVariantMap &v2)
{
    return v1["opentime"].toUInt() < v2["opentime"].toUInt();
}

bool BaseFileDefine::sortNewestPinTime(const QVariantMap &v1, const QVariantMap &v2)
{
    return v1["pintime"].toUInt() > v2["pintime"].toUInt();
}

bool BaseFileDefine::sortOldestPinTime(const QVariantMap &v1, const QVariantMap &v2)
{
    return v1["pintime"].toUInt() < v2["pintime"].toUInt();
}

QString BaseFileDefine::formatShowStorage(quint64 size, int decimal)
{
    double gbSize = size / (1024 * 1024 * 1024 * 1.0);
    double mbSize = size / (1024 * 1024 * 1.0);
    double kbSize = size / (1024 * 1.0);
    if (gbSize >= 1.0) {
        return QString::number(gbSize, 'f', decimal) + " GB";
    } else if (mbSize >= 1.0) {
        return QString::number(mbSize, 'f', decimal) + " MB";
    } else if (kbSize >= 1.0) {
        return QString::number(kbSize, 'f', decimal) + " KB";
    } else {
        return QString::number(size, 'f', decimal) + " KB";
    }
}

QString BaseFileDefine::formatPassTimeStr(const QDateTime &dateTime)
{
    QDateTime currentTime = QDateTime::currentDateTime();
    if (dateTime == QDateTime::fromSecsSinceEpoch(0)) {
        return "";
    }
    int passDays = dateTime.daysTo(currentTime);
    int passSeconds = dateTime.secsTo(currentTime);
    if (passDays >= 365) {
        // year
        return QString::number(passDays / 365) + " " + tr("years") + " " + tr("ago");
    } else if (passDays >= 30) {
        // month
        return QString::number(passDays / 30) + " " + tr("months") + " " + tr("ago");
    } else if (passDays > 0) {
        // day
        return QString::number(passDays) + " " + tr("days") + " " + tr("ago");
    } else if (passSeconds >= 3600) {
        // hour
        return QString::number(passSeconds / 3600) + " " + tr("hours") + " " + tr("ago");
    } else if (passSeconds >= 60) {
        // minute
        return QString::number(passSeconds / 60) + " " + tr("minutes") + " " + tr("ago");
    } else if (passSeconds > 0) {
        // second
        return QString::number(passSeconds) + " " + tr("seconds") + " " + tr("ago");
    }
    return "";
}
