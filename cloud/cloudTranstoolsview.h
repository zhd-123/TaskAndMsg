#ifndef CLOUDTRANSTOOLSVIEW_H
#define CLOUDTRANSTOOLSVIEW_H

#include <QWidget>
#include <QFrame>
#include <QMap>
#include "./src/toolsview/basemodelview/basefiledefine.h"
#include "cloudpredefine.h"

class QLabel;
class QComboBox;
class QPushButton;
class QStackedWidget;
class CloudTransListView;
class CShowTipsBtn;
class MainWindow;
class CloudUploadsToolsView : public QWidget
{
    Q_OBJECT
public:
    enum ViewType {
        EmptyFileView = 0,
        FileView,
    };
    explicit CloudUploadsToolsView(MainWindow* mainWindow, QWidget *parent = nullptr);
    ~CloudUploadsToolsView();
    void initView();
    QFrame* createEmptyFileWidget();
    QFrame* createCloudView();
    void showStackWidget(ViewType viewType);
    void addUploadItem(const CloudFilePropInfo &fileInfo);
    void removeUploadItem(BaseFileDetailData *data);
    void setViewMode(int viewMode);
    void refreshList();
    void loadUploadTaskList();
    void removeListItem(qint64 fileId);
protected:
   virtual void showEvent(QShowEvent *event) override;
signals:
    void enableFreshButton(bool show);
    void enableViewModeButton(bool enable);
    void removeItem(qint64 fileId);
    void removeItemByPath(const QString &filePath);

public slots:
    void onOpenFile(BaseFileDetailData *data);
    void onOpenMenu(BaseFileDetailData *data);
    void onFileUploadProgress(const OssUploadParam &param);
    void onOssUploadResult(bool success, const OssUploadParam &param);
    void onTransferState(qint64 fileId, TransferState state);
    void onInsertListView(const CloudFilePropInfo &fileInfo);
    void onUpdateListItem(const CloudFilePropInfo &fileInfo);
    void onRemoveListItem(const QString &filePath);
private:
    MainWindow* mMainWindow;
    CloudTransListView* mListView;
    QStackedWidget* mStackedWidget;
    // 记录失败的任务
    QMap<qint64, QString> mFailFileConfigMap;
    QMap<qint64, QString> mCacheFileConfigMap;
    bool mRetryUpload;

    QLabel* mIconLabel;
    QLabel* mTipsLabel;
    QLabel* mTipsLabel1;
    QLabel* mNameLabel;
};


class CloudDownloadsToolsView : public QWidget
{
    Q_OBJECT
public:
    enum ViewType {
        EmptyFileView = 0,
        FileView,
    };
    explicit CloudDownloadsToolsView(MainWindow* mainWindow, QWidget *parent = nullptr);
    ~CloudDownloadsToolsView();
    void initView();
    QFrame* createEmptyFileWidget();
    QFrame* createCloudView();
    void showStackWidget(ViewType viewType);
    void addDownloadItem(const CloudFilePropInfo &fileInfo, bool saveAsRecord);
    void removeDownloadItem(BaseFileDetailData *data);
    void setViewMode(int viewMode);
    void refreshList();
    void loadDownloadTaskList();
protected:
   virtual void showEvent(QShowEvent *event) override;

signals:
    void enableFreshButton(bool enable);
    void enableViewModeButton(bool enable);
public slots:
    void onOpenFile(BaseFileDetailData *data);
    void onOpenMenu(BaseFileDetailData *data);
    void onFileDownloadProgress(const OssDownloadParam &param);
    void onOssDownloadResult(bool success, const OssDownloadParam &param);
    void onTransferState(qint64 fileId, TransferState state);
    void onCloudFileError(qint64 fileId, int code);
private:
    MainWindow* mMainWindow;
    CloudTransListView* mListView;
    QStackedWidget* mStackedWidget;
    // 记录失败的任务
    QMap<qint64, QString> mFailFileConfigMap;
    QMap<qint64, QString> mCacheFileConfigMap;

    QLabel* mIconLabel;
    QLabel* mTipsLabel;
    QLabel* mTipsLabel1;
    QLabel* mNameLabel;
};

#endif // CLOUDTRANSTOOLSVIEW_H
