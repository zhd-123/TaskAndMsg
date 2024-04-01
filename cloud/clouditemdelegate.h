#ifndef CLOUDITEMDELEGATE_H
#define CLOUDITEMDELEGATE_H

#include <QObject>
#include "./src/toolsview/basemodelview/basefileitemdelegate.h"


class CloudListView;
class CloudItemDelegate : public BaseFileItemDelegate
{
    Q_OBJECT
public:
    explicit CloudItemDelegate(BaseFileView* listView,QObject *parent = nullptr);

signals:

};

#endif // CLOUDITEMDELEGATE_H
