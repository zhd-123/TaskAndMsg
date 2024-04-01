#include "fileoperationtool.h"
#include <QDir>
#include <QFileInfo>
#include <QCryptographicHash>
#include <QFile>
#include <QtConcurrent>

FileOperationTool::FileOperationTool(QObject *parent)
    : QObject{parent}
{

}

QString FileOperationTool::getFileHash(const QString &filePath)
{
    QCryptographicHash hash(QCryptographicHash::Md5);

    QFile file(filePath);
    if (!file.exists() || !file.open(QIODevice::ReadOnly)) {
        return "";
    }

    hash.addData(file.readAll());
    file.close();
    return hash.result().toHex();
}

QString FileOperationTool::getContentHash(const QByteArray &data)
{
    QCryptographicHash hash(QCryptographicHash::Md5);
    hash.addData(data);
    return hash.result().toHex();
}
