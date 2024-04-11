#include "pdfpagemgrlistview.h"
#include <QDebug>
#include <QSortFilterProxyModel>
#include <QKeyEvent>
#include <QScrollBar>
#include <QTimer>
#include "./src/basicwidget/prelistview/baisc/pagebasicitemdatadef.h"
#include <QUndoCommand>
#include "pagecommands.h"
#include "./src/pageengine/pagectrl.h"
#include "./src/pageengine/pageengine.h"
#include "./src/pageengine/rendermanager.h"
#include "./src/pageengine/priv/pagemgrctrlpriv.h"
#include "./src/pageengine/DataCenter.h"
#include "./src/pageengine/pdfcore/PDFDocInfo.h"
#include <QFileDialog>
#include <QDesktopServices>
#include <QPainter>
#include <QDrag>
#include <QMimeData>
#include <QStandardPaths>
#include <QApplication>
#include <QPainterPath>
#include <QClipboard>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include "./src/util/guiutil.h"
#include "./src/util/loginutil.h"
#include "./src/util/configutil.h"
#include "./src/mainview.h"
#include "./src/mainwindow.h"
#include "pdfpagemgrview.h"
#include "spdfview.h"
#include "./src/pageengine/pdfcore/PDFPageManager.h"
#include "./src/basicwidget/prelistview/baisc/pagebasicitemmodel.h"
#include "./src/basicwidget/prelistview/baisc/pagebasicitemdelegate.h"
#include "./src/filemgr/fileunlockwin.h"
#include "./src/pdfeditor/pdfpageview.h"
#include "./src/usercenter/limituse/limitusemgr.h"


PdfPageMgrListView::PdfPageMgrListView(PdfPageMgrView *pdfPageMgrView, PdfPageView* pdfPageView, QWidget* parent)
    : PageBasicView(pdfPageMgrView, BasicViewMode::MgrViewMode,parent, pdfPageView)
{
    setObjectName("PdfPageViewMgr");
    mHighlightedRow = -1;
    setMouseTracking(true);
    setSelectionRectVisible(false);
    setAcceptDrops(true);
    setDragDropMode(QAbstractItemView::InternalMove);
    setDragEnabled(true);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    connect(RenderManager::instance(), &RenderManager::cachesUpdated, this, [this](PDFPageManager* pageManager, int pageIndex) {
        if (this->pageManager() != pageManager)
        {
            return;
        }
        if (pageIndex >= 0 && pageIndex < model()->rowCount())
        {
            update(model()->index(pageIndex, 0));
        }
        else
        {
            update();
        }
    });
}

void PdfPageMgrListView::keyPressEvent(QKeyEvent *e)
{
    switch (e->key()) {
    case Qt::Key_A:
        update();
        break;
    case Qt::Key_Left:
    case Qt::Key_Up:
    {
        if (selectedIndexes().size() > 0) {
            QRect rect = visualRect(selectedIndexes().at(0));
            if (verticalScrollBar()->minimum() != verticalScrollBar()->maximum() && verticalScrollBar()->value() > verticalScrollBar()->minimum()
                     && (rect.y() - 20) < viewport()->rect().y()) {
                verticalScrollBar()->setValue(verticalScrollBar()->value() - verticalScrollBar()->pageStep());
            }
        }
    }
        break;
    case Qt::Key_Right:
    case Qt::Key_Down:
    {
        if (selectedIndexes().size() > 0) {
            QRect rect = visualRect(selectedIndexes().at(0));
            if (verticalScrollBar()->minimum() != verticalScrollBar()->maximum() && verticalScrollBar()->value() < verticalScrollBar()->maximum()
                    && (rect.y() + rect.height() + 20) > (viewport()->rect().y() + viewport()->rect().height())) {
                verticalScrollBar()->setValue(verticalScrollBar()->value() + verticalScrollBar()->pageStep());
            }
        }
    }
        break;
    }
    PageBasicView::keyPressEvent(e);
}

void PdfPageMgrListView::mousePressEvent(QMouseEvent *e)
{
    PageBasicView::mousePressEvent(e);
    if (mPressIn && e->button() & Qt::LeftButton) {
        QModelIndex modelIndex = indexAt(e->pos());
        emit curIndexChanged(modelIndex.isValid()?modelIndex:QModelIndex());
    }
}

void PdfPageMgrListView::mouseMoveEvent(QMouseEvent *e)
{
    PageBasicView::mouseMoveEvent(e);
}

void PdfPageMgrListView::mouseReleaseEvent(QMouseEvent *e)
{
    PageBasicView::mouseReleaseEvent(e);
}

void PdfPageMgrListView::mouseDoubleClickEvent(QMouseEvent *e)
{
    PageBasicView::mouseDoubleClickEvent(e);
    if (e->button() & Qt::LeftButton) {
        if(indexAt(e->pos()).isValid()) {
            emit goToPage(indexAt(e->pos()).row());
        }
    } else if (e->button() & Qt::RightButton) {

    }
}

void PdfPageMgrListView::paintEvent(QPaintEvent* e)
{
    PageBasicView::paintEvent(e);
    if(mMousePressMove){
        QRect selectRect = QRect(qMin(mStartPos.x(),mEndPos.x()),
                                 qMin(mStartPos.y(),mEndPos.y()),
                                 fabs(mStartPos.x() - mEndPos.x()),
                                 fabs(mStartPos.y() - mEndPos.y()));
        QPainter painter(viewport());
        painter.setPen(QPen(QColor("#964FF2")));
        painter.setBrush(QColor(192, 143, 255, 100));
        painter.drawRect(selectRect);
    }
}

bool PdfPageMgrListView::event(QEvent *e)
{
    QWheelEvent *wheelEvent = dynamic_cast<QWheelEvent *>(e);
    if (wheelEvent) {
        PageBasicView::wheelEvent(wheelEvent);
    }
    return PageBasicView::event(e);
}

void PdfPageMgrListView::pageNotify(PageNotifyData* notifyData)
{
    switch (notifyData->notifyType) {
    case PageNotifyType::MarkupAdd:
    case PageNotifyType::MarkupDelete:
    case PageNotifyType::MarkupUpdate:
    case PageNotifyType::EditUpdate:
    case PageNotifyType::EditDelete:
    case PageNotifyType::PageWatermark:
    case PageNotifyType::PageCrop:
        break;

    case PageNotifyType::Insert:
    case PageNotifyType::Delete:
    case PageNotifyType::Update: {
        // 调整边距
        takeZoomTask(1.0);
        break;
    }
    }
}

void PdfPageMgrListView::selectPageRange(const std::vector<int>& pagesRange)
{
    clearSelection();
    if(pagesRange.empty())
        return ;

    for(auto index:pagesRange){
        selectionModel()->select(getModel()->index(index,0),QItemSelectionModel::Select);
    }
    update();
}
void PdfPageMgrListView::takeSelectItems(PdfPageMgrView::SelectType type)
{
    int count = getModel()->rowCount();
    QItemSelection result;
    if (type == PdfPageMgrView::OddPages) {
        for (int i = 0; i < count; i += 2) {
            result.merge(QItemSelection(getModel()->index(i, 0), getModel()->index(i, 0)), QItemSelectionModel::Select);
        }
    }
    else {
        for (int i = 1; i < count; i += 2) {
            result.merge(QItemSelection(getModel()->index(i, 0), getModel()->index(i, 0)), QItemSelectionModel::Select);
        }
    }
    selectionModel()->select(result, QItemSelectionModel::Select);
}
void PdfPageMgrListView::selectSpecifWidthRotiaPage(PdfPageMgrView::SelectType type)
{
    int count = getModel()->rowCount();
    bool wBigh = type == PdfPageMgrView::PortraitPages?false:true;

    QItemSelection result;
    for(int i = 0; i < count; i++){
        PrePageItemData* pageItemData = getModel()->getItemByRow(i);
        if(wBigh){
            if(pageItemData->realWidth >= pageItemData->realHeight){
                result.merge(QItemSelection(getModel()->index(pageItemData->pageIndex, 0), getModel()->index(pageItemData->pageIndex, 0)), QItemSelectionModel::Select);
            }
        }
        else{
            if(pageItemData->realWidth < pageItemData->realHeight){
                result.merge(QItemSelection(getModel()->index(pageItemData->pageIndex, 0), getModel()->index(pageItemData->pageIndex, 0)), QItemSelectionModel::Select);
            }
        }
    }
    selectionModel()->select(result, QItemSelectionModel::Select);
}


void PdfPageMgrListView::onSelectTypeChanged(int type)
{
    clearSelection();
    switch (type) {
        case PdfPageMgrView::OddPages:
        case PdfPageMgrView::EvenPages:
            takeSelectItems((PdfPageMgrView::SelectType)type);
        break;
        case PdfPageMgrView::PortraitPages:
        case PdfPageMgrView::LandscapePages:
            selectSpecifWidthRotiaPage((PdfPageMgrView::SelectType)type);
        break;
        case PdfPageMgrView::AllPages:
            selectAll();
        break;
    }
    update();
}

void PdfPageMgrListView::currentChanged(const QModelIndex &current, const QModelIndex &previous)
{
    PageBasicView::currentChanged(current, previous);
}

void PdfPageMgrListView::setCurPageIndex(int pageIndex)
{
    PrePageItemData *itemData = getModel()->getItemByRow(currentIndex().row());
    if (!itemData || itemData->pageIndex != pageIndex) {
        blockSignals(true);
        PageBasicView::setCurrentIndex(getModel()->currentIndexByRow(pageIndex));
        clearSelection();
        blockSignals(false);
    }
}

void PdfPageMgrListView::showEvent(QShowEvent *e)
{
    setFocus();
    PageBasicView::showEvent(e);
}

void PdfPageMgrListView::wheelEvent(QWheelEvent *e)
{
    PageBasicView::wheelEvent(e);
}

void PdfPageMgrListView::selectionChanged(const QItemSelection &selected, const QItemSelection &deselected)
{
    if (!isVisible()) {
        return;
    }
    emit selectCountChanged(selectedIndexes().count());
}
