#ifndef RECYCLEBINTOOLSVIEW_H
#define RECYCLEBINTOOLSVIEW_H

#include <QWidget>
#include <QFrame>
#include <QPushButton>
#include <QLabel>
#include <QTimer>
#include "../cloudpredefine.h"
#include "./src/toolsview/basemodelview/basefiledefine.h"


class UComboBox;
class ClickLabel;
class CShowTipsBtn;
class QStackedWidget;
class RecycleBinListView;
class MainWindow;
class RecycleBinToolsView : public QWidget
{
    Q_OBJECT
public:
    enum ViewType {
        EmptyFileView = 0,
        FileView,
    };
    explicit RecycleBinToolsView(MainWindow*mainWindow, QWidget *parent = nullptr);
    ~RecycleBinToolsView();
    void initView();
    QFrame* createToolBar();
    QFrame* createEmptyFileWidget();
    QFrame* createBottomToolWidget();
    void showStackWidget(ViewType viewType);

    void clearListView();
private:
    void startLoadingAnimation();
signals:
    void openFile(const QString&); // 点击打开文件

    void returnClicked();
public slots:
    void onViewTypeChanged();
    void onClearList();
    void onOpenMenu(BaseFileDetailData *data);
    void onDownloadThumbnailDone(const OssDownloadParam &param);
    void onGetCloudFileList(const CloudFilePropInfos &fileInfos);
    void onReturnFromRecycleFinished(const DeleteOprStruct &deleteOpr);
    void onDeleteFromRecycleFinished(const DeleteOprStruct &deleteOpr);

    void onRefresh();
    void onLoadingTimeout();
    void onAnimationTimeout();
    void stopAnimation();
    void stopAnimationImpl();
protected:
    virtual void showEvent(QShowEvent *event) override;
    virtual bool eventFilter(QObject *watched, QEvent *event);

private:
    MainWindow* mMainWindow;
    RecycleBinListView* mListView;
    UComboBox* mSortCombobox;
    CShowTipsBtn* mListViewBtn;
    CShowTipsBtn* mThumbnailBtn;
    CShowTipsBtn* mRefreshBtn;
    QStackedWidget* mStackedWidget;

    QLabel* mIconLabel;
    QLabel* mTipsLabel;
    QLabel* mTipsLabel1;
    QLabel* mNameLabel;
    QPushButton *mRecycleBinBtn;
    ClickLabel *mRecycleLabel;

    QLabel *mAnimationLabel;
    bool mCanStop;
    mutable bool mMinTimeAnimation;
    QTimer mAnimationTimer;
    QTimer mTimer;
    int mIndex;
};

#endif // RECYCLEBINTOOLSVIEW_H
