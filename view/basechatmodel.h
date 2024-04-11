#ifndef BASECHATMODEL_H
#define BASECHATMODEL_H

#include <QObject>
#include <QListView>
#include <QAbstractItemModel>
#include <QStyledItemDelegate>
#include "../chatgpttypedefine.h"
#include "./src/basicwidget/basicitemview.h"

template <typename T>
class ListModelData : public QAbstractListModel
{
public:
    explicit ListModelData(QObject *parent = nullptr);
    void addItemCell(const T &cellData);
    void insertItemCell(int index, const T &cellData);
    void updateItemCell(int row, const T &itemData);
    void updateLastCell(const T &itemData);
    void updateAnimationCell();
    void removeAnimationCell();
    void removeFetchCell();
    void removeItemCell(int row);
    int indexAnimationCell();
    int modelSize();

    T getItemCell(int row);
    void clearItemCells();
    QModelIndex currentIndexByRow(int row);
    void repaintItemCell(int row);
    void repaintView();
private:
    QVariant data(const QModelIndex &index, int role) const;
    int rowCount(const QModelIndex &parent = QModelIndex()) const;
    int columnCount(const QModelIndex &parent = QModelIndex()) const;
    Qt::ItemFlags flags(const QModelIndex &index) const;
protected:
    QList<T> mModelList;
};
template<typename T>
ListModelData<T>::ListModelData(QObject *parent)
 : QAbstractListModel(parent)
{

}

template<typename T>
void ListModelData<T>::addItemCell(const T &cellData)
{
    beginInsertRows(QModelIndex(), mModelList.count(), mModelList.count());
    mModelList.append(cellData);
    endInsertRows();
}
template<typename T>
void ListModelData<T>::insertItemCell(int index, const T &cellData)
{
    beginInsertRows(QModelIndex(), index, index);
    mModelList.insert(index, cellData);
    endInsertRows();
}
template<typename T>
void ListModelData<T>::removeItemCell(int row)
{
    if (row == -1) {
        return;
    }
    beginRemoveRows(QModelIndex(), row, row);
    mModelList.removeAt(row);
    endRemoveRows();
}
template<typename T>
int ListModelData<T>::indexAnimationCell()
{
    int index = -1;
    for (int row = mModelList.count() - 1; row >= 0; row--) {
        T itemData = getItemCell(row);
        if (itemData.waitingAnimation) {
            index = row;
            break;
        }
    }
    return index;
}
template<typename T>
int ListModelData<T>::modelSize()
{
    return mModelList.count();
}
template<typename T>
QModelIndex ListModelData<T>::currentIndexByRow(int row)
{
    return createIndex(row, 0);
}
template<typename T>
void ListModelData<T>::repaintItemCell(int row)
{
    emit dataChanged(createIndex(row, 0), createIndex(row, columnCount()));
}
template<typename T>
void ListModelData<T>::repaintView()
{
    for (int row = 0; row < mModelList.count(); ++row) {
        repaintItemCell(row);
    }
}
template<typename T>
void ListModelData<T>::updateItemCell(int row, const T &itemData)
{
    if (row == -1) {
        return;
    }
    mModelList.replace(row, itemData);
    repaintItemCell(row);
}
template<typename T>
void ListModelData<T>::updateLastCell(const T &itemData)
{
    updateItemCell(mModelList.count() - 1, itemData);
}
template<typename T>
void ListModelData<T>::updateAnimationCell()
{
    for (int row = mModelList.count() - 1; row >= 0; row--) {
        T itemData = getItemCell(row);
        if (itemData.waitingAnimation) {
            repaintItemCell(row);
            break;
        }
    }
}
template<typename T>
void ListModelData<T>::removeAnimationCell()
{
    for (int row = mModelList.count() - 1; row >= 0; row--) {
        T itemData = getItemCell(row);
        if (itemData.waitingAnimation) {
            removeItemCell(row);
            break;
        }
    }
}
template<typename T>
void ListModelData<T>::removeFetchCell()
{
    for (int row = 0; row < mModelList.count(); row++) {
        T itemData = getItemCell(row);
        if (itemData.fetchData) {
            removeItemCell(row);
            break;
        }
    }
}
template<typename T>
T ListModelData<T>::getItemCell(int row)
{
    if (mModelList.size() <= row) {
        return T();
    }
    return mModelList.at(row);
}
template<typename T>
void ListModelData<T>::clearItemCells()
{
    beginResetModel();
    for (T modelData : mModelList)
    {

    }
    mModelList.clear();
    endResetModel();
}
template<typename T>
QVariant ListModelData<T>::data(const QModelIndex &index, int role) const
{
    if (!index.isValid()) {
        return QVariant();
    }
    if (mModelList.size() <= index.row()) {
        return QVariant();
    }
    QVariant variant;
    if (Qt::UserRole == role) {
        variant.setValue(mModelList.at(index.row()));
    }
    return variant;
}
template<typename T>
int ListModelData<T>::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent)
    return mModelList.size();
}
template<typename T>
int ListModelData<T>::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent)
    return 1;
}
template<typename T>
Qt::ItemFlags ListModelData<T>::flags(const QModelIndex &index) const
{
     if (index.isValid())
         return Qt::ItemIsEditable | QAbstractListModel::flags(index);
     return QAbstractListModel::flags(index);
}

///////////////////////////////////////
template <typename T, typename Model, typename Delegate>
class ListViewT : public AbstractItemViewType<QListView>
{
public:
    explicit ListViewT(QWidget *parent = nullptr);
    void addItemCell(const T &itemData);
    void insertItemCell(int row, const T &itemData);
    void updateItemCell(int row, const T &itemData);
    void removeItemCell(int row);
    void updateLastItem(const T &itemData);
    void updateAnimationItem();
    void removeAnimationItem();
    void removeFetchItem();
    int indexAnimationItem();
    int modelSize();
    void clearItems();
    T getItemCell(int row);
    void repaintChatItem(int row);
protected:
    virtual bool eventFilter(QObject *watched, QEvent *event) override;
    virtual void resizeEvent(QResizeEvent *event) override;
protected:
    Model* mListModel;
    Delegate* mItemDelegate;
    QLabel* mTopGradientLabel;
    QLabel* mBottomGradientLabel;
};
template <typename T, typename Model, typename Delegate>
ListViewT<T, Model, Delegate>::ListViewT(QWidget *parent)
    : AbstractItemViewType<QListView>{parent}
{
    setMouseTracking(true);
    setAttribute(Qt::WA_Hover, true);
    setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setResizeMode(QListView::Adjust);
    setAutoScroll(false);

    setSpacing(ChatItemSpacing);
    setViewMode(QListView::ListMode);
    setFlow(QListView::TopToBottom);
    setLayoutMode(QListView::SinglePass);
    setUniformItemSizes(false);

    mListModel = new Model(this);
    mItemDelegate = new Delegate(this);
    connect(mItemDelegate, &Delegate::refreshItem, this, [=](int row) { repaintChatItem(row);});
    connect(mItemDelegate, &Delegate::triggerEditor, this, [=](const QPoint &pos) {
        mouseDoubleClickEvent(new QMouseEvent(QEvent::MouseButtonDblClick, pos, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier));
    });
    setItemDelegate(mItemDelegate);
    setModel(mListModel);

    mTopGradientLabel = new QLabel(this);
    mTopGradientLabel->setObjectName("topGradient");
    mBottomGradientLabel = new QLabel(this);
    mBottomGradientLabel->setObjectName("bottomGradient");
    installEventFilter(this);
}
template <typename T, typename Model, typename Delegate>
void ListViewT<T, Model, Delegate>::addItemCell(const T &itemData)
{
    mListModel->addItemCell(itemData);
}
template <typename T, typename Model, typename Delegate>
void ListViewT<T, Model, Delegate>::insertItemCell(int row, const T &itemData)
{
    mListModel->insertItemCell(row, itemData);
}
template <typename T, typename Model, typename Delegate>
void ListViewT<T, Model, Delegate>::updateItemCell(int row, const T &itemData)
{
    mListModel->updateItemCell(row, itemData);
}
template <typename T, typename Model, typename Delegate>
void ListViewT<T, Model, Delegate>::removeItemCell(int row)
{
    mListModel->removeItemCell(row);
}
template <typename T, typename Model, typename Delegate>
void ListViewT<T, Model, Delegate>::updateLastItem(const T &itemData)
{
    mListModel->updateLastCell(itemData);
}
template <typename T, typename Model, typename Delegate>
void ListViewT<T, Model, Delegate>::updateAnimationItem()
{
    mListModel->updateAnimationCell();
}
template <typename T, typename Model, typename Delegate>
void ListViewT<T, Model, Delegate>::removeAnimationItem()
{
    mListModel->removeAnimationCell();
}
template <typename T, typename Model, typename Delegate>
void ListViewT<T, Model, Delegate>::removeFetchItem()
{
    mListModel->removeFetchCell();
}
template <typename T, typename Model, typename Delegate>
int ListViewT<T, Model, Delegate>::indexAnimationItem()
{
    return mListModel->indexAnimationCell();
}
template <typename T, typename Model, typename Delegate>
int ListViewT<T, Model, Delegate>::modelSize()
{
    return mListModel->modelSize();
}
template <typename T, typename Model, typename Delegate>
void ListViewT<T, Model, Delegate>::clearItems()
{
    mListModel->clearItemCells();
}
template <typename T, typename Model, typename Delegate>
T ListViewT<T, Model, Delegate>::getItemCell(int row)
{
    return mListModel->getItemCell(row);
}
template <typename T, typename Model, typename Delegate>
void ListViewT<T, Model, Delegate>::repaintChatItem(int row)
{
    mListModel->repaintItemCell(row);
}

template <typename T, typename Model, typename Delegate>
bool ListViewT<T, Model, Delegate>::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == this) {
        switch (event->type()) {
        case QEvent::Leave:
            mItemDelegate->hideTip();
            break;
        default:
            break;
        }
    }
    return QListView::eventFilter(watched,event);
}
template <typename T, typename Model, typename Delegate>
void ListViewT<T, Model, Delegate>::resizeEvent(QResizeEvent *event)
{
    QListView::resizeEvent(event);
    mListModel->repaintView();

    mTopGradientLabel->setGeometry(0, 0, width(), 4);
    mBottomGradientLabel->setGeometry(0, height() - 4, width(), 4);
}

class AIToolsResultModel : public ListModelData<RspGptStruct>
{
    Q_OBJECT
public:
    explicit AIToolsResultModel(QObject *parent = nullptr);
};


class ChatPDFResultModel : public ListModelData<GptRspPDFStruct>
{
    Q_OBJECT
public:
    explicit ChatPDFResultModel(QObject *parent = nullptr);
};


class TranslateResultModel : public ListModelData<RspTranslateStruct>
{
    Q_OBJECT
public:
    explicit TranslateResultModel(QObject *parent = nullptr);
};
#endif // BASECHATMODEL_H
