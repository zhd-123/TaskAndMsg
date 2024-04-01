#ifndef RECYCLEBINITEMDELEGATE_H
#define RECYCLEBINITEMDELEGATE_H

#include <QObject>
#include "./src/toolsview/basemodelview/basefileitemdelegate.h"

class RecycleBinItemDelegate : public BaseFileItemDelegate
{
    Q_OBJECT
public:
    explicit RecycleBinItemDelegate(BaseFileView* listView,QObject *parent = nullptr);

signals:

};

#endif // RECYCLEBINITEMDELEGATE_H
