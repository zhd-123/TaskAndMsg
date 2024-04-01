#ifndef CLOUDLISTMODEL_H
#define CLOUDLISTMODEL_H

#include <QObject>
#include "./src/toolsview/basemodelview/basefilemodel.h"

class CloudListModel : public BaseFileModel
{
    Q_OBJECT
public:
    explicit CloudListModel(QObject *parent = nullptr);
signals:

};

#endif // CLOUDLISTMODEL_H
