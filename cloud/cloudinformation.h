#ifndef CLOUDINFORMATION_H
#define CLOUDINFORMATION_H

#include <QObject>

class CloudInformation : public QObject
{
    Q_OBJECT
public:
    explicit CloudInformation(QObject *parent = nullptr);

signals:

};

#endif // CLOUDINFORMATION_H
