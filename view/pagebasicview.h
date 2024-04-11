#ifndef PAGEBASICVIEW_H
#define PAGEBASICVIEW_H

#include <QListView>
#include <QButtonGroup>
#include <QStandardItem>
#include "pagebasicitemdelegate.h"
#include "./src/basicwidget/basicitemview.h"
#include <QPushButton>
#include "./src/pdfeditor/pdfpageview.h"
#include "./src/pdfpagemgr/pagecommands.h"


class ThumbnailPageView;
class PdfPageMgrView;
class QSortFilterProxyModel;
class PageBasicItemModel;
class PdfPageView;

class PageBasicView : public  AbstractItemViewType<QListView>
{
    Q_OBJECT
public:
    enum BasicViewMode {
        PreViewMode = 0,
        MgrViewMode
    };
    enum ZoomStatus {
        ZOOMMAX = 0,
        ZOOMMIN,
        NORMAL
    };
    explicit PageBasicView(PdfPageMgrView *pdfPageMgrView,BasicViewMode mode,QWidget* parent = nullptr,PdfPageView* pageView = nullptr);

    void initView();
    void initViewFlag();
    QSize getItemSize();
    void setRotate(int,bool rotation_left);
    std::vector<int> getVisiblePage();
    virtual void setPagesDetail(const std::vector<PageDetail>&);
    void takeCloseTask();

    void takeZoomTask(bool zoomIn);
    void takeZoomTask(double scaleRatio);

    void deletePage(int pageIndex);
    void deletePages(const QList<int> &pageIndexs);
    void insertPage(int pageIndex, int realWidth, int realHeight);
    void insertPages(const QList<PrePageItemData *> &pageItemDatas);
    void pageCut(int curPageIndex, const std::vector<int> &pageIndexes);
    void pageCopy(int curPageIndex, const std::vector<int> &pageIndexes);
    void pageDuplicate(int curPageIndex, const std::vector<int> &pageIndexes);
    void extract();

    void updatePageIndex();
    BasicViewMode viewModel() { return mBasicViewMode; }
    PageBasicItemModel* getModel() { return mPageModel; }
    int getModelCount();

    bool isDraging();
    ZoomStatus getZoomStatus();
    void zoomChange(ZoomStatus status);

    bool curShowPlayerView() { return isPlayer; }
    void setCurShowPlayerView(bool value) { isPlayer = value; }
    PdfPageMgrView *getPdfPageMgrView();
    int highlightedRow() const {return mHighlightedRow;}
    void scrollToShowItem(int index);

    void insertEmptyPage();
    void insertPagesByPath(const QString &path);
    void replacePagesByPath(const QString &path);
    void pageDelete();
    void pageRotation(int rotation);
    void pageRotation(const std::vector<int> &pageIndexes, int rotation);
    QList<QModelIndex> getSelectedIndexs();

    bool drawSelectedStyle();
protected:
    QSize calcItemWH(int realWidth, int realHeight);
    void getInsertPosAndLastItem(int *insertIndex, PrePageItemData **lastItemData);
    bool getSelectedPageList(std::vector<CommandPageData> *selectPageDataList, bool replacePage = false);
private:
    void makeNewItemVisiable();
protected:
    void wheelEvent(QWheelEvent *e) override;
    void showEvent(QShowEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    virtual void mousePressEvent(QMouseEvent *e) override;
    virtual void mouseMoveEvent(QMouseEvent *e) override;
    virtual void mouseReleaseEvent(QMouseEvent *e) override;
    virtual void startDrag(Qt::DropActions supportedActions) override; // 空方法有用

    class PDFPageManager* pageManager() const;
    void startUserDrag(QMouseEvent *e);
    void dragMove(QMouseEvent *e);
    void drop(QMouseEvent *e);
signals:
    void currentPageChanged(int);
    void currentZoomStatus(ZoomStatus status);
    void updateItem(int index);
    void marginChanged(int left, int top, int right, int bottom);
    void containerWidth(int *width);
    void pageCountChanged(int);
    void extractPages(const std::vector<int> &pageIndexes);

protected:
    PdfPageMgrView *mPdfPageMgrView;
    PageBasicItemDelegate *mPageBasicItemDelegate;                 //委托
    PageBasicItemModel *mPageModel;
    QMap<int,QStandardItem*> mItemsMap;
    BasicViewMode mBasicViewMode;
    PdfPageView* mPageView = nullptr;
    bool mInDrag;
    int mLastMargin;
    int mBeforeScaleWidth;
    ZoomStatus mZoomStatus;

    bool isPlayer = false;
    bool mMousePress;
    bool mMousePressMove;
    bool mPressIn;
    int mHighlightedRow;
    bool mSmoothScroll;
    QByteArray mDragMimeData;
    QPoint mStartPos;
    QPoint mEndPos;
};

#endif // PAGEBASICVIEW_H
