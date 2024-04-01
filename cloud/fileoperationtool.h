#ifndef FILEOPERATIONTOOL_H
#define FILEOPERATIONTOOL_H

#include <QObject>
#include "cloudpredefine.h"


class FileOperationTool : public QObject
{
    Q_OBJECT
public:
    explicit FileOperationTool(QObject *parent = nullptr);
    static QString getFileHash(const QString &filePath);
    static QString getContentHash(const QByteArray &data);

signals:

};

#endif // FILEOPERATIONTOOL_H
