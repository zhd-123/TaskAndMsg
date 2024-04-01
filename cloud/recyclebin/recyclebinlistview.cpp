#include "recyclebinlistview.h"

RecycleBinListView::RecycleBinListView(QWidget *parent)
    : BaseFileView{parent}
{
    setObjectName("RecycleBinListView");
    initView();
}

void RecycleBinListView::initView()
{
    mListModel = new RecycleBinListModel(this);
    mItemDelegate = new RecycleBinItemDelegate(this, this);
    connect(this, &RecycleBinListView::updateItem, mListModel, &RecycleBinListModel::onItemUpdate);
    connect(mItemDelegate,&RecycleBinItemDelegate::itemClick,this,&RecycleBinListView::itemClick);
    connect(mItemDelegate,&RecycleBinItemDelegate::popMenu,this,&RecycleBinListView::popMenu);
    connect(mItemDelegate,&RecycleBinItemDelegate::updateRow, mListModel,&RecycleBinListModel::onItemUpdate);

    setItemDelegate(mItemDelegate);
    setModel(mListModel);
}

RecycleBinListModel* RecycleBinListView::getModel()
{
    return mListModel;
}

RecycleBinListModel::RecycleBinListModel(QObject *parent)
    : BaseFileModel{parent}
{

}

RecycleBinItemDelegate::RecycleBinItemDelegate(BaseFileView* listView,QObject *parent)
    : BaseFileItemDelegate{listView,parent}
{

}
