#include "pagebasicview.h"
#include <QDateTime>
#include <QOpenGLWidget>
#include <QTime>
#include "pagebasicitemdatadef.h"
#include <QTimer>
#include "pagebasicitemmodel.h"
#include <QScrollBar>
#include <QDebug>
#include <QWheelEvent>
#include <QSortFilterProxyModel>
#include <QKeyEvent>
#include <QModelIndex>
#include <iostream>
#include <QApplication>
#include <QClipboard>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include "./src/pdfeditor/pdfeditor.h"
#include "./src/datadef/viewmode.h"
#include "./src/pdfeditor/priv/pagecrop/pagecropctrlpriv.h"
#include "./src/mainview.h"
#include "./src/toolsview/lefttoolsview.h"
#include "./src/basicwidget/messagewidget.h"
#include "./src/pageengine/pdfcore/PDFPageManager.h"
#include "./src/pageengine/rendermanager.h"
#include "./src/pdfpagemgr/pdfpagemgrview.h"
#include "./src/toolsview/thumbnailpageview.h"
#include "./src/usercenter/limituse/limituse_enterprise/enterpriselimituse.h"
#include "./src/pdfeditor/priv/editpagectrlpriv.h"

PageBasicView::PageBasicView(PdfPageMgrView *pdfPageMgrView,BasicViewMode mode,QWidget* parent,PdfPageView* pageView)
    : AbstractItemViewType<QListView>(parent)
    , mPdfPageMgrView(pdfPageMgrView)
    , mBasicViewMode(mode)
    , mInDrag(false)
    , mPageView(pageView)
    , mLastMargin(0)
    , mZoomStatus(ZoomStatus::ZOOMMIN)
    , mMousePress(false)
    , mMousePressMove(false)
    , mPressIn(false)
    , mSmoothScroll(true)
{
    setFocusPolicy(Qt::StrongFocus);
    initView();
    connect(this, &PageBasicView::extractPages, mPdfPageMgrView, &PdfPageMgrView::onExtractPages);
}

void PageBasicView::initView()
{
    mPageModel = new PageBasicItemModel(this);

    mPageBasicItemDelegate = new PageBasicItemDelegate(mPageView,this,this);
    mPageBasicItemDelegate->setItemMargins({2, 2, 2, 2});
    connect(this, &PageBasicView::updateItem, mPageModel, &PageBasicItemModel::onItemUpdate);

    setItemDelegate(mPageBasicItemDelegate);       //为视图设置委托

    //像素滚动
    setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setEditTriggers(QAbstractItemView::NoEditTriggers);

    setModel(mPageModel);
    setResizeMode(QListView::Adjust);
    setSelectionMode(QAbstractItemView::SingleSelection);
    setSpacing(3);
    if(mBasicViewMode == MgrViewMode){
        initViewFlag();
    }
}

void PageBasicView::initViewFlag()
{
    setSpacing(ViewItemSpace);                   //为视图设置控件间距
    setViewMode(QListView::IconMode); //设置Item图标显示
    setFlow(QListView::LeftToRight) ;
    setDragEnabled(false);
    setLayoutMode(QListView::Batched);
    setAutoScroll(false);
    setUniformItemSizes(true);
    //view缩放后自动调整位置
    setResizeMode(QListView::Adjust);
    setSelectionMode(QAbstractItemView::ExtendedSelection);
}

void PageBasicView::takeCloseTask()
{
    getModel()->clearItems();
}

void PageBasicView::wheelEvent(QWheelEvent *e)
{
    if(e->modifiers() & Qt::ControlModifier ){
        QPoint scrollAmount = e->angleDelta();
        // 正值表示滚轮远离使用者（放大），负值表示朝向使用者（缩小）
        bool zoomIn = scrollAmount.y() > 0;
        takeZoomTask(zoomIn);
        e->accept();
    } else {
        QListView::wheelEvent(e);
    }
}

void PageBasicView::takeZoomTask(double scaleRatio)
{
    // 获取container的宽度
    int maxWidth = 0;
    emit containerWidth(&maxWidth);
    QSize itemSize = mPageBasicItemDelegate->itemSize();
    QSize newItemSize = itemSize * scaleRatio;
    int itemRealWidth = newItemSize.width();
    int xMargin = ViewItemSpace * 2;
    int lastSpace = 10;
    int rowCount = (maxWidth - xMargin - lastSpace - ViewItemSpace) / (itemRealWidth + ViewItemSpace);
    // 如果一行不满
    rowCount = qMin(getModel()->rowCount(), rowCount);
    // 最少一个
    rowCount = qMax(1, rowCount);
    // calc margin
    int realMargin = (maxWidth - (rowCount * itemRealWidth + (rowCount - 1) * ViewItemSpace) - lastSpace - xMargin -8) / 2;
//    qDebug() << "width:" << maxWidth << " rowCount:" << rowCount << " realMargin:" << realMargin << " newItemSize.width:" << newItemSize.width();

    if(scaleRatio != 1.0) {
        zoomChange(NORMAL);
    }
    if (scaleRatio != 1.0 && newItemSize.width() >= PageItemMinWidth * (1.25 * 4)) {
        zoomChange(ZOOMMAX);
    }
    //不可小于最小值
    if(scaleRatio != 1.0 && newItemSize.width() <= PageItemMinWidth)
    {
        zoomChange(ZOOMMIN);
    }

    mPageBasicItemDelegate->setItemSize(newItemSize);
    // 触发container设置margin
    emit marginChanged(realMargin, 0, realMargin, 0);
    mLastMargin = realMargin;
}

void PageBasicView::setRotate(int pageIndex,bool rotation_left)
{
    PrePageItemData *itemData = getModel()->getItemByRow(pageIndex);
    qSwap(itemData->realWidth, itemData->realHeight);
    emit updateItem(pageIndex);
}

QSize PageBasicView::getItemSize()
{
    return mPageBasicItemDelegate->itemSize();
}

void PageBasicView::setPagesDetail(const std::vector<PageDetail>& pageDetailList)
{
    int count = pageDetailList.size();
    getModel()->clearItems();
    for (int i = 0; i < count; i++) {
        int realHeight = pageDetailList.at(i).realHeight;
        int realWidth = pageDetailList.at(i).realWidth;
        PrePageItemData *itemData = new PrePageItemData;
        itemData->pageIndex = i;
        itemData->realHeight = realHeight;
        itemData->realWidth = realWidth;
        getModel()->appendItem(itemData, calcItemWH(realWidth, realHeight));
    }
}

QSize PageBasicView::calcItemWH(int realWidth, int realHeight)
{
    const double imgRatio = double(realHeight)/double(realWidth);
    int itemWidth = 0;
    int itemHeight = 0;
    if (realWidth < realHeight) {
        itemHeight = imgRatio * PageItemBasicWidth;
        itemWidth = PageItemBasicWidth;
    } else {
        itemHeight = PageItemBasicHeight;
        itemWidth = PageItemBasicHeight / imgRatio;
    }
    return QSize(itemWidth, itemHeight);
}

void PageBasicView::showEvent(QShowEvent *event)
{
    setFocus(Qt::OtherFocusReason);
    QListView::showEvent(event);
}

void PageBasicView::resizeEvent(QResizeEvent *event)
{
    QListView::resizeEvent(event);
    takeZoomTask(1.0); //不缩放，只是调整布局
}

void PageBasicView::mousePressEvent(QMouseEvent *e)
{
    QListView::mousePressEvent(e);
    mMousePress = true;
    mMousePressMove = false;
    mStartPos = e->pos();
    QModelIndex modelIndex = indexAt(e->pos());
    if(!modelIndex.isValid()) {
        return;
    }
    if (e->button() & Qt::LeftButton) {
        mPressIn = true;
    } else if (e->button() & Qt::RightButton) {
        if (e->button() == Qt::RightButton) {
            mPageView->getPdfEditor()->getEditCtrl()->clearSelect();
        }
        mMousePress = false;
        mPressIn = false;
        bool popUpMenu = true;
        ThumbnailPageView* thumbnailPageView = dynamic_cast<ThumbnailPageView*>(this);
        if (thumbnailPageView && thumbnailPageView->isInPageUnchangeMode()) {
            popUpMenu = false;
        }
        if (popUpMenu) {
            QPoint pos = mapToGlobal(e->pos());
            pos -= QPoint(14, 14);
            std::vector<int> pageIndexes;
            bool containCurItem = false;
            QList<QModelIndex> selectIndexs = getSelectedIndexs();
            for (QModelIndex mIndex : selectIndexs) {
                if (mIndex.row() == modelIndex.row()) {
                    containCurItem = true;
                }
                pageIndexes.push_back(mIndex.row());
            }
            if (!containCurItem) {
                pageIndexes.push_back(modelIndex.row());
            }
            getPdfPageMgrView()->popMenu(pos, modelIndex.row(), this, pageIndexes);
        }
    }
    mInDrag = false;
}

void PageBasicView::mouseMoveEvent(QMouseEvent *e)
{
    int factorValue = 1;
    if (mBasicViewMode == PreViewMode) {
        factorValue = 3;
    }
    if (!mInDrag && mPressIn && (e->pos() - mStartPos).manhattanLength() > (QApplication::startDragDistance() * factorValue)) {
        ThumbnailPageView* thumbnailPageView = dynamic_cast<ThumbnailPageView*>(this);
        bool startDrag = true;
        auto enable = mPageView->getPdfEditor()->getMainView()->getPermissionStru()->AllowAssembly;
        if (isPrivateDeploy() && !Loginutil::getInstance()->isLogin()) {
            enable = false;
        }
        if (thumbnailPageView && thumbnailPageView->isInPageUnchangeMode() || !enable) {
            startDrag = false;
        }
        if (startDrag) {
            mInDrag = true;
            startUserDrag(e);
            e->accept();
        }
    } else if (mInDrag) {
        dragMove(e);
        e->accept();
    }
    if (!mInDrag) {
        QListView::mouseMoveEvent(e);
    }
    if (mMousePress && !mInDrag && !mPressIn) {
        mMousePressMove = true;
        mEndPos = e->pos();
    }
    update();
}

void PageBasicView::mouseReleaseEvent(QMouseEvent *e)
{
    mMousePress = false;
    mPressIn = false;
    mMousePressMove = false;

    if (mInDrag) {
        mInDrag = false;
        drop(e);
    }
    QListView::mouseReleaseEvent(e);
    update();
}

void PageBasicView::dragMove(QMouseEvent *e)
{
    int dragIntoRow = indexAt(e->pos()).row();
    if (dragIntoRow == -1) {
        return;
    }
    mHighlightedRow = dragIntoRow;
    emit updateItem(mHighlightedRow);
//    e->setDropAction(Qt::MoveAction);
//    e->accept();

    if (mSmoothScroll) {
        mSmoothScroll = false;
        // 移动到底部需要滚动条下拉
        if (verticalScrollBar()->minimum() != verticalScrollBar()->maximum() && verticalScrollBar()->value() < verticalScrollBar()->maximum()
                && (e->pos().y() + 100) > (viewport()->rect().y() + viewport()->rect().height())) {
            verticalScrollBar()->setValue(verticalScrollBar()->value() + verticalScrollBar()->pageStep());
        }
        //到顶部需要滚动向上
        if (verticalScrollBar()->minimum() != verticalScrollBar()->maximum() && verticalScrollBar()->value() > verticalScrollBar()->minimum()
                && (e->pos().y() - 100) < viewport()->rect().y()) {
            verticalScrollBar()->setValue(verticalScrollBar()->value() - verticalScrollBar()->pageStep());
        }
        QTimer::singleShot(500, this, [&] {
            mSmoothScroll = true;
        });
    }
}

void PageBasicView::drop(QMouseEvent *e)
{
    setCursor(Qt::ArrowCursor);
    QByteArray mimeData = mDragMimeData;
    mDragMimeData.clear();
    if (mimeData.isEmpty()) {
        return;
    }
    QRect dropItemRect = visualRect(indexAt(e->pos()));
    QList<int> dragItemIndexList;
    int insertRow = 0;
    if (mBasicViewMode == PreViewMode) {
        // 左边还是右边插入
        if (e->pos().y() > (dropItemRect.y() + dropItemRect.height() / 2)) {
            insertRow = highlightedRow() + 1;
        } else {
            insertRow = highlightedRow();
        }
    } else {
        // 左边还是右边插入
        if (e->pos().x() > (dropItemRect.x() + dropItemRect.width() / 2)) {
            insertRow = highlightedRow() + 1;
        } else {
            insertRow = highlightedRow();
        }
    }
    int smallerThanInsert = 0; // 比插入位置index小的item数
    int largerThanInsert = 0; // 比插入位置index大的item数
    int dragIndex = 0;
    if (mimeData.contains(',')) {
        QList<QByteArray> list = mimeData.split(',');
        for (QByteArray index : list) {
            dragIndex = index.toInt();
            dragItemIndexList.append(dragIndex);
            if (insertRow > dragIndex) {
                smallerThanInsert += 1;
            } else {
                largerThanInsert += 1;
            }
        }
    } else {
        dragIndex = mimeData.toInt();
        if (insertRow > dragIndex) {
            smallerThanInsert = 1;
        } else {
            largerThanInsert = 1;
        }
        dragItemIndexList.append(dragIndex);
    }
    int insertIndex = insertRow - smallerThanInsert;
    // insert
    std::vector<CommandPageData> insertPageDataList;
    for (int pageIndex : dragItemIndexList) {
        PrePageItemData *itemData = getModel()->getItemByRow(pageIndex);
        if (!itemData) {
            continue;
        }
        PagePointer pagePointer = getPdfPageMgrView()->getPageManager()->pageAtIndex(pageIndex);
        insertPageDataList.push_back(CommandPageData(pagePointer, insertIndex, itemData->realWidth,
                                                                   itemData->realHeight));
        insertIndex++;
    }
    std::vector<CommandPageData> deletePageDataList;
    getSelectedPageList(&deletePageDataList);

    clearSelection();
    getPdfPageMgrView()->doDeleteInsertPage(deletePageDataList, insertPageDataList);
    // 定位
    bool needScroll = false;
    if (verticalScrollBar()->minimum() != verticalScrollBar()->maximum()) {
        needScroll = true;
    }
    // 需要在beginInsertRows后scroll
    if (needScroll) {
        insertIndex--;
        if (insertIndex >= 0) {
            emit currentPageChanged(insertIndex);
            setCurrentIndex(dynamic_cast<PageBasicItemModel*>(model())->currentIndexByRow(insertIndex));
        }
        QTimer::singleShot(1500, this, [=] {
            if (insertIndex >= 0) {
                scrollToShowItem(insertIndex);
            }
        });
    }
}
void PageBasicView::startDrag(Qt::DropActions supportedActions)
{
    // 需要保留空方法
}

void PageBasicView::startUserDrag(QMouseEvent *e)
{
    QList<QModelIndex> indexList = getSelectedIndexs();
    if (indexList.size() <= 0) {
        return;
    }
    QModelIndex lastIndex = indexList.last(); //赋初始值
    for (const QModelIndex& index : indexList) {
        if (visualRect(index).contains(e->pos())) {
            // 获取拖拽点的item
            lastIndex = index;
            break;
        }
    }
    QRect dragRect = visualRect(lastIndex);
    QPoint newPos = e->pos() - dragRect.topLeft();
    QPixmap bgPixmap = QPixmap("://updf-win-icon/ui2/2x/mouse-cross/mask-bg.png");
    QPixmap maskPixmap = QPixmap("://updf-win-icon/ui2/2x/mouse-cross/mask.png");

    QVariant userData = lastIndex.data(Qt::UserRole); //获取数据
    PageItemModelData modelData = userData.value<PageItemModelData>();

    QPixmap pixmap(bgPixmap.size());
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.drawPixmap(0, 0, bgPixmap);
    QRect iconRect = bgPixmap.rect().adjusted(15, 10, -15, -15);

    ThumbnailRenderParam renderParam;
    renderParam.item = mPageView->getPageItem(modelData.itemData->pageIndex);
    renderParam.view = this;
    renderParam.row = lastIndex.row();
    renderParam.col = lastIndex.column();
    auto realImg = RenderManager::instance()->getThumbnail(renderParam);

    if (modelData.itemData->rotate != 0) {
        QMatrix matrix;
        matrix.rotate(modelData.itemData->rotate);
        realImg = realImg.transformed(matrix);
    }
    //宽高比
    const double imgRatio = double(realImg.height())/double(realImg.width());
    double targetHeight = iconRect.height();
    double targetWidth = targetHeight/imgRatio;
    //修复比例
    if(double(iconRect.width()) < targetWidth ){
        targetWidth = double(iconRect.width());
        targetHeight = targetWidth*imgRatio;
    }
    QImage newImage = realImg.scaled(targetWidth, targetHeight,Qt::KeepAspectRatio, Qt::SmoothTransformation);
    QRect targetRect = QRect(iconRect.topLeft()+QPointF((iconRect.width() - targetWidth)/2,
                            (iconRect.height() - targetHeight)/2).toPoint(),
                            QSizeF(targetWidth,targetHeight).toSize());
    int adjustX = 0;
    QPen linePen;
    linePen.setWidth(1);
    linePen.setColor(QColor("#D9D9D9"));
    painter.setPen(linePen);
    if (indexList.size() >= 3) {
        adjustX = 4;
        targetRect = targetRect.adjusted(-adjustX, 0, 0, 0);
        painter.drawPixmap(targetRect.adjusted(adjustX*2, adjustX*2, adjustX*2, adjustX*2), maskPixmap.scaled(targetRect.size()));
        painter.drawRect(targetRect.adjusted(adjustX*2, adjustX*2, adjustX*2, adjustX*2));
    }
    if (indexList.size() >= 2) {
        adjustX = 2;
        targetRect = targetRect.adjusted(-adjustX, 0, 0, 0);
        painter.drawPixmap(targetRect.adjusted(adjustX*2, adjustX*2, adjustX*2, adjustX*2), maskPixmap.scaled(targetRect.size()));
        painter.drawRect(targetRect.adjusted(adjustX*2, adjustX*2, adjustX*2, adjustX*2));
    }

    painter.drawImage(targetRect, newImage);
    painter.drawRect(targetRect);
    // 页数
    painter.setRenderHint(QPainter::Antialiasing, true);
    QString pageNumber = QString::number(indexList.size());

    QPen pen;
    pen.setColor(Qt::white);
    painter.setPen(pen);
    QFont font;
    font.setPixelSize(12);
    font.setWeight(QFont::DemiBold);
    painter.setFont(font);

    QFontMetrics fm = painter.fontMetrics();
    int margin = 0;
    if (pageNumber.length() == 1) {
        margin = 6;
    } else {
        margin = 4;
    }
    QRect pageRect = QRect(iconRect.x() - 7, iconRect.y() - 2, fm.width(pageNumber) + 2 * margin, 20);
    QPainterPath subPath;
    subPath.addRoundedRect(pageRect, 4, 4);
    painter.fillPath(subPath, QColor("#FF6644"));
    painter.drawText(pageRect, Qt::AlignCenter, pageNumber);
    // 鼠标
    painter.drawPixmap(QRect(newPos.x(), newPos.y(), 18, 18), QPixmap(":/updf-win-icon/ui2/2x/mouse-cross/direction-adjustment.png"));
    QByteArray data;
    for (const QModelIndex& modelIndex : indexList) {
        data.append(QString::number(modelIndex.row()));
        data.append(',');
    }
    if (!data.isEmpty()) {
        data.chop(1);
    }

    mDragMimeData = data;
    setCursor(pixmap);
}

void PageBasicView::scrollToShowItem(int index)
{
    scrollTo(model()->index(index, 0));
}

void PageBasicView::takeZoomTask(bool zoomIn)
{
    // ratio
    double scaleRatio = 1.25;
    if (zoomIn) {
        if (getZoomStatus() == ZoomStatus::ZOOMMAX) {
            return;
        }
        scaleRatio = 1.25;
    } else {
        if (getZoomStatus() == ZoomStatus::ZOOMMIN) {
            return;
        }
        scaleRatio = 1.0/1.25;
    }
    takeZoomTask(scaleRatio);
}
void PageBasicView::deletePage(int pageIndex)
{
    getModel()->removeItem(pageIndex);
    emit pageCountChanged(model()->rowCount());
}

void PageBasicView::deletePages(const QList<int> &pageIndexs)
{
    getModel()->removeItems(pageIndexs);
    emit pageCountChanged(model()->rowCount());
}

void PageBasicView::insertPage(int pageIndex, int realWidth, int realHeight)
{
    PrePageItemData *itemData = new PrePageItemData;
    itemData->pageIndex = pageIndex;
    itemData->realWidth = realWidth;
    itemData->realHeight = realHeight;
    getModel()->insertItem(pageIndex, itemData, calcItemWH(realWidth, realHeight));

    clearSelection();
    selectionModel()->select(getModel()->currentIndexByRow(pageIndex), QItemSelectionModel::Select);
    emit pageCountChanged(model()->rowCount());
}

void PageBasicView::insertPages(const QList<PrePageItemData *> &pageItemDatas)
{
    QMap<int, PageItemModelData> modelDatas;
    for (PrePageItemData *itemData : pageItemDatas) {
        modelDatas.insert(itemData->pageIndex, PageItemModelData(itemData, calcItemWH(itemData->realWidth, itemData->realHeight)));
    }
    getModel()->insertItems(modelDatas);

    clearSelection();
    for (PrePageItemData *itemData : pageItemDatas) {
        selectionModel()->select(getModel()->currentIndexByRow(itemData->pageIndex), QItemSelectionModel::Select);
    }
    emit pageCountChanged(model()->rowCount());
}
void PageBasicView::extract()
{
    std::vector<int> pageIndexes;
    auto indexes = getSelectedIndexs();
    for (const auto& tmpIndex : indexes) {
        auto itemData = getModel()->getItemByRow(tmpIndex.row());
        if (!itemData) {
            continue;
        }
        pageIndexes.push_back(itemData->pageIndex);
    }
    if (pageIndexes.size() > 0) {
        std::sort(pageIndexes.begin(), pageIndexes.end());
        emit extractPages(pageIndexes);
    }
}

void PageBasicView::pageCut(int curPageIndex, const std::vector<int> &pageIndexes)
{
    if (getModelCount() <= 1) {
        return;
    }
    if(pageIndexes.size() == getModel()->rowCount()){
        MessageWidget win({tr("You cannot delete all pages. At least one page must remain.")},
                          {tr("OK"),"",""});
        win.resetIconImage(MessageWidget::IT_Warn);
        win.exec();
    } else {
        pageCopy(curPageIndex, pageIndexes);
        std::vector<CommandPageData> deletePageDataList;
        for(int index : pageIndexes){
            if(auto itemData = getModel()->getItemByRow(index)){
                PagePointer pagePointer = getPdfPageMgrView()->getPageManager()->pageAtIndex(index);
                deletePageDataList.push_back(CommandPageData(pagePointer,index,itemData->realWidth,
                                                                           itemData->realHeight));
            }
        }
        getPdfPageMgrView()->doDeletePage(deletePageDataList);
    }
}

void PageBasicView::pageCopy(int curPageIndex, const std::vector<int> &pageIndexes)
{
    QJsonArray jsonArray;
    for(int index : pageIndexes){
        if(auto itemData = getModel()->getItemByRow(index)){
            QJsonObject jsonObj;
            jsonObj.insert("width", itemData->realWidth);
            jsonObj.insert("height", itemData->realHeight);
            jsonArray.append(jsonObj);
        }
    }
    QByteArray formatPageInfo = QJsonDocument(jsonArray).toJson(QJsonDocument::Compact);
    getPdfPageMgrView()->doCopyPage(curPageIndex, pageIndexes, formatPageInfo);
}

void PageBasicView::pageDuplicate(int curPageIndex, const std::vector<int> &pageIndexes)
{
    std::vector<CommandPageData> insertPageDataList;
    int i = 0;
    for(int index : pageIndexes){
        PrePageItemData *itemData = getModel()->getItemByRow(index);
        if (!itemData) {
            continue;
        }
        i++;
        insertPageDataList.push_back(CommandPageData(nullptr,curPageIndex + i,
                                    itemData->realWidth, itemData->realHeight));
    }
    getPdfPageMgrView()->doDuplicatePage(curPageIndex, pageIndexes, insertPageDataList);
}

void PageBasicView::updatePageIndex()
{
    for (int i = 0; i < getModel()->rowCount(); i++) {
        if (auto item = getModel()->getItemByRow(i)) {
            item->pageIndex = i;
        }
    }
}

int PageBasicView::getModelCount()
{
    return getModel()->rowCount();
}

std::vector<int> PageBasicView::getVisiblePage()
{
    std::vector<int> pageIndex = {};
    int firstIndex,lastIndex;
    int findAreaWidth = mBasicViewMode == PreViewMode?0:ViewItemSpace+20;
    firstIndex = indexAt(QPoint(0,0)).row();
    lastIndex = indexAt(QPoint(findAreaWidth,height())).isValid()?(indexAt(QPoint(findAreaWidth,height()))).row()+1:getModel()->rowCount();
    for(int i=firstIndex;i<=lastIndex;i++){
        pageIndex.push_back(i);
    }
    return pageIndex;
}

bool PageBasicView::isDraging()
{
    return mInDrag;
}

PageBasicView::ZoomStatus PageBasicView::getZoomStatus()
{
    return mZoomStatus;
}

PDFPageManager* PageBasicView::pageManager() const {
    if (!mPageView || !mPageView->getPdfEditor() || !mPageView->getPdfEditor()->getPageEngine())
    {
        return nullptr;
    }
    auto engine = mPageView->getPdfEditor()->getPageEngine();
    if (!engine->dataCenter() || !engine->dataCenter()->doc())
    {
        return nullptr;
    }
    return engine->dataCenter()->doc()->pageManager();
}

void PageBasicView::zoomChange(ZoomStatus status)
{
    mZoomStatus = status;
    emit currentZoomStatus(status);
}

PdfPageMgrView *PageBasicView::getPdfPageMgrView()
{
    return mPdfPageMgrView;
}

void PageBasicView::getInsertPosAndLastItem(int *insertIndex, PrePageItemData **lastItemData)
{
    // 插入index
    int lastIndex = 0;
    auto indexes = getSelectedIndexs();
    if(indexes.size() <= 0){
        lastIndex = getModel()->rowCount() - 1;
        *lastItemData = getModel()->getItemByRow(lastIndex);
    } else {
        lastIndex = indexes.at(indexes.size() - 1).row();
        *lastItemData = getModel()->getItemByRow(lastIndex);
    }
    *insertIndex = lastIndex + 1;
}

void PageBasicView::insertEmptyPage()
{
    int insertIndex = 0;
    PrePageItemData *itemData = nullptr;
    getInsertPosAndLastItem(&insertIndex, &itemData);
    if (!itemData) {
        return;
    }

    std::vector<CommandPageData> insertPageDataList;
    insertPageDataList.push_back(CommandPageData(nullptr, insertIndex, itemData->realWidth, itemData->realHeight));
    getPdfPageMgrView()->doInsertEmptyPage(insertPageDataList);

    makeNewItemVisiable();
}

void PageBasicView::makeNewItemVisiable()
{
    bool needScroll = false;
    auto indexes = getSelectedIndexs();
    if (verticalScrollBar()->minimum() != verticalScrollBar()->maximum()) {
        needScroll = true;
    }
    // 需要在beginInsertRows后scroll
    if (needScroll) {
        QTimer::singleShot(1500, this, [=] {
            if (indexes.size() > 0) {
                scrollTo(indexes.at(indexes.size() - 1));
            } else {
                scrollToBottom();
            }
        });
    }
}
void PageBasicView::insertPagesByPath(const QString &path)
{
    int insertIndex = 0;
    PrePageItemData *itemData = nullptr;
    getInsertPosAndLastItem(&insertIndex, &itemData);
    QTimer::singleShot(10, this, [=] {
        clearSelection();
        getPdfPageMgrView()->doInsertPageByPath(insertIndex, path);
    });

    makeNewItemVisiable();
}

void PageBasicView::replacePagesByPath(const QString &path)
{
    std::vector<CommandPageData> deletePageDataList;
    if (!getSelectedPageList(&deletePageDataList, true)) {
        return;
    }

    QTimer::singleShot(10, this, [=] {
        clearSelection();
        getPdfPageMgrView()->doReplacePagesByPath(deletePageDataList, path);
    });
    makeNewItemVisiable();
}

void PageBasicView::pageDelete()
{
    if (getModelCount() <= 1) {
        return;
    }
    std::vector<CommandPageData> deletePageDataList;
    if(!getSelectedPageList(&deletePageDataList)){
        MessageWidget win({tr("You cannot delete all pages. At least one page must remain.")},
                          {tr("OK"),"",""});
        win.resetIconImage(MessageWidget::IT_Warn);
        win.exec();
    } else {
        getPdfPageMgrView()->doDeletePage(deletePageDataList);
    }
}

void PageBasicView::pageRotation(int rotation)
{
    auto pageIndexs = getSelectedIndexs();
    std::vector<int> pageIndexes;
    for(const auto& index:pageIndexs){
        pageIndexes.push_back(index.row());
    }
    pageRotation(pageIndexes, rotation);
}

void PageBasicView::pageRotation(const std::vector<int> &pageIndexes, int rotation)
{
    const qreal rotate = (rotation == PdfPageMgrView::LeftRotate?-90:90);
    std::list<RotatePageData> rotateList;
    for(const auto& index:pageIndexes){
        PrePageItemData *itemData = getModel()->getItemByRow(index);
        if (!itemData) {
            continue;
        }
        qreal oldRotate = itemData->rotate;
        qreal newRotate = oldRotate + rotate;
        rotateList.push_back(RotatePageData(oldRotate,newRotate,itemData->pageIndex));
    }
    if (rotateList.size() > 0) {
        getPdfPageMgrView()->doRotatePage(rotateList);
    }
}

QList<QModelIndex> PageBasicView::getSelectedIndexs()
{
    return selectedIndexes();
}

bool PageBasicView::getSelectedPageList(std::vector<CommandPageData> *selectPageDataList, bool replacePage)
{
    auto indexes = getSelectedIndexs();
    if(!replacePage && indexes.size() == getModel()->rowCount()){
        return false; // 不能全删
    }

    for(const auto& tmpIndex:indexes){
        PrePageItemData *itemData = getModel()->getItemByRow(tmpIndex.row());
        if (!itemData) {
            continue;
        }
        int pageIndex = itemData->pageIndex;
        PagePointer pagePointer = getPdfPageMgrView()->getPageManager()->pageAtIndex(pageIndex);
        (*selectPageDataList).push_back(CommandPageData(pagePointer,pageIndex,itemData->realWidth,
                                                                   itemData->realHeight));
    }
    return true;
}

bool PageBasicView::drawSelectedStyle()
{
    if(curShowPlayerView()) {
        return true;
    }
    auto pageViewMode = mPageView->getPdfEditor()->getCurMode();
    return !(pageViewMode == VM_Reader ||
             pageViewMode == VM_Annot ||
             pageViewMode == VM_Edit ||
             pageViewMode == VM_Field ||
             pageViewMode == VM_Redact) || viewModel() == PageBasicView::MgrViewMode ;
}

