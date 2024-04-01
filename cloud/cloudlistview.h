#ifndef CLOUDLISTVIEW_H
#define CLOUDLISTVIEW_H

#include <QObject>
#include "./src/toolsview/basemodelview/basefileview.h"
#include "./src/basicwidget/basicitemview.h"
#include "./src/toolsview/basemodelview/basefilemodel.h"
#include "./src/toolsview/basemodelview/basefileitemdelegate.h"


class CloudListModel;
class CloudItemDelegate;
class CloudListView : public BaseFileView
{
    Q_OBJECT
public:
    explicit CloudListView(QWidget *parent = nullptr);
    void initView();
    CloudListModel* getModel();
signals:
private:
    CloudListModel* mListModel;
    CloudItemDelegate* mItemDelegate;
};

class CloudListModel : public BaseFileModel
{
    Q_OBJECT
public:
    explicit CloudListModel(QObject *parent = nullptr);
signals:

};

class CloudItemDelegate : public BaseFileItemDelegate
{
    Q_OBJECT
public:
    explicit CloudItemDelegate(BaseFileView* listView,QObject *parent = nullptr);

signals:

};

class CloudTransItemDelegate;
class CloudTransListView : public BaseFileView
{
    Q_OBJECT
public:
    explicit CloudTransListView(QWidget *parent = nullptr);
    void initView();
    CloudListModel* getModel();
signals:
private:
    CloudListModel* mListModel;
    CloudTransItemDelegate* mItemDelegate;
};

class CloudTransItemDelegate : public BaseFileItemDelegate
{
    Q_OBJECT
public:
    explicit CloudTransItemDelegate(BaseFileView* listView,QObject *parent = nullptr);
    virtual bool editorEvent(QEvent *event, QAbstractItemModel *model, const QStyleOptionViewItem &option, const QModelIndex &index);
    virtual void drawIconExpand(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const;
    virtual void drawListExpand(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const;

signals:

};

#endif // CLOUDLISTVIEW_H
