#include "basefileitemdelegate.h"
#include "basefileview.h"
#include <QPainter>
#include <QDateTime>
#include <QApplication>
#include <QPainterPath>
#include <QMouseEvent>
#include <QFileInfo>
#include "./src/util/guiutil.h"
#include "./src/style/stylesheethelper.h"

BaseFileItemDelegate::BaseFileItemDelegate(BaseFileView *listView, QObject *parent)
    : QStyledItemDelegate{parent}
    , mListView(listView)
    , mHoverRow(-1)
{
    initData();
}

BaseFileItemDelegate::~BaseFileItemDelegate()
{
    for (QTimer *timer : mItemShineTimerMap.values()) {
        timer->stop();
        timer->deleteLater();
    }
    mItemShineTimerMap.clear();
}

void BaseFileItemDelegate::initData()
{
    mCheckFileExist = false;
    mNormalFont = QApplication::font();
    mLittleFont = QApplication::font();

    mNormalFont.setWeight(60);
    mNormalFont.setPixelSize(12);
    mLittleFont.setWeight(44);
    mLittleFont.setPixelSize(11);
    mStarredPixmap.load(":/updf-win-icon/ui2/2x/welcome-page/starreding.png");
    mMenuPixmap.load(":/updf-win-icon/ui2/2x/welcome-page/selectfolder.png");
    mMenuHoverPixmap.load(":/updf-win-icon/ui2/2x/welcome-page/selectfolder_hover.png");
    mMenuPressedPixmap.load(":/updf-win-icon/ui2/2x/welcome-page/selectfolder_pressed.png");
    mPinPixmap.load(":/updf-win-icon/ui2/2x/welcome-page/pin.png");
    mDefaultIconPixmap.load(":/updf-win-icon/icon2x/24icon/historyfileImg.png");
    mPagePixmap.load(":/updf-win-icon/ui2/2x/welcome-page/page.png");
    mLocalFilePixmap.load(":/updf-win-icon/ui2/2x/welcome-page/local_file.png");
    mCloudSyncFilePixmap.load(":/updf-win-icon/ui2/2x/welcome-page/cloud_file1.png");
    mCloudNosyncFilePixmap.load(":/updf-win-icon/ui2/2x/welcome-page/cloud_nosync_file.png");
    mTransSuccessPixmap.load("://updf-win-icon/ui2/2x/welcome-page/transport_success.png");
    mTransFailPixmap.load("://updf-win-icon/ui2/2x/welcome-page/transport_fail.png");
    mlockImg.load(":/updf-win-icon/ui2/2x/batch/lock.png");

    mListBgImg.load(":/updf-win-icon/ui2/2x/welcome-page/line.png");
    mIconBgImg.load(":/updf-win-icon/ui2/2x/welcome-page/box.png");
    mHoverRow = -1;
}

void BaseFileItemDelegate::setDrawHoverStyle(bool draw)
{
    mDrawHoverStyle = draw;
    // update mHoverRow
    if (mHoverRow != -1) {
        emit updateRow(mHoverRow);
    }
    if (!mDrawHoverStyle) {
        mHoverRow = -1;
    }
}

void BaseFileItemDelegate::setCheckFileExist(bool check)
{
    mCheckFileExist = check;
}

void BaseFileItemDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    QStyledItemDelegate::paint(painter,option,index);

    auto pos = option.widget->mapFromGlobal(QCursor::pos());
    painter->setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing);
    if (index.isValid()) {
        // 清空背景
        painter->setPen(Qt::transparent);
        painter->setBrush(QColor("#FBFBFB"));
        painter->drawRect(option.rect);
        if (option.rect.contains(pos) /*&& mDrawHoverStyle*/) {
            drawHoverStyle(painter, option, index);
            int row = index.row();
            mHoverRow = row;
        }
        if(mListView->viewMode() == QListView::ListMode){
            drawList(painter,option,index);
            drawListExpand(painter, option, index);
        } else{
            drawIcon(painter,option,index);
            drawIconExpand(painter, option, index);
        }
    }
}

bool BaseFileItemDelegate::editorEvent(QEvent *event, QAbstractItemModel *model, const QStyleOptionViewItem &option, const QModelIndex &index)
{
    QMouseEvent *pEvent = static_cast<QMouseEvent *>(event);
    auto paintRect = option.rect;
    auto menuRectBtn =  mListView->viewMode() == QListView::ListMode ?  QRect(paintRect.width() - 24 - 20 - 10 + paintRect.x(),26 + paintRect.y(),20,20) :
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
        }
    }
    return QStyledItemDelegate::editorEvent(event, model, option, index);
}

void BaseFileItemDelegate::drawIcon(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    QVariant userData = index.data(Qt::UserRole);
    if (userData.canConvert<BaseFileDetailData*>()) {
        BaseFileDetailData *modelData = userData.value<BaseFileDetailData*>();
        CloudFileDetailData *cloudData = dynamic_cast<CloudFileDetailData *>(modelData);
        QRect paintRect = option.rect.adjusted(10,10,-10,-10);
        auto pos = option.widget->mapFromGlobal(QCursor::pos());
        painter->setBrush((option.state.testFlag(QStyle::State_MouseOver) || paintRect.contains(pos))
                           ? QColor("#FFFFFF") : QColor("#F7F7F7"));
        if(!option.state.testFlag(QStyle::State_MouseOver) && !option.rect.contains(pos)){
            painter->drawRoundedRect(paintRect,8,8);
        }
        if (modelData->getHighlight()) {
            QPainterPath path;
            path.addRoundedRect(paintRect,8,8);
            painter->setPen(QPen("#964FF2"));
            painter->drawPath(path);
        }
        painter->setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform,true);
        QPixmap image(modelData->imgPath);
        QPixmap newPixmap = image;
        bool needOutline = true;
        if (modelData->havePwd) {
            newPixmap = mlockImg;
            needOutline = false;
        } else {
            if (image.isNull()) {
                newPixmap = mDefaultIconPixmap;
                needOutline = false;
            }
        }
        auto finalRect = getFinalRect(newPixmap.size(),false);
        if(needOutline){
            QPen pen;
            pen.setWidthF(1);
            pen.setColor("#DADADA");
            painter->setPen(pen);
            painter->drawRect(QRect(finalRect.x() + paintRect.x(),finalRect.y() + paintRect.y(),finalRect.width(),finalRect.height()));
        }
        painter->drawPixmap(QRect(finalRect.topLeft() + paintRect.topLeft(), finalRect.size()), newPixmap, newPixmap.rect());
        if(modelData->isPin){
            painter->drawPixmap(QRect(finalRect.x() + finalRect.width() - 8 + paintRect.x(),finalRect.y() - 8  + paintRect.y(),16,16),mPinPixmap);
        }
        painter->setPen(QColor("#353535"));
        painter->setFont(mNormalFont);
        QFontMetrics fontWidth(mNormalFont);
        QString elideNote = fontWidth.elidedText(modelData->fileName, Qt::ElideRight,220);
        bool strikeStyle = false;
        if (cloudData && cloudData->getTransFerState() == TransferSuccess && cloudData->getFileWriteDown() && mCheckFileExist && !QFileInfo(modelData->filePath).exists()) {
//            elideNote = fontWidth.elidedText(modelData->fileName, Qt::ElideRight,190); //+ tr("Deleted")
//            int xOffset = fontWidth.width(elideNote) % 110;
//            int yIndex = 1;
//            if (xOffset + 30 > 110) {
//                xOffset = 0;
//                yIndex += 1;
//            }
//            int line = qBound(1, fontWidth.width(elideNote) / 110 + yIndex, 2);
//            int yOffset = line * 12 + (line - 1) * 6;
//            painter->drawText(30 + paintRect.x() + xOffset + 8,168 + paintRect.y() + yOffset,tr("Deleted"));
            QFont newFont = painter->font();
            newFont.setStrikeOut(true);
            painter->setFont(newFont);
            strikeStyle = true;
            painter->setPen(QColor("#B8B8B8"));
        }
        painter->drawText(QRectF(30 + paintRect.x(),168 + paintRect.y(),114,32*(StyleSheetHelper::WinDpiScale())),Qt::TextWrapAnywhere|Qt::AlignHCenter,elideNote);
        if (strikeStyle) {
            painter->setPen(QColor("#B8B8B8"));
        } else {
            painter->setPen(QColor("#8A8A8A"));
        }
        painter->drawPixmap(QRect(8 + paintRect.x(),240 -26 + paintRect.y(),12,12), mPagePixmap);

        // page | size
        painter->setFont(mLittleFont);
        QString showCount = QString::number(modelData->currentPageIndex + 1) + "/" + QString::number(modelData->totalPage)
                + "     " + BaseFileDefine::formatShowStorage(modelData->fileSize);
        painter->drawText(8 + paintRect.x() + 14,240 -16 + paintRect.y(), showCount);

        painter->setPen(QColor("#D5D5D5"));
        int leftWidth = DrawUtil::textWidth(mLittleFont, QString::number(modelData->currentPageIndex + 1) + "/" + QString::number(modelData->totalPage)
                + "  ");
        painter->drawText(8 + paintRect.x() + 14 + leftWidth,240 -16 + paintRect.y(), "|");

        if(modelData->isStarred){
            painter->drawPixmap(paintRect.width() + paintRect.x() - 50,240 - 29 + paintRect.y(),16,16,mStarredPixmap);
        }
        painter->drawPixmap(QRect(paintRect.width() - 30 + paintRect.x(),paintRect.height() - 30 + paintRect.y(),20,20),
                            mMenuBtnState[index.row()] == MouseState::Normal ? mMenuPixmap :
                            mMenuBtnState[index.row()] == MouseState::Hover ?  mMenuHoverPixmap :mMenuPressedPixmap);
    }
}

void BaseFileItemDelegate::drawIconExpand(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
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
                    connect(timer, &QTimer::timeout, const_cast<BaseFileItemDelegate*>(this), &BaseFileItemDelegate::onShineTimeout);
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
                    painter->drawPixmap(iconRect, mCloudNosyncFilePixmap);
                } else {
                    painter->drawPixmap(iconRect, mCloudSyncFilePixmap);
                }
            }
            if (cloudData->getTransFerState() == WaitTransfer || cloudData->getTransFerState() == Transferring) {
                QPainterPath path;
                path.addRoundedRect(paintRect, 8, 8);
                painter->fillPath(path, QColor(240,240,240, 100));
            }
        } else {
//            painter->drawPixmap(iconRect, mLocalFilePixmap);
        }
    }
}

void BaseFileItemDelegate::drawList(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    QVariant userData = index.data(Qt::UserRole);
    if (userData.canConvert<BaseFileDetailData*>()) {
        BaseFileDetailData *modelData = userData.value<BaseFileDetailData*>();
        CloudFileDetailData *cloudData = dynamic_cast<CloudFileDetailData *>(modelData);
        QRect paintRect = option.rect.adjusted(0,0,-0,-0);
        auto pos = option.widget->mapFromGlobal(QCursor::pos());
        painter->setPen(Qt::transparent);
        painter->setBrush((option.state.testFlag(QStyle::State_MouseOver) || paintRect.contains(pos))
                           ? QColor("#FFFFFF") : QColor("#FBFBFB"));
        if(!option.state.testFlag(QStyle::State_MouseOver) && !option.rect.contains(pos)){
            painter->drawRoundedRect(paintRect,8,8);
        }
        if (modelData->getHighlight()) {
            QPainterPath path;
            path.addRoundedRect(paintRect,8,8);
            painter->setPen(QPen("#964FF2"));
            painter->drawPath(path);
        }
        painter->setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform,true);

        QPixmap image(modelData->imgPath);
        QPixmap newPixmap = image;
        bool needOutline = true;
        if (modelData->havePwd) {
            newPixmap = mlockImg;
            needOutline = false;
        } else {
            if (image.isNull()) {
                newPixmap = mDefaultIconPixmap;
                needOutline = false;
            }
        }
        auto finalRect = getFinalRect(newPixmap.size(),true);
        if(needOutline){
            QPen pen;
            pen.setWidthF(1);
            pen.setColor("#DADADA");
            painter->setPen(pen);
            painter->drawRect(QRect(finalRect.x() + paintRect.x(),finalRect.y() + paintRect.y(),finalRect.width(), finalRect.height()));
        }
        painter->drawPixmap(QRect(finalRect.topLeft() + paintRect.topLeft(), finalRect.size()), newPixmap, newPixmap.rect());
        if(modelData->isPin){
            painter->drawPixmap(QRect(finalRect.x() + finalRect.width() - 6 + paintRect.x(),finalRect.y() - 6 + paintRect.y(),12,12),mPinPixmap);
        }
        painter->setPen(QColor("#353535"));
        painter->setFont(mNormalFont);
        QFontMetrics fontWidth(mNormalFont);
        QString elideNote = fontWidth.elidedText(modelData->fileName, Qt::ElideRight,350);
        bool strikeStyle = false;
        if (cloudData && cloudData->getTransFerState() == TransferSuccess && cloudData->getFileWriteDown() && mCheckFileExist && !QFileInfo(modelData->filePath).exists()) {
//            elideNote = fontWidth.elidedText(modelData->fileName, Qt::ElideRight,300); //+ tr("Deleted")
//            painter->drawText(72 + paintRect.x() + fontWidth.width(elideNote) + 8,20 + 12 + paintRect.y(),tr("Deleted"));
            QFont newFont = painter->font();
            newFont.setStrikeOut(true);
            painter->setFont(newFont);
            strikeStyle = true;
            painter->setPen(QColor("#B8B8B8"));
        }
        painter->drawText(72 + paintRect.x(),20 + 12 + paintRect.y(),elideNote);
        if (strikeStyle) {
            painter->setPen(QColor("#B8B8B8"));
        } else {
            painter->setPen(QColor("#8A8A8A"));
        }
        painter->setFont(mLittleFont);

        // page | size
        painter->drawPixmap(QRect(72 + paintRect.x(),31 + 15 + paintRect.y(),12,12), mPagePixmap);
        QString showCount = QString::number(modelData->currentPageIndex + 1) + "/" + QString::number(modelData->totalPage)
                + "     " + BaseFileDefine::formatShowStorage(modelData->fileSize);
        painter->drawText(72 + paintRect.x() + 14,36 + 20 + paintRect.y(), showCount);

        painter->setPen(QColor("#D5D5D5"));
        int leftWidth = DrawUtil::textWidth(mLittleFont, QString::number(modelData->currentPageIndex + 1) + "/" + QString::number(modelData->totalPage)
                + "  ");
        painter->drawText(72 + paintRect.x() + 14 + leftWidth,36 + 20 + paintRect.y(), "|");
        int countWidth = DrawUtil::textWidth(mLittleFont,showCount);
        if(modelData->isStarred){
            painter->drawPixmap(72 + 24 + countWidth + paintRect.x(),31 + 12 + paintRect.y(),16,16,mStarredPixmap);
        }
        painter->setPen(QColor("#8A8A8A"));
//        QString showPassTime = BaseFileDefine::formatPassTimeStr(modelData->openTime);
        if (modelData->openTime > QDateTime::fromSecsSinceEpoch(0)) {
            QString showTime = "";
            qint64 daysTo = modelData->openTime.daysTo(QDateTime::currentDateTime());
            if (daysTo <= 0) {
                showTime = modelData->openTime.time().toString(Qt::DefaultLocaleLongDate);
            } else if (daysTo <= 30) {
                showTime = modelData->openTime.date().toString("MM/dd");
            } else {
                showTime = modelData->openTime.date().toString("yyyy/MM");
            }
            int textWidth = DrawUtil::textWidth(mLittleFont, showTime);
            painter->drawText(QPointF(paintRect.width() - 30 - 280 + paintRect.x(),27 + 17 + paintRect.y()), showTime);
        }
        painter->drawPixmap(paintRect.width() - 24 - 20 + paintRect.x(),30 + paintRect.y(),20,20,
                            mMenuBtnState[index.row()] == MouseState::Normal ? mMenuPixmap :
                            mMenuBtnState[index.row()] == MouseState::Hover ?  mMenuHoverPixmap :mMenuPressedPixmap);
    }
}

void BaseFileItemDelegate::drawListExpand(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
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
                    connect(timer, &QTimer::timeout, const_cast<BaseFileItemDelegate*>(this), &BaseFileItemDelegate::onShineTimeout);
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
                    painter->drawPixmap(iconRect, mCloudNosyncFilePixmap);
                } else {
                    painter->drawPixmap(iconRect, mCloudSyncFilePixmap);
                }
            }
            if (cloudData->getTransFerState() == WaitTransfer || cloudData->getTransFerState() == Transferring) {
                QPainterPath path;
                path.addRoundedRect(paintRect.adjusted(2,2,-2,-2), 8, 8);
                painter->fillPath(path, QColor(240,240,240, 100));
            }
        } else {
//            painter->drawPixmap(iconRect, mLocalFilePixmap);
        }
    }
}

void BaseFileItemDelegate::drawHoverStyle(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    if (mListView->viewMode() == QListView::IconMode) {
        auto w = 10.0 / painter->transform().m11();
        auto rc = option.rect.adjusted(10,10,-10,-10);
        painter->drawImage(QRectF( -w + rc.x(), - w + rc.y(),rc.width() + w*2 ,rc.height() +w*2),mIconBgImg);
    } else {
        auto w = 0.0 / painter->transform().m11();
        auto rc = option.rect.adjusted(0,0,-0,-0);
        painter->drawImage(QRectF( -w + rc.x(), - w + rc.y(),rc.width() + w*2,rc.height() + w*2),mListBgImg);
    }
}

//40*54  88*128
QRect BaseFileItemDelegate::getFinalRect(const QSize& size,bool list) const
{
    QSize targetSize;
    QRect targetRect;
    auto maxSize = list ? QSize(40,54) : QSize(88,128);
    bool whScale = size.width()*maxSize.height() >= size.height()*maxSize.width();//过宽 true
    if(whScale){
        targetSize = QSize(maxSize.width(),size.height()*maxSize.width()/size.width());
    }else if(!whScale){
        targetSize = QSize(size.width()*maxSize.height()/size.height(),maxSize.height());
    }

    if(list){
        targetRect = QRect(QPoint(24 + (40 - targetSize.width())/2,(80 - targetSize.height())/2),targetSize);
    } else {
        targetRect = QRect(QPoint(44 + (88 - targetSize.width())/2,(164-targetSize.height())/2),targetSize);
    }
    return targetRect;
}

void BaseFileItemDelegate::updateRowByFileId(qint64 fileId)
{
    int row = mListView->itemRow(fileId);
    if (row < 0) {
        return;
    }
    emit updateRow(row);
}

void BaseFileItemDelegate::onShineTimeout()
{
    QTimer *timer = qobject_cast<QTimer *>(sender());
    if (!timer) {
        return;
    }
    qint64 fileId = timer->property("fileId").toLongLong();
    int shine = mItemShineStateMap.value(fileId);
    mItemShineStateMap.insert(fileId, !shine);
    updateRowByFileId(fileId);
}

QSize BaseFileItemDelegate::sizeHint(const QStyleOptionViewItem &, const QModelIndex &) const
{
    return mListView->viewMode() == QListView::ListMode ? QSize(800, 80):QSize(176 + 20, 240 + 20);
}
