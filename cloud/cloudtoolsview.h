#ifndef CLOUDTOOLSVIEW_H
#define CLOUDTOOLSVIEW_H

#include <QWidget>
#include <QFrame>
#include <QImage>
#include <QString>
#include <QFrame>
#include <QPaintEvent>
#include <QProgressBar>
#include <QTimer>
#include <QComboBox>
#include <QStackedWidget>
#include <QGraphicsDropShadowEffect>
#include "./src/toolsview/basemodelview/basefiledefine.h"
#include "cloudpredefine.h"
#include "./src/basicwidget/lineedit.h"
#include "floatingwidget.h"

class UComboBox;
class CloudListView;
class QPushButton;
class QStackedWidget;
class QGraphicsOpacityEffect;
class QLabel;
class QToolButton;
class QVBoxLayout;
class CShowTipsBtn;
class QButtonGroup;
class UploadFileWidget;
class CloudUploadsToolsView;
class CloudDownloadsToolsView;
class MainWindow;
class PreferencesMgr;
class CheckComboBox;
class CloudToolsView : public QWidget
{
    Q_OBJECT
public:
    enum MainViewType {
        CloudView = 0,
        RecycleView,
    };
    enum SubViewType {
        CloudList = 0,
        UploadsList,
        DownloadsList,
    };
    enum OutterViewType {
        UnloginView = 0,
        LoginView,
    };
    enum ViewType {
        EmptyFileView = 0,
        FileView,
    };
    explicit CloudToolsView(MainWindow *mainWindow, QWidget *parent = nullptr);
    virtual ~CloudToolsView() override;

    void initView();
    QFrame* createCloudView();
    QFrame* createToolBar();
    QFrame* createUnloginWidget();
    QFrame* createEmptyFileWidget();
    QFrame* createBottomToolWidget();
    QFrame* createFileMangerFrame();

    void showStackWidget(ViewType viewType);
    bool isInListView(const QString &filePath);
    void setDiskInfo(const QString &text);
    void setDiskCapacity(double ratio);

    void clearListView();
    void locateItem(const CloudFilePropInfo &fileInfo);
private:
    void toolsEnable(bool enable);
    void startLoadingAnimation();
    void removeCloudItem(BaseFileDetailData *data);
protected:
    virtual void dragEnterEvent(QDragEnterEvent *event) override;  //拖拽进入
    virtual void dropEvent(QDropEvent *event) override;//释放拖拽的文件
    virtual void showEvent(QShowEvent *event) override;
    virtual bool eventFilter(QObject *watched, QEvent *event);
signals:
    void openFile(const QString &filePath, qint64 fileId);
    void recycleClicked();
    void addToStarred(BaseFileDetailData *data);
    void viewModeChange(int viewMode);
public slots:
    void onViewTypeChanged(int viewMode);
    void onOpenDialogToUpload();
    void onOpenFile(BaseFileDetailData *data);
    void onOpenMenu(BaseFileDetailData *data);
    void onGetCloudFileList(const CloudFilePropInfos &fileInfos);
    void onFileUploadResult(const CloudFilePropInfo &fileInfo); // 最终完成
    void onFileOssUploadResult(bool success, const OssUploadParam &param);
    void onFileUploadProgress(const OssUploadParam &param);
    void onFileDownloadResult(bool success, const OssDownloadParam &param);
    void onDownloadThumbnailDone(const OssDownloadParam &param);
    void onFileDownloadProgress(const OssDownloadParam &param);

    void onUpdateDiskInfo(const CloudDiskInfo &diskInfo);
    void onRenameFileFinished(const RenameStruct &renameStruct);
    void onMoveToRecycleFinished(const DeleteOprStruct &deleteOpr);

    void onCloudStarredFinished(const DeleteOprStruct &deleteOpr);
    void onCloudUnstarredFinished(const DeleteOprStruct &deleteOpr);
    // 上传占用item,可显示进度，不可编辑
    void onInsertListView(const CloudFilePropInfo &fileInfo);
    void onUpdateListItem(const CloudFilePropInfo &fileInfo);
    void onRemoveListItem(const QString &filePath);
    void onCloudFileError(qint64 fileId, int code);
    void onRemoveItem(qint64 fileId);
    void onRemoveItemByPath(const QString &filePath);
    void onTransferState(qint64 fileId, TransferState state);
    void onRefresh();
    void onLoadingTimeout();
    void onAnimationTimeout();
    void stopAnimation();
    void stopAnimationImpl();

    void onUploadWidthNewCloudVersion(const CloudFilePropInfo &fileInfo, bool *continueUpload);
    void onDownloadWidthNewCloudVersion(const CloudFilePropInfo &fileInfo, bool *continueDownload);

    void onLogin();
    void onLogout();

    void onStartUploadTask(const OssUploadParam &param);
private:
    MainWindow *mMainWindow;
    CloudListView* mListView;
    CloudUploadsToolsView* mUploadsToolsView;
    CloudDownloadsToolsView* mDownloadsToolsView;
    PreferencesMgr *mReferences;

    CheckComboBox* mSortCombobox;
    CShowTipsBtn* mListViewBtn;
    CShowTipsBtn* mThumbnailBtn;
    CShowTipsBtn* mRemoveBtn;
    CShowTipsBtn* mRefreshBtn;
    QStackedWidget* mStackedWidget;
    QStackedWidget* mOutterStackedWidget;
    QStackedWidget* mMainStackedWidget;
    QStackedWidget* mTypeStackedWidget;

    QLabel* mIconLabel;
    QLabel* mTipsLabel;
    QLabel* mTipsLabel1;
    UComboBox* mTypeCombobox;
    QLabel *mDiskInfoLabel;
    QProgressBar *mDiskCapacityBar;
    CShowTipsBtn *mBuyBtn;

    QLabel *mAnimationLabel;
    bool mCanStop;
    mutable bool mMinTimeAnimation;
    QTimer mAnimationTimer;
    QTimer mTimer;
    int mIndex;

    bool mFirstLoad;
    SortType mSortType;
};

class UploadFileWidget : public QWidget
{
    Q_OBJECT
public:
    UploadFileWidget(QWidget *parent = nullptr);
    void initView();

private:
    QStringList getFilePathList();
    QGraphicsDropShadowEffect* getShadowEffect();
    void runAnimation(bool left);

protected:
    void enterEvent(QEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void showEvent(QShowEvent *event) override;

private:
    QPushButton* mOpenBtn = nullptr;
    QLabel* mExpandLab = nullptr;
    QPoint mOldPoint;
    QGraphicsDropShadowEffect *mShadow;
signals:
    void openDir();
};

class SortItemCell : public QWidget
{
    Q_OBJECT
public:
    explicit SortItemCell(int type, const QString &text, QWidget *parent = nullptr);
    void setChecked(bool check);
    bool isChecked();
    void setHoverStyle(bool hover);
    int type();
    QString text();
    void setText(const QString &text);
protected:
    virtual bool eventFilter(QObject *watched, QEvent *event);
    void mousePressEvent(QMouseEvent *event);
signals:
    void checked();
private:
    int mType;
    QString mText;
    QLabel* mBGLabel;
    QLabel* mTextLable;
    QLabel* mIconLable;
    bool mChecked;
};

class SortTypeWidget : public FloatingWidget
{
    Q_OBJECT
public:
    explicit SortTypeWidget(QWidget *parent = nullptr);
private:
    void initView();
signals:
    void sortInfo(int type, int direction);
    void textChanged(const QString &text);
private slots:
    void onTypeChecked();
    void onDirectionChecked();
private:
    QList<SortItemCell *> mTypeGroup;
    QList<SortItemCell *> mDirectionGroup;
    int mCheckedType;
    int mCheckedDirection;
};

class CheckComboBox : public QComboBox
{
    Q_OBJECT
public:
    explicit CheckComboBox(QWidget *parent=Q_NULLPTR);
    void setCurrentText(const QString &text);
protected:
    virtual void showPopup();
    virtual void hidePopup();
signals:
    void sortTypeChanged(int type);
private:
    LineEdit* mLineEdit;
    SortTypeWidget* mView;
};
#endif // CLOUDTOOLSVIEW_H
