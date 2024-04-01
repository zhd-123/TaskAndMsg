#include "cloudlistview.h"
#include <QDateTime>
#include <QTimer>
#include <QVBoxLayout>
#include <QPainter>
#include <QDateTime>
#include <QApplication>
#include <QPainterPath>
#include <QMouseEvent>
#include <QFileInfo>

CloudListView::CloudListView(QWidget *parent)
    : BaseFileView{parent}
{
    setObjectName("CloudListView"); //qss中去掉默认样式
    initView();
}

void CloudListView::initView()
{
    mListModel = new CloudListModel(this);
    mItemDelegate = new CloudItemDelegate(this, this);
    connect(this, &CloudListView::updateItem, mListModel, &CloudListModel::onItemUpdate);
    connect(mItemDelegate,&CloudItemDelegate::itemClick,this,&CloudListView::itemClick);
    connect(mItemDelegate,&CloudItemDelegate::popMenu,this,&CloudListView::popMenu);
    connect(mItemDelegate,&CloudItemDelegate::updateRow, mListModel,&CloudListModel::onItemUpdate);

    setItemDelegate(mItemDelegate);
    setModel(mListModel);
}

CloudListModel* CloudListView::getModel()
{
    return mListModel;
}

CloudListModel::CloudListModel(QObject *parent)
    : BaseFileModel{parent}
{

}

CloudItemDelegate::CloudItemDelegate(BaseFileView* listView,QObject *parent)
    : BaseFileItemDelegate{listView, parent}
{

}


CloudTransListView::CloudTransListView(QWidget *parent)
    : BaseFileView{parent}
{
    setObjectName("CloudTransListView"); //qss中去掉默认样式
    initView();
}

void CloudTransListView::initView()
{
    mListModel = new CloudListModel(this);
    mItemDelegate = new CloudTransItemDelegate(this, this);
    connect(this, &CloudTransListView::updateItem, mListModel, &CloudListModel::onItemUpdate);
    connect(mItemDelegate,&CloudTransItemDelegate::itemClick,this,&CloudTransListView::itemClick);
    connect(mItemDelegate,&CloudTransItemDelegate::popMenu,this,&CloudTransListView::popMenu);
    connect(mItemDelegate,&CloudTransItemDelegate::updateRow, mListModel,&CloudListModel::onItemUpdate);

    setItemDelegate(mItemDelegate);
    setModel(mListModel);
}

CloudListModel* CloudTransListView::getModel()
{
    return mListModel;
}

CloudTransItemDelegate::CloudTransItemDelegate(BaseFileView* listView,QObject *parent)
    : BaseFileItemDelegate{listView, parent}
{

}

bool CloudTransItemDelegate::editorEvent(QEvent *event, QAbstractItemModel *model, const QStyleOptionViewItem &option, const QModelIndex &index)
{
    QMouseEvent *pEvent = static_cast<QMouseEvent *>(event);
    auto paintRect = option.rect;
    auto menuRectBtn =  mListView->viewMode() == QListView::ListMode ?  QRect(paintRect.width() - 24 - 20 - 6 + paintRect.x(),30 + paintRect.y(),20,20) :
                                                                           QRect(paintRect.width() - 30 + paintRect.x() - 10,paintRect.height() - 30 + paintRect.y() -10,20,20); //(-4)margin-left:4px
    QVariant userData = index.data(Qt::UserRole);
    BaseFileDetailData *modelData = userData.value<BaseFileDetailData*>();
    CloudFileDetailData *cloudData = dynamic_cast<CloudFileDetailData *>(modelData);
    int row = index.row();
    mMenuBtnState[row] = MouseState::Normal;
    if (menuRectBtn.contains(pEvent->pos())) {
        if(event->type() == QEvent::MouseButtonRelease){
            if(pEvent->button() == Qt::LeftButton || pEvent->button() == Qt::RightButton){
                emit popMenu(modelData);
            }
        } else if (event->type() == QEvent::MouseButtonPress) {
            mMenuBtnState[row] = MouseState::Pressed;
        } else if (event->type() == QEvent::MouseMove) {
            mMenuBtnState[row] = MouseState::Hover;
        }
        emit updateRow(row);
    } else {
        if(event->type() == QEvent::MouseButtonRelease){
            if(pEvent->button() == Qt::LeftButton){
                emit itemClick(modelData);
            } else if(pEvent->button() == Qt::RightButton){
                emit popMenu(modelData);
            }
        } else if (event->type() == QEvent::MouseMove) {
            auto win = const_cast<QWidget*>(option.widget);
            int maxToolTipLen = mListView->viewMode() == QListView::ListMode ? 350 : 220;
            QRect textRect = paintRect.adjusted(8, 8, -8, -8);
            QFontMetrics fontWidth(mNormalFont);
            if(fontWidth.width(modelData->fileName) > maxToolTipLen && textRect.contains(pEvent->pos())){
                win->setToolTip(modelData->fileName);
            } else {
                win->setToolTip("");
            }
            QRect paintRect, iconRect;
            if (mListView->viewMode() == QListView::ListMode) {
                paintRect = option.rect.adjusted(0,0,-0,-0);
                iconRect = QRect(paintRect.width() - 28 - 56 + paintRect.x(),42 + paintRect.y(),16,16);
            } else {
                paintRect = option.rect.adjusted(10,10,-10,-10);
                iconRect = QRect(paintRect.width() + paintRect.x() - 28, paintRect.y() + 8,16,16);
            }
            if (iconRect.contains(pEvent->pos())) {
                if(cloudData->getTransFerState() == TransferFail){
                    win->setToolTip(tr("Network exception"));
                } else {
                    win->setToolTip("");
                }
            }
        }
    }
    return QStyledItemDelegate::editorEvent(event, model, option, index);
}

void CloudTransItemDelegate::drawIconExpand(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    QVariant userData = index.data(Qt::UserRole);
    if (userData.canConvert<BaseFileDetailData*>()) {
        BaseFileDetailData *modelData = userData.value<BaseFileDetailData*>();
        CloudFileDetailData *cloudData = dynamic_cast<CloudFileDetailData *>(modelData);
        QRect paintRect = option.rect.adjusted(10,10,-10,-10);
        QRect iconRect = QRect(paintRect.width() + paintRect.x() - 28, paintRect.y() + 8,16,16);
        if (cloudData) {
            double progress = cloudData->getProgress();
            if (cloudData->getTransFerState() == WaitTransfer || cloudData->getTransFerState() == Transferring) {
                if (!mItemShineTimerMap.contains(cloudData->fileId)) {
                    QTimer *timer = new QTimer;
                    connect(timer, &QTimer::timeout, const_cast<CloudTransItemDelegate*>(this), &CloudTransItemDelegate::onShineTimeout);
                    timer->setInterval(800);
                    QVariant variant;
                    variant.setValue(cloudData->fileId);
                    timer->setProperty("fileId", variant);
                    timer->start();
                    mItemShineTimerMap.insert(cloudData->fileId, timer);
                    mItemShineStateMap.insert(cloudData->fileId, 0);
                }
                QPen pen;
                pen.setCapStyle(Qt::FlatCap);
                pen.setWidthF(3.0);
                pen.setColor(QColor(164, 175, 255, 100));
                painter->setPen(pen);
                QRect paintRect = iconRect.adjusted(0, 0, 0, 0);
                painter->drawArc(paintRect, 0 * 16, 16 * 360);

                QConicalGradient conicalGradient(paintRect.center(), 270);
                conicalGradient.setColorAt(0, QColor("#BE6BFF"));
                conicalGradient.setColorAt(1, QColor("#7D8FFF"));
                pen.setBrush(conicalGradient);
                painter->setPen(pen);
                painter->drawArc(paintRect, -106 * 16, 16 * (-360 * progress / 100));

                if (mItemShineStateMap.contains(cloudData->fileId)) {
                    int shine = mItemShineStateMap.value(cloudData->fileId);
                    if (shine) {
                        pen.setColor(QColor(223,208,242, 100));
                    } else {
                        pen.setColor(QColor(251,251,251, 0));
                    }
                    painter->setPen(pen);
                    QRect shineRect = iconRect.adjusted(-2, -2, 2, 2);
                    painter->drawArc(shineRect, 0 * 16, 16 * 360);
                }
            } else {
                if (mItemShineTimerMap.contains(cloudData->fileId)) {
                    QTimer *timer = mItemShineTimerMap.take(cloudData->fileId);
                    timer->stop();
                    timer->deleteLater();
                }
                if (cloudData->getTransFerState() == TransferFail) {
                    painter->drawPixmap(iconRect, mTransFailPixmap);
                } else {
                    if (cloudData->getTransFerState() == TransferSuccess) {
                        painter->drawPixmap(iconRect, mTransSuccessPixmap);
                    }
                }
            }
            if (cloudData->getTransFerState() == WaitTransfer || cloudData->getTransFerState() == Transferring) {
                QPainterPath path;
                path.addRoundedRect(paintRect, 8, 8);
                painter->fillPath(path, QColor(240,240,240, 100));
            }
        }
    }
}

void CloudTransItemDelegate::drawListExpand(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    QVariant userData = index.data(Qt::UserRole);
    if (userData.canConvert<BaseFileDetailData*>()) {
        BaseFileDetailData *modelData = userData.value<BaseFileDetailData*>();
        CloudFileDetailData *cloudData = dynamic_cast<CloudFileDetailData *>(modelData);
        QRect paintRect = option.rect.adjusted(0,0,-0,-0);
        QRect iconRect = QRect(paintRect.width() - 24 - 56 + paintRect.x(),32 + paintRect.y(),16,16);
        if (cloudData) {
            double progress = cloudData->getProgress();
            if (cloudData->getTransFerState() == WaitTransfer || cloudData->getTransFerState() == Transferring) {
                if (!mItemShineTimerMap.contains(cloudData->fileId)) {
                    QTimer *timer = new QTimer;
                    connect(timer, &QTimer::timeout, const_cast<CloudTransItemDelegate*>(this), &CloudTransItemDelegate::onShineTimeout);
                    timer->setInterval(800);
                    QVariant variant;
                    variant.setValue(cloudData->fileId);
                    timer->setProperty("fileId", variant);
                    timer->start();
                    mItemShineTimerMap.insert(cloudData->fileId, timer);
                    mItemShineStateMap.insert(cloudData->fileId, 0);
                }
                QPen pen;
                pen.setCapStyle(Qt::FlatCap);
                pen.setWidthF(3.0);
                pen.setColor(QColor(164, 175, 255, 100));
                painter->setPen(pen);
                QRect paintRect = iconRect.adjusted(0, 0, 0, 0);
                painter->drawArc(paintRect, 0 * 16, 16 * 360);

                QConicalGradient conicalGradient(paintRect.center(), 270);
                conicalGradient.setColorAt(0, QColor("#BE6BFF"));
                conicalGradient.setColorAt(1, QColor("#7D8FFF"));
                pen.setBrush(conicalGradient);
                painter->setPen(pen);
                painter->drawArc(paintRect, -106 * 16, 16 * (-360 * progress / 100));

                if (mItemShineStateMap.contains(cloudData->fileId)) {
                    int shine = mItemShineStateMap.value(cloudData->fileId);
                    if (shine) {
                        pen.setColor(QColor(223,208,242, 100));
                    } else {
                        pen.setColor(QColor(251,251,251, 0));
                    }
                    painter->setPen(pen);
                    QRect shineRect = iconRect.adjusted(-2, -2, 2, 2);
                    painter->drawArc(shineRect, 0 * 16, 16 * 360);
                }
            } else {
                if (mItemShineTimerMap.contains(cloudData->fileId)) {
                    QTimer *timer = mItemShineTimerMap.take(cloudData->fileId);
                    timer->stop();
                    timer->deleteLater();
                }
                if (cloudData->getTransFerState() == TransferFail) {
                    painter->drawPixmap(iconRect, mTransFailPixmap);
                } else {
                    if (cloudData->getTransFerState() == TransferSuccess) {
                        painter->drawPixmap(iconRect, mTransSuccessPixmap);
                    }
                }
            }
            if (cloudData->getTransFerState() == WaitTransfer || cloudData->getTransFerState() == Transferring) {
                QPainterPath path;
                path.addRoundedRect(paintRect.adjusted(2,2,-2,-2), 8, 8);
                painter->fillPath(path, QColor(240,240,240, 100));
            }
        }
    }
}
