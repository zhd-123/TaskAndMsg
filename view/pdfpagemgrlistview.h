#ifndef PDFPAGEMGRLISTVIEW_H
#define PDFPAGEMGRLISTVIEW_H

#include <QListView>
#include <QButtonGroup>
#include <QStandardItem>
#include "./src/basicwidget/prelistview/baisc/pagebasicview.h"
#include "./src/notifyinterface.h"
#include "pdfpagemgrview.h"

class QUndoStack;
class PdfPageView;

typedef void* PagePointer;
class PageBasicItemModel;
class PdfPageMgrListView : public PageBasicView, public NotifyInterface
{
    Q_OBJECT
public:
    explicit PdfPageMgrListView(PdfPageMgrView *pdfPageMgrView, PdfPageView* pdfPageView, QWidget* parent = nullptr);
    virtual void pageNotify(PageNotifyData* notifyData) override;
    void setCurPageIndex(int pageIndex);

    void selectPageRange(const std::vector<int>&);
    void currentChanged(const QModelIndex &current, const QModelIndex &previous) override;

private:
    void takeSelectItems(PdfPageMgrView::SelectType);
    void selectSpecifWidthRotiaPage(PdfPageMgrView::SelectType);
protected:
    virtual void mousePressEvent(QMouseEvent *e) override;
    virtual void mouseDoubleClickEvent(QMouseEvent *e) override;
    virtual void mouseMoveEvent(QMouseEvent *e) override;
    virtual void mouseReleaseEvent(QMouseEvent *e) override;
    virtual void paintEvent(QPaintEvent* e) override;

    virtual bool event(QEvent* e) override;
    virtual void showEvent(QShowEvent* e) override;
    virtual void wheelEvent(QWheelEvent *e) override;
    virtual void selectionChanged(const QItemSelection &selected, const QItemSelection &deselected) override;
    virtual void keyPressEvent(QKeyEvent *e) override;
signals:
    void curIndexChanged(const QModelIndex);
    void selectCountChanged(int);
    void goToPage(int);
public slots:
    void onSelectTypeChanged(int type);
private:

};

#endif // PDFPAGEMGRLISTVIEW_H
