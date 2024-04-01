#ifndef RECYCLEBINLISTMODEL_H
#define RECYCLEBINLISTMODEL_H

#include "./src/toolsview/basemodelview/basefilemodel.h"

class RecycleBinListModel : public BaseFileModel
{
    Q_OBJECT
public:
    explicit RecycleBinListModel(QObject *parent = nullptr);
signals:

};

#endif // RECYCLEBINLISTMODEL_H
