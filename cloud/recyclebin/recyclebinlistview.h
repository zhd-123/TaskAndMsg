#ifndef RECYCLEBINLISTVIEW_H
#define RECYCLEBINLISTVIEW_H

#include "./src/toolsview/basemodelview/basefileview.h"
#include "./src/toolsview/basemodelview/basefilemodel.h"
#include "./src/toolsview/basemodelview/basefileitemdelegate.h"


class RecycleBinListModel;
class RecycleBinItemDelegate;
class RecycleBinListView : public BaseFileView
{
    Q_OBJECT
public:
    explicit RecycleBinListView(QWidget *parent = nullptr);
    void initView();
    RecycleBinListModel* getModel();
signals:
private:
    RecycleBinListModel* mListModel;
    RecycleBinItemDelegate* mItemDelegate;
};

class RecycleBinListModel : public BaseFileModel
{
    Q_OBJECT
public:
    explicit RecycleBinListModel(QObject *parent = nullptr);
signals:

};
class RecycleBinItemDelegate : public BaseFileItemDelegate
{
    Q_OBJECT
public:
    explicit RecycleBinItemDelegate(BaseFileView* listView,QObject *parent = nullptr);

signals:

};
#endif // RECYCLEBINLISTVIEW_H
