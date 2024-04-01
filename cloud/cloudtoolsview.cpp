#include "cloudtoolsview.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QComboBox>
#include <QDebug>
#include <QFile>
#include <QFrame>
#include <QDragEnterEvent>
#include <QMimeData>
#include <QJsonObject>
#include <QStackedWidget>
#include <QApplication>
#include <QDesktopWidget>
#include <QEasingCurve>
#include <QFileDialog>
#include <QStandardPaths>
#include <QPropertyAnimation>
#include <QWidgetAction>
#include <QtConcurrent>

#include "src/filemgr/preferencesmgr.h"
#include "./src/mainwindow.h"
#include "./src/util/guiutil.h"
#include "./src/util/loginutil.h"
#include "./src/util/historyfilesutil.h"
#include "./src/customwidget/chimgandtextbtn.h"
#include "./src/customwidget/colorbtn.h"
#include "cloudlistview.h"
#include "./src/filemgr/filerenamewidget.h"
#include "./src/style/stylesheethelper.h"
#include "./utils/settingutil.h"
#include "./src/basicwidget/scrollareawidget.h"
#include "./src/customwidget/sequenceframeanimation.h"
#include "./src/util/guiutil.h"
#include "./src/customwidget/menustyle.h"
#include "./src/util/configutil.h"
#include "signalcenter.h"
#include "cloudmanager.h"
#include "commonutils.h"
#include "netrequestmanager.h"
#include "transportcontroller.h"
#include "./src/usercenter/usercentermanager.h"
#include "./src/cloud/recyclebin/recyclebintoolsview.h"
#include "serializeconfig.h"
#include "cloudtranstoolsview.h"
#include "./src/util/loginutil.h"
#include "fileoperationtool.h"
#include "./src/usercenter/limituse/limitusemgr.h"
#include "./src/pageengine/pdfcore/PDFDocument.h"
#include "./src/util/gamanager.h"
#include "./src/basicwidget/messagewidget.h"
#include "../../../utils/settingutil.h"
#include "ucombobox.h"

CloudToolsView::CloudToolsView(MainWindow *mainWindow, QWidget *parent)
    : QWidget{parent}
    , mMainWindow(mainWindow)
    , mListView(nullptr)
    , mReferences(nullptr)
    , mFirstLoad(false)
    , mSortType(TimeDesc)
{
    setObjectName("CloudToolsView");

    initView();
    setAcceptDrops(true);
    connect(mMainWindow->getCloudManager(), &CloudManager::getCloudSpaceFileList, this, &CloudToolsView::onGetCloudFileList);
    connect(mMainWindow->getCloudManager(), &CloudManager::fileUploadSuccess, this, &CloudToolsView::onFileUploadResult);
    connect(mMainWindow->getCloudManager(), &CloudManager::fileOssUploadResult, this, &CloudToolsView::onFileOssUploadResult);
    connect(mMainWindow->getCloudManager(), &CloudManager::fileDownloadResult, this, &CloudToolsView::onFileDownloadResult);
    connect(mMainWindow->getCloudManager(), &CloudManager::updateDiskInfo, this, &CloudToolsView::onUpdateDiskInfo);
    connect(mMainWindow->getCloudManager(), &CloudManager::renameFileFinished, this, &CloudToolsView::onRenameFileFinished);
//    connect(mMainWindow->getCloudManager(), &CloudManager::moveToRecycleFinished, this, &CloudToolsView::onMoveToRecycleFinished);
    connect(mMainWindow->getCloudManager(), &CloudManager::cloudStarredFinished, this, &CloudToolsView::onCloudStarredFinished);
    connect(mMainWindow->getCloudManager(), &CloudManager::cloudUnstarredFinished, this, &CloudToolsView::onCloudUnstarredFinished);
    connect(mMainWindow->getCloudManager(), &CloudManager::insertListView, this, &CloudToolsView::onInsertListView);
    connect(mMainWindow->getCloudManager(), &CloudManager::updateListItem, this, &CloudToolsView::onUpdateListItem);
    connect(mMainWindow->getCloudManager(), &CloudManager::removeListItem, this, &CloudToolsView::onRemoveListItem);

    connect(mMainWindow->getCloudManager(), &CloudManager::cloudFileError, this, &CloudToolsView::onCloudFileError);
    connect(mMainWindow->getCloudManager(), &CloudManager::uploadWidthNewCloudVersion, this, &CloudToolsView::onUploadWidthNewCloudVersion);
    connect(mMainWindow->getCloudManager(), &CloudManager::downloadWidthNewCloudVersion, this, &CloudToolsView::onDownloadWidthNewCloudVersion);

    connect(mMainWindow->getTransportController(), &TransportController::uploadProgress, this, &CloudToolsView::onFileUploadProgress);
    connect(mMainWindow->getTransportController(), &TransportController::downloadProgress, this, &CloudToolsView::onFileDownloadProgress);
    connect(mMainWindow->getTransportController(), &TransportController::transferStateSignal, this, &CloudToolsView::onTransferState);
    connect(mMainWindow->getTransportController(), &TransportController::downloadThumbnailDone, this, &CloudToolsView::onDownloadThumbnailDone);
    connect(mMainWindow->getTransportController(), &TransportController::startUploadTask, this, &CloudToolsView::onStartUploadTask);

    connect(UserCenterManager::instance(), &UserCenterManager::logout, this, &CloudToolsView::onLogout);
    connect(Loginutil::getInstance(),&Loginutil::loginSignal, this, &CloudToolsView::onLogin);
    connect(UserCenterManager::instance(), &UserCenterManager::getPlanFinished, this, [=] {
        if (UserCenterManager::instance()->haveCloudPrivalege()) {
            mBuyBtn->hide();
        } else {
            mBuyBtn->show();
        }
    });
    connect(SignalCenter::instance(), &SignalCenter::clearCloudCache, this, [=](void *mainWindow) {
        if (mainWindow != mMainWindow) {
            return;
        }
        mMainWindow->getCloudManager()->getCloudList(CloudSpace, 0);
    });
    installEventFilter(this);
}

CloudToolsView::~CloudToolsView()
{
    if(mListView != nullptr){
        mListView->deleteLater();
    }
    if(mReferences != nullptr){
        mReferences->deleteLater();
        mReferences = nullptr;
    }
    if (mAnimationTimer.isActive()) {
        mAnimationTimer.stop();
    }
    if (mTimer.isActive()) {
        mTimer.stop();
    }
}

void CloudToolsView::initView()
{
    mMainStackedWidget = new QStackedWidget();
    mMainStackedWidget->setObjectName("mainStackedWidget");

    RecycleBinToolsView *recycleBinToolsView = new RecycleBinToolsView(mMainWindow, this);
    connect(recycleBinToolsView, &RecycleBinToolsView::returnClicked, this, [=] {
        mMainWindow->getCloudManager()->getDiskInfo();
        mMainWindow->getCloudManager()->getCloudList(CloudSpace, 0);
        mMainStackedWidget->setCurrentIndex(CloudView);
    });
    mMainStackedWidget->insertWidget(CloudView, createCloudView());
    mMainStackedWidget->insertWidget(RecycleView, recycleBinToolsView);
    QVBoxLayout* layout = new QVBoxLayout();
    layout->setMargin(0);
    layout->setSpacing(0);
    layout->addWidget(mMainStackedWidget);
    setLayout(layout);

    connect(this, &CloudToolsView::recycleClicked, this, [=] {
        mMainWindow->getCloudManager()->getCloudList(RecycleBin, 0);
        mMainStackedWidget->setCurrentIndex(RecycleView);
    });
}

QFrame *CloudToolsView::createCloudView()
{
    QFrame* contentFrame = new QFrame();
    mOutterStackedWidget = new QStackedWidget();
    mOutterStackedWidget->setObjectName("outterStackedWidget");

    QFrame* cloudViewFrame = new QFrame();
    cloudViewFrame->setObjectName("mainFrame");
    mOutterStackedWidget->insertWidget(UnloginView, createUnloginWidget());
    mOutterStackedWidget->insertWidget(LoginView, cloudViewFrame);

    auto openFileFrame = createFileMangerFrame();
    mStackedWidget = new QStackedWidget();
    mStackedWidget->setObjectName("stackedWidget");

    mListView = new CloudListView(this);
    connect(mListView,&CloudListView::itemClick,this,[=](BaseFileDetailData *data){
        CloudFileDetailData *cloudData = dynamic_cast<CloudFileDetailData *>(data);
        if (cloudData && cloudData->getTransFerState() != TransferSuccess) {
            return;
        }
        onOpenFile(data); // 点击打开文件
    });
    connect(mListView,&CloudListView::popMenu,this,[=](BaseFileDetailData *data){
        onOpenMenu(data);
    });
    mStackedWidget->insertWidget(EmptyFileView, createEmptyFileWidget());

    QWidget *viewContainer = new QWidget(this);
    MAKE_SCROLLBAR_CONTAINER(mListView, 6, viewContainer)
    mStackedWidget->insertWidget(FileView, viewContainer);

    mTypeStackedWidget = new QStackedWidget();
    mTypeStackedWidget->setObjectName("stackedWidget");
    mTypeStackedWidget->insertWidget(CloudList, mStackedWidget);
    mUploadsToolsView = new CloudUploadsToolsView(mMainWindow, this);
    connect(mUploadsToolsView, &CloudUploadsToolsView::enableFreshButton, this, [=](bool enable) {
        if (mTypeStackedWidget->currentIndex() == CloudList) {
            return;
        }
        mRefreshBtn->setEnabled(enable);
    });
    connect(mUploadsToolsView, &CloudUploadsToolsView::enableViewModeButton, this, [=](bool enable) {
        mListViewBtn->setEnabled(enable);
        mThumbnailBtn->setEnabled(enable);
    });
    connect(mUploadsToolsView, &CloudUploadsToolsView::removeItem, this, &CloudToolsView::onRemoveItem);
    connect(mUploadsToolsView, &CloudUploadsToolsView::removeItemByPath, this, &CloudToolsView::onRemoveItemByPath);

    mTypeStackedWidget->insertWidget(UploadsList, mUploadsToolsView);
    mDownloadsToolsView = new CloudDownloadsToolsView(mMainWindow, this);
    connect(mDownloadsToolsView, &CloudDownloadsToolsView::enableFreshButton, this, [=](bool enable) {
        if (mTypeStackedWidget->currentIndex() == CloudList) {
            return;
        }
        mRefreshBtn->setEnabled(enable);
    });
    connect(mDownloadsToolsView, &CloudDownloadsToolsView::enableViewModeButton, this, [=](bool enable) {
        mListViewBtn->setEnabled(enable);
        mThumbnailBtn->setEnabled(enable);
    });
    mTypeStackedWidget->insertWidget(DownloadsList, mDownloadsToolsView);

    QFrame* mainFrame = new QFrame();
    QVBoxLayout* mainFrameLayout = new QVBoxLayout();
    mainFrameLayout->setMargin(0);
    mainFrameLayout->setSpacing(0);
    mainFrameLayout->addWidget(createToolBar());
    mainFrameLayout->addWidget(mTypeStackedWidget);
    mainFrame->setLayout(mainFrameLayout);

    QVBoxLayout* bigLayout = new QVBoxLayout();
    bigLayout->setContentsMargins(0,0,0,0);
    bigLayout->setSpacing(0);
    bigLayout->addSpacing(6);
    bigLayout->addWidget(mainFrame);
    bigLayout->addSpacing(8);
    bigLayout->addWidget(createBottomToolWidget());
    cloudViewFrame->setLayout(bigLayout);

    QVBoxLayout* layout = new QVBoxLayout();
    layout->setMargin(0);
    layout->setSpacing(0);
    layout->addSpacing(12);
    layout->addWidget(openFileFrame);
    layout->addSpacing(4);
    layout->addWidget(mOutterStackedWidget);
    contentFrame->setLayout(layout);
    return contentFrame;
}

QFrame* CloudToolsView::createToolBar()
{
    QFrame* toolBar = new QFrame();
    toolBar->setObjectName("toolBar");

    QHBoxLayout* hlayout = new QHBoxLayout();
    hlayout->setSpacing(16);
    hlayout->setContentsMargins(20,8,20,0);

    mListViewBtn = new CShowTipsBtn();
    mListViewBtn->setObjectName("listViewBtn");
    mListViewBtn->setToolTip(tr("List View"));

    mThumbnailBtn = new CShowTipsBtn();
    mThumbnailBtn->setObjectName("thumbnailBtn");
    mThumbnailBtn->setToolTip(tr("Thumbnail View"));

    mListViewBtn->setAutoExclusive(true);
    mThumbnailBtn->setAutoExclusive(true);

    mListViewBtn->setCheckable(true);
    mThumbnailBtn->setCheckable(true);
    mListViewBtn->setChecked(true);

    connect(this, &CloudToolsView::viewModeChange, this, &CloudToolsView::onViewTypeChanged);
    connect(mListViewBtn,&QPushButton::released,this,[=] { emit viewModeChange(QListView::ListMode); });
    connect(mThumbnailBtn,&QPushButton::released,this,[=] { emit viewModeChange(QListView::IconMode); });

//    mRemoveBtn = new CShowTipsBtn();
//    mRemoveBtn->setObjectName("removeBtn");
//    mRemoveBtn->setToolTip(tr("Clear List"));
//    connect(mRemoveBtn,&QPushButton::clicked,this,&CloudToolsView::onClearList);

    mRefreshBtn = new CShowTipsBtn();
    mRefreshBtn->setObjectName("refreshBtn");
    mRefreshBtn->setToolTip(tr("Refresh"));
    mRefreshBtn->setFixedSize(24, 24);
    connect(mRefreshBtn,&QPushButton::clicked,this,&CloudToolsView::onRefresh);

    mTimer.setInterval(40);
    connect(&mTimer, &QTimer::timeout, this, &CloudToolsView::onLoadingTimeout);

    mAnimationTimer.setInterval(1000 * 5);
    connect(&mAnimationTimer, &QTimer::timeout, this, &CloudToolsView::onAnimationTimeout);
    mAnimationLabel = new QLabel(this);
    mAnimationLabel->setStyleSheet("background:#FBFBFB;");
    mAnimationLabel->hide();

    mTypeCombobox = new UComboBox({tr("UPDF Cloud"), tr("Uploads"), tr("Downloads")});
    mTypeCombobox->setObjectName("typeCombobox");
    GuiUtil::setComboboxRoundStyle(mTypeCombobox,QColor(0,0,0,150));
    connect(mTypeCombobox, static_cast<void(QComboBox::*)(int)>(&QComboBox::activated),
            [this](int index){
        mTypeStackedWidget->setCurrentIndex(index);
        onViewTypeChanged(mListViewBtn->isChecked() ? QListView::ListMode : QListView::IconMode);
        if (index == CloudList) {
            mSortCombobox->setVisible(true);
            showStackWidget(mListView->getModel()->rowCount() > 0 ? FileView : EmptyFileView);
        } else {
            mSortCombobox->setEnabled(false);
            mSortCombobox->setVisible(false);
        }
    });

    mSortCombobox = new CheckComboBox();
    mSortCombobox->setObjectName("sortCombobox");
    GuiUtil::setComboboxRoundStyle(mSortCombobox,QColor(0,0,0,150));
    connect(mSortCombobox, &CheckComboBox::sortTypeChanged, [this](int index){
        mSortType = (SortType)index;
        mListView->getModel()->sortItems(SortType(index));
    });

    hlayout->addWidget(mTypeCombobox);
    hlayout->addItem(new QSpacerItem(10,10,QSizePolicy::Expanding,QSizePolicy::Expanding));
    hlayout->addWidget(mSortCombobox);
    hlayout->addSpacing(100);
    hlayout->addWidget(mListViewBtn);
    hlayout->addWidget(mThumbnailBtn);
//    hlayout->addWidget(mRemoveBtn);
    hlayout->addWidget(mRefreshBtn);

    toolBar->setLayout(hlayout);
    return toolBar;
}

QFrame *CloudToolsView::createUnloginWidget()
{
    QFrame* frame = new QFrame();
    frame->setObjectName("unloginFrame");

    auto vlayout = new QVBoxLayout();
    vlayout->setMargin(0);
    vlayout->setSpacing(16);

    mIconLabel = new QLabel();
    mIconLabel->setObjectName("welcomeIllustrationLabel");
    mIconLabel->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Expanding);

    auto tipFrame = new QFrame();
    auto tipLayout = (QVBoxLayout*)LayoutUtil::CreateLayout(Qt::Vertical,0,4);
    if("zh" == SettingUtil::language() || "zhhk" == SettingUtil::language() ){
            tipLayout->setContentsMargins(8*StyleSheetHelper::DpiScale(),0,0,0);
    }
    tipFrame->setLayout(tipLayout);
    QFont font = QApplication::font();
    font.setWeight(60);
    mTipsLabel = new QLabel(tr("Login to access UPDF Cloud files."));
    mTipsLabel->setObjectName("tipsLabel");
    mTipsLabel->setFont(font);
    mTipsLabel->setAlignment(Qt::AlignHCenter);
    mTipsLabel1 = new QLabel(tr("Work with, manage and share files across web, desktop and mobile."));
    mTipsLabel1->setObjectName("tipsLabel1");
    mTipsLabel1->setAlignment(Qt::AlignHCenter);
    tipLayout->addWidget(mTipsLabel,0,Qt::AlignHCenter);
    tipLayout->addWidget(mTipsLabel1,0,Qt::AlignHCenter);

    QPushButton *loginBtn = new QPushButton(tr("Login"));
    loginBtn->setObjectName("confirmBtn");
    loginBtn->setFixedSize(118,32);
    connect(loginBtn,&QPushButton::clicked, this, [=]{
        Loginutil::getInstance()->openLoginWeb();
        GaMgr->takeGaLoginTask(GaManager::cloud_button);
    });

    vlayout->addItem(new QSpacerItem(0,0,QSizePolicy::Minimum,QSizePolicy::Expanding));
    vlayout->addWidget(mIconLabel,0,Qt::AlignCenter);
    vlayout->addWidget(tipFrame);
    vlayout->addWidget(loginBtn,0,Qt::AlignCenter);
    vlayout->addSpacing(30);
    vlayout->addItem(new QSpacerItem(0,0,QSizePolicy::Minimum,QSizePolicy::Expanding));

    frame->setLayout(vlayout);
    return frame;
}

void CloudToolsView::onViewTypeChanged(int viewMode)
{
    if (mTypeStackedWidget->currentIndex() == CloudList) {
        mListView->setListViewMode(QListView::ViewMode(viewMode));
        mListView->update();
    } else if (mTypeStackedWidget->currentIndex() == UploadsList) {
        mUploadsToolsView->setViewMode(viewMode);
    } else if (mTypeStackedWidget->currentIndex() == DownloadsList) {
        mDownloadsToolsView->setViewMode(viewMode);
    }
}

void CloudToolsView::onOpenDialogToUpload()
{
    if(!LimitUseMGR->isLogin()){
        if(LimitUseMGR->takeUCloudLimit() != LimitUseMgr::Continue){
            return ;
        }
    }
    auto openPath = ConfigureUtil::readHisFromINI(HisOpenPath,HisOpenPath,ConfigUtil::UPDFHisPath());
    const QStringList filePathList = QFileDialog::getOpenFileNames(this,
                                                                   tr("Upload"), openPath, ("(*.pdf)"));
    if(filePathList.isEmpty()) {
        return;
    }
    setCursor(Qt::WaitCursor);
    mMainWindow->getCloudManager()->onUploadFiles(filePathList);
    setCursor(Qt::ArrowCursor);
}

void CloudToolsView::onOpenFile(BaseFileDetailData *data)
{
    CloudFileDetailData *cloudData = dynamic_cast<CloudFileDetailData *>(data);
    if (!cloudData) {
        return;
    }
    QString fileIdPath = QDir::fromNativeSeparators(CloudPredefine::workSpaceDir() + QDir::separator() + QString::number(cloudData->fileId));
    QString filePath = QDir::fromNativeSeparators(fileIdPath + QDir::separator() + cloudData->fileName + PDF_FILE_SUFFIX);
    if(QFile::exists(filePath)){
        // 判断版本
        mMainWindow->getCloudManager()->checkFileInfo(cloudData->fileId, Download);
    } else {
        mMainWindow->getCloudManager()->preDownloadFile(cloudData->fileId);
    }
}

void CloudToolsView::onCloudStarredFinished(const DeleteOprStruct &deleteOpr)
{
    for (qint64 fileId : deleteOpr.fileIds) {
        CloudFileDetailData cloudItem;
        cloudItem.fileId = fileId;
        int row = mListView->getModel()->itemRow(&cloudItem);
        if (row != -1) {
            BaseFileDetailData *data = mListView->getModel()->getItemByRow(row);
            data->isStarred = true;
            mListView->getModel()->updateItem(data);
        }
    }
}

void CloudToolsView::onCloudUnstarredFinished(const DeleteOprStruct &deleteOpr)
{
    for (qint64 fileId : deleteOpr.fileIds) {
        CloudFileDetailData cloudItem;
        cloudItem.fileId = fileId;
        int row = mListView->getModel()->itemRow(&cloudItem);
        if (row != -1) {
            BaseFileDetailData *data = mListView->getModel()->getItemByRow(row);
            data->isStarred = false;
            mListView->getModel()->updateItem(data);
        }
    }
}

void CloudToolsView::onInsertListView(const CloudFilePropInfo &fileInfo)
{
    CloudFileDetailData *detailData = new CloudFileDetailData(fileInfo.fileId, 0, fileInfo.fileShowName, fileInfo.filePath,
                                                            fileInfo.logoPath, fileInfo.lastVisitTime, QDateTime(),
                                                            fileInfo.currentPageIndex,fileInfo.totalPage, fileInfo.fileSize,
                                                            fileInfo.isStarred, false, false, fileInfo.havePwd);
    if (fileInfo.transFail) {
        detailData->setTransFerState(TransferFail);
    } else {
        detailData->setTransFerState(WaitTransfer);
    }
    mListView->getModel()->insertItem(0, detailData);
    mListView->getModel()->sortItems(mSortType);
    showStackWidget(mListView->getModel()->rowCount() > 0 ? FileView : EmptyFileView);
}

void CloudToolsView::onUpdateListItem(const CloudFilePropInfo &fileInfo)
{
    QString fileIdPath = QDir::fromNativeSeparators(CloudPredefine::workSpaceDir() + QDir::separator() + QString::number(fileInfo.fileId));
    if (fileInfo.filePath.contains(fileIdPath)) {
        return;
    }
    BaseFileDetailData baseData;
    baseData.filePath = fileInfo.filePath;
    int row = mListView->getModel()->itemRow(&baseData);
    if (row == -1) {
        return;
    }
    BaseFileDetailData* data = mListView->getModel()->getItemByRow(row);
    CloudFileDetailData *cloudData = dynamic_cast<CloudFileDetailData *>(data);
    if (cloudData) {
        cloudData->fileId = fileInfo.fileId;
        cloudData->currentPageIndex = fileInfo.currentPageIndex;
        cloudData->totalPage = fileInfo.totalPage;
        cloudData->imgPath = fileInfo.logoPath;
        if (fileInfo.transFail) {
            cloudData->setTransFerState(TransferFail);
        }
        mListView->getModel()->updateItem(cloudData);
    }
}

void CloudToolsView::onRemoveListItem(const QString &filePath)
{
    if (filePath.contains(CloudPredefine::workSpaceDir())) {
        return;
    }
    BaseFileDetailData baseData;
    baseData.filePath = filePath;
    int row = mListView->getModel()->itemRow(&baseData);
    if (row == -1) {
        return;
    }
    mListView->getModel()->removeItem(row);
    showStackWidget(mListView->getModel()->rowCount() > 0 ? FileView : EmptyFileView);
}

void CloudToolsView::onCloudFileError(qint64 fileId, int code)
{
    onRemoveItem(fileId);
}

void CloudToolsView::onRemoveItemByPath(const QString &filePath)
{
    BaseFileDetailData baseItem;
    baseItem.filePath = filePath;
    int row = mListView->getModel()->itemRow(&baseItem);
    if (row == -1) {
        return;
    }
    BaseFileDetailData *data = mListView->getModel()->getItemByRow(row);
    removeCloudItem(data);
}
void CloudToolsView::onRemoveItem(qint64 fileId)
{
    CloudFileDetailData cloudItem;
    cloudItem.fileId = fileId;
    int row = mListView->getModel()->itemRow(&cloudItem);
    if (row == -1) {
        return;
    }
    BaseFileDetailData *data = mListView->getModel()->getItemByRow(row);
    removeCloudItem(data);
}

void CloudToolsView::removeCloudItem(BaseFileDetailData *data)
{
    CloudFileDetailData *cloudData = dynamic_cast<CloudFileDetailData *>(data);
    Q_UNUSED(cloudData);
    mListView->getModel()->removeItem(data);
    if(mListView->getModel()->rowCount() > 0) {
        showStackWidget(FileView);
    } else {
        showStackWidget(EmptyFileView);
    }
}

void CloudToolsView::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    }else {
        event->ignore();
    }
    //    QWidget::dragEnterEvent(event);
}

void CloudToolsView::dropEvent(QDropEvent *event)
{
    QStringList fileList;
    auto urls = event->mimeData()->urls();
    for(auto url:urls){
        if (!url.path().endsWith(PDF_FILE_SUFFIX, Qt::CaseInsensitive)){
            continue;
        }
        fileList.push_back(url.path().mid(1).toUtf8());
    }
    if(!fileList.empty()){
        setCursor(Qt::WaitCursor);
        mMainWindow->getCloudManager()->onUploadFiles(fileList);
        setCursor(Qt::ArrowCursor);
    }
    if (mTypeStackedWidget->currentIndex() == UploadsList) {
        mUploadsToolsView->loadUploadTaskList();
    }
}

void CloudToolsView::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    if (!Loginutil::getInstance()->isLogin()) {
        mOutterStackedWidget->setCurrentIndex(UnloginView);
        return;
    }
    mOutterStackedWidget->setCurrentIndex(LoginView);
    if (!mFirstLoad) {
        mFirstLoad = true;
        if (mMainStackedWidget->currentIndex() == CloudView) {
            if (mTypeStackedWidget->currentIndex() == CloudList) {
                onRefresh();
            }
        } else {
            mMainWindow->getCloudManager()->getCloudList(RecycleBin, 0);
        }
    }
}
bool CloudToolsView::eventFilter(QObject *watched, QEvent *event)
{
    if (this == watched) {
        if (event->type() == QEvent::WindowActivate) {
        }
    }
    return QWidget::eventFilter(watched, event);
}

QFrame* CloudToolsView::createEmptyFileWidget()
{
    QFrame* frame = new QFrame();
    frame->setObjectName("EmptyFileFrame");

    auto vlayout = new QVBoxLayout();
    vlayout->setMargin(0);
    vlayout->setSpacing(16);

    mIconLabel = new QLabel();
    mIconLabel->setObjectName("welcomeIllustrationLabel");
    mIconLabel->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Expanding);

    auto tipFrame = new QFrame();
    auto tipLayout = (QVBoxLayout*)LayoutUtil::CreateLayout(Qt::Vertical,0,4);
    if("zh" == SettingUtil::language() || "zhhk" == SettingUtil::language() ){
            tipLayout->setContentsMargins(8*StyleSheetHelper::DpiScale(),0,0,0);
    }
    tipFrame->setLayout(tipLayout);
    QFont font = QApplication::font();
    font.setWeight(60);
    mTipsLabel = new QLabel(tr("No cloud files yet."));
    mTipsLabel->setObjectName("tipsLabel");
    mTipsLabel->setFont(font);
    mTipsLabel->setAlignment(Qt::AlignHCenter);
    mTipsLabel1 = new QLabel(tr("Files you upload to UPDF Cloud will appear here. You can work with and share files across web, desktop and mobile."));
    mTipsLabel1->setObjectName("tipsLabel1");
    mTipsLabel1->setAlignment(Qt::AlignHCenter);
    mTipsLabel1->setWordWrap(true);
    mTipsLabel1->setMinimumWidth(725);
    tipLayout->addWidget(mTipsLabel,0,Qt::AlignHCenter);
    tipLayout->addWidget(mTipsLabel1,0,Qt::AlignHCenter);

    QPushButton *uploadBtn = new QPushButton(tr("Upload"));
    uploadBtn->setObjectName("confirmBtn");
    uploadBtn->setFixedSize(118,32);
    connect(uploadBtn,&QPushButton::clicked, this, &CloudToolsView::onOpenDialogToUpload);

    vlayout->addItem(new QSpacerItem(0,0,QSizePolicy::Minimum,QSizePolicy::Expanding));
    vlayout->addWidget(mIconLabel,0,Qt::AlignCenter);
    vlayout->addWidget(tipFrame);
    vlayout->addWidget(uploadBtn,0,Qt::AlignCenter);
    vlayout->addSpacing(30);
    vlayout->addItem(new QSpacerItem(0,0,QSizePolicy::Minimum,QSizePolicy::Expanding));

    frame->setLayout(vlayout);
    return frame;
}

QFrame *CloudToolsView::createBottomToolWidget()
{
    QFrame* toolFrame = new QFrame();
    toolFrame->setObjectName("toolFrame");
    toolFrame->setFixedHeight(32);
    QHBoxLayout* frameLayout = new QHBoxLayout();
    frameLayout->setMargin(0);
    frameLayout->setSpacing(0);

    CShowTipsBtn *recycleBtn = new CShowTipsBtn(this);
    recycleBtn->setArrowPoint(CShowTipsBtn::BOTTOM);
    recycleBtn->setToolTip(tr("Recycle bin"));
    recycleBtn->setObjectName("recycleBtn");
    recycleBtn->setFixedSize(16, 16);
    connect(recycleBtn, &CShowTipsBtn::clicked, this, &CloudToolsView::recycleClicked);

    mDiskInfoLabel = new QLabel(this);
    mDiskInfoLabel->setObjectName("diskLabel");
    mDiskInfoLabel->setAlignment(Qt::AlignCenter);

    mDiskCapacityBar = new QProgressBar(this);
    mDiskCapacityBar->setRange(0, 100);
    mDiskCapacityBar->setFixedHeight(8);
    mDiskCapacityBar->setMinimumWidth(620);
    mDiskCapacityBar->setTextVisible(false);
    mDiskCapacityBar->setObjectName("diskBar");

    mBuyBtn = new CShowTipsBtn(this);
    mBuyBtn->setArrowPoint(CShowTipsBtn::BOTTOM);
    mBuyBtn->setToolTip(tr("Upgrade to get more storage"));
    mBuyBtn->setObjectName("buyBtn");
    mBuyBtn->setFixedSize(16, 16);
    connect(mBuyBtn, &QPushButton::clicked, this, [=]{
        if (!Loginutil::getInstance()->isLogin()) {
            Loginutil::getInstance()->openLoginWeb();
        } else {
            UserCenterManager::instance()->buy();
        }
    });
    frameLayout->addSpacing(10);
    frameLayout->addWidget(recycleBtn, 1, Qt::AlignCenter);
    frameLayout->addWidget(mDiskInfoLabel, 1, Qt::AlignCenter);
    frameLayout->addWidget(mDiskCapacityBar, 100, Qt::AlignCenter);
    frameLayout->addWidget(mBuyBtn, 1, Qt::AlignCenter);
    frameLayout->addSpacing(10);
    frameLayout->setSpacing(10);

    toolFrame->setLayout(frameLayout);
    return toolFrame;
}

QFrame* CloudToolsView::createFileMangerFrame()
{
    auto frame = new QFrame(this);
    frame->setFixedHeight(156);
    auto hlayout = new QHBoxLayout();
    hlayout->setSpacing(0);
    hlayout->setContentsMargins(0,0,0,0);
    auto mOpenFileFrame = new UploadFileWidget(this);

    hlayout->addWidget(mOpenFileFrame);
    connect(mOpenFileFrame,&UploadFileWidget::openDir,this, &CloudToolsView::onOpenDialogToUpload);

    frame->setLayout(hlayout);
    return frame;
}

bool CloudToolsView::isInListView(const QString &filePath)
{
    BaseFileDetailData detailData;
    detailData.filePath = filePath;
    return mListView->getModel()->itemRow(&detailData) != -1;
}

void CloudToolsView::onOpenMenu(BaseFileDetailData *data)
{
    CloudFileDetailData *cloudData = dynamic_cast<CloudFileDetailData *>(data);
    if (!cloudData) {
        return;
    }
    if (cloudData->getTransFerState() == TransferSuccess) {
        QMenu menu;
        menu.setObjectName("customMenu");
        GuiUtil::setMenuRoundStyle(&menu);

        QAction* action_download = new QAction(tr("Download"));
        QAction* action_open = new QAction(tr("Open"));
        QAction* action_rename = new QAction(tr("Rename"));
        QAction* action_star = new QAction( !data->isStarred ? tr("Star this File") : tr("Unstar this File"));
        QAction* action_remove = new QAction(tr("Delete"));

        QActionGroup* actionGroup = new QActionGroup(&menu);
        QWidgetAction *widgetAction = new QWidgetAction(&menu);
        widgetAction->setDefaultWidget(new QLabel(&menu));
        actionGroup->addAction(widgetAction); // 添加空action
        actionGroup->addAction(action_open);
        actionGroup->addAction(action_rename);
        actionGroup->addAction(action_download);
        actionGroup->addAction(action_star);
        actionGroup->addAction(action_remove);

        connect(actionGroup,&QActionGroup::triggered,this,[&](QAction *action){
            CloudFileDetailData *cloudData = dynamic_cast<CloudFileDetailData *>(data);
            if (!cloudData) {
                return;
            }
            if(action == action_open){
                onOpenFile(data);
            } else if(action == action_download){
                QString docDir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
                QString saveDir = QFileDialog::getExistingDirectory(this, tr("Save"), docDir, QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
                if (saveDir.isEmpty()) {
                    return;
                }
                mMainWindow->getCloudManager()->preDownloadFile(cloudData->fileId, saveDir);
            } else if(action == action_remove){
                if (MainWindow::isOpenCloudFile(cloudData->fileId)) {
                    MessageWidget win(QStringList{tr("This file cannot be deleted because it is currently open.")},
                                      QStringList{tr("OK"),"",""});
                    win.resetIconImage(MessageWidget::IT_UnableDelete);
                    win.exec();
                    return;
                }
                MessageWidget msgTipsWin({tr("Are you sure you want to delete this file?"),tr("Files you deleted can be restored from the Recycle Bin.")},
                                         {tr("Delete"),"",tr("Cancel")});
                msgTipsWin.resetIconImage(MessageWidget::IT_Clear);
                if (msgTipsWin.exec() == MessageWidget::Accepted) {
                    QList<qint64> fileIds;
                    QList<qint64> dirIds;
                    fileIds.append(cloudData->fileId);
                    mMainWindow->getCloudManager()->moveToRecycle(fileIds, dirIds);
                    // remove earlier
                    DeleteOprStruct deleteOpr;
                    deleteOpr.fileIds = fileIds;
                    deleteOpr.dirIds = dirIds;
                    onMoveToRecycleFinished(deleteOpr);
                }
            } else if(action == action_star){
                QList<qint64> fileIds;
                QList<qint64> dirIds;
                fileIds.append(cloudData->fileId);
                if (cloudData->isStarred) {
                    mMainWindow->getCloudManager()->cloudUnstarred(fileIds, dirIds);
                } else {
                    mMainWindow->getCloudManager()->cloudStarred(fileIds, dirIds);
                }
            } else if(action == action_rename) {
                FileRenameWidget fileRenameWidget;
                connect(&fileRenameWidget, &FileRenameWidget::nameChanged, this, [=](const QString &name) {
                    if (name != cloudData->fileName) {
                        mMainWindow->getCloudManager()->renameFile(cloudData->fileId, name);
                    }
                });
                fileRenameWidget.setFileName(cloudData->fileName);
                fileRenameWidget.exec();
            }
        });
        for(auto action:actionGroup->actions()){
            menu.addAction(action);
        }
        menu.insertSeparator(action_download);
        menu.insertSeparator(action_star);
        menu.insertSeparator(action_remove);
        QWidget *widget = new QWidget(&menu);
        widget->setFixedHeight(32);
        QLabel *contentLabel = new QLabel;
        contentLabel->setStyleSheet("background:#F0F0F0;border-top-left-radius:4px; border-top-right-radius:4px;");
        QLabel *iconLabel = new QLabel;
        iconLabel->setFixedSize(16, 16);
        iconLabel->setStyleSheet("image: url(://updf-win-icon/ui2/2x/welcome-page/cloud_file1.png);");
        QLabel *titleLabel = new QLabel;
        titleLabel->setStyleSheet("color:#353535;font-weight:500;");
        QHBoxLayout *layout = new QHBoxLayout(widget);
        layout->setContentsMargins(13, 0, 13, 0);
        layout->addWidget(contentLabel);

        QHBoxLayout *innerLayout = new QHBoxLayout(contentLabel);
        innerLayout->setContentsMargins(4, 0, 0, 0);
        innerLayout->setSpacing(8);
        innerLayout->addWidget(iconLabel, 0, Qt::AlignCenter);
        innerLayout->addWidget(titleLabel, 0, Qt::AlignCenter);
        innerLayout->addStretch();
        titleLabel->setText(tr("Cloud File"));
        widget->setMinimumWidth(menu.sizeHint().width());

        menu.exec(QCursor::pos());
      } else {
        UMenu menu;
        GuiUtil::setMenuRoundStyle(&menu);

        QAction* action_retry = new QAction(tr("Retry"));
        QAction* action_cancel = new QAction(tr("Cancel"));
        QAction* action_delete = new QAction(tr("Delete"));

        CloudFileDetailData *cloudData = dynamic_cast<CloudFileDetailData *>(data);
        if (!cloudData) {
            return;
        }
        QActionGroup* actionGroup = new QActionGroup(&menu);
        if (mMainWindow->getTransportController()->checkFileDownloading(cloudData->fileId)) {
            if (cloudData->getTransFerState() == Transferring) {
                actionGroup->addAction(action_cancel);
            }
        } else {
            if (cloudData->getTransFerState() == TransferFail) {
                actionGroup->addAction(action_retry);
            }
            if (cloudData->getTransFerState() == Transferring) {
                actionGroup->addAction(action_cancel);
            }
            actionGroup->addAction(action_delete);
        }
        connect(actionGroup,&QActionGroup::triggered,this,[&](QAction *action){
            CloudFileDetailData *cloudData = dynamic_cast<CloudFileDetailData *>(data);
            if (!cloudData) {
                return;
            }
            if(action == action_retry){
                QMap<QString, QString> taskMap = SerializeConfig::instance()->loadPauseTask();
                for (QString key : taskMap.keys()) {
                    if (key.contains(Upload_File_Prefix) && key.contains(QString::number(cloudData->fileId))) {
                        QString configPath = taskMap.value(key);
                        OssUploadParam uploadParam = SerializeConfig::instance()->readUploadParam(configPath);
                        if (uploadParam.fileInfo.transFail) {
                            mMainWindow->getCloudManager()->resumeUpload(configPath, true);
                            emit mMainWindow->getTransportController()->transferStateSignal(cloudData->fileId, WaitTransfer);
                            break;
                        }
                    }
                }
            } else if(action == action_cancel){
                if (!mMainWindow->getTransportController()->checkFileDownloading(cloudData->fileId)) {
                    emit mMainWindow->getTransportController()->transferStateSignal(cloudData->fileId, TransferFail);
                    mMainWindow->getTransportController()->abortFileUpload(cloudData->fileId);
                    mMainWindow->getCloudManager()->finishUploadTask();
                } else {
                    emit mMainWindow->getTransportController()->transferStateSignal(cloudData->fileId, TransferSuccess);
                    mMainWindow->getTransportController()->abortFileDownload(cloudData->fileId);
                }
            }  else if(action == action_delete){
                // uploads 进行itemRemove
                if (cloudData->fileId <= 0) {
                    mMainWindow->getCloudManager()->removeWaitForUploadFileList(cloudData->filePath);
                    SerializeConfig::instance()->removeWaitUploadFile(cloudData->filePath);
                } else {
                    mUploadsToolsView->removeListItem(cloudData->fileId);
                    mMainWindow->getCloudManager()->removeWaitForUploadFileList(cloudData->filePath);
                    SerializeConfig::instance()->removeWaitUploadFile(cloudData->filePath);
                    SerializeConfig::instance()->removeTransportTask(cloudData->fileId, true);
                    mMainWindow->getTransportController()->abortFileUpload(cloudData->fileId);
                    emit mMainWindow->getTransportController()->transferStateSignal(cloudData->fileId, TransferFail);
                }
                removeCloudItem(data);
                mMainWindow->getCloudManager()->finishUploadTask();
            }
        });
        for(auto action:actionGroup->actions()){
            menu.addAction(action);
        }
        menu.setMinimumWidth(144);
        menu.setAnimationPoint(QCursor::pos());
      }
}

void CloudToolsView::onGetCloudFileList(const CloudFilePropInfos &fileInfos)
{
    stopAnimation();
    clearListView();
    for (CloudFilePropInfo fileInfo : fileInfos) {
        Q_ASSERT(fileInfo.fileId);
        QString fileIdPath = QDir::fromNativeSeparators(CloudPredefine::workSpaceDir() + QDir::separator() + QString::number(fileInfo.fileId));
        QDir dir(fileIdPath);
        if (!dir.exists()) {
            dir.mkdir(fileIdPath);
        }
        QString logoPath;
        // 判断是否本机文件
        if (0/*QFileInfo(fileInfo.filePath).exists()*/) {
            BaseFileDetailData detailData = HISTFileRecordsUtil::getInstance()->getHistoryRecord(fileInfo.filePath);
            logoPath = detailData.imgPath;
        } else {
            // 判断id目录是否存在logo.png
            logoPath = QDir::fromNativeSeparators(fileIdPath + QDir::separator() + "logo.png");
            if (!QFileInfo(logoPath).exists() && !fileInfo.logoUrl.isEmpty()) {
                OssDownloadParam downloadParam;
                fileInfo.logoName = "logo.png";
                downloadParam.fileInfo = fileInfo;
                mMainWindow->getTransportController()->downloadThumbnail(downloadParam);
            }
        }
        CloudFileDetailData *detailData = new CloudFileDetailData(fileInfo.fileId, 0, fileInfo.fileShowName, fileInfo.filePath,
                                                                logoPath, fileInfo.uploadTime, QDateTime(),
                                                                fileInfo.currentPageIndex,fileInfo.totalPage, fileInfo.fileSize,
                                                                fileInfo.isStarred, false, false, fileInfo.havePwd);
        detailData->setTransFerState(TransferSuccess);
        mListView->getModel()->appendItem(detailData);
        // 添加收藏
        if (fileInfo.isStarred) {
            emit addToStarred(detailData);
        }
    }
    QMap<QString, QString> taskMap = SerializeConfig::instance()->loadPauseTask();
    for (QString key : taskMap.keys()) {
        if (key.contains(Upload_File_Prefix)) {
            QString configPath = taskMap.value(key);
            OssUploadParam uploadParam = SerializeConfig::instance()->readUploadParam(configPath);
            QString fileIdPath = QDir::fromNativeSeparators(CloudPredefine::workSpaceDir() + QDir::separator() + QString::number(uploadParam.fileInfo.fileId));
            if (!uploadParam.fileInfo.filePath.contains(fileIdPath)/* && !uploadParam.fileInfo.transFail*/) { // 本地第一次上传的文件
                onInsertListView(uploadParam.fileInfo);
            }
        }
    }
    mMainWindow->getCloudManager()->addWaitUploadListView(SerializeConfig::instance()->loadWaitUploadFile());
    mListView->getModel()->sortItems(mSortType);
    showStackWidget(mListView->getModel()->rowCount() > 0 ? FileView : EmptyFileView);
}

void CloudToolsView::onFileUploadResult(const CloudFilePropInfo &fileInfo)
{
    CloudFileDetailData data;
    data.fileId = fileInfo.fileId;
    int row = mListView->getModel()->itemRow(&data);
    if (row != -1) {
        BaseFileDetailData* baseData = mListView->getModel()->getItemByRow(row);
        CloudFileDetailData *cloudData = dynamic_cast<CloudFileDetailData *>(baseData);
        if (cloudData) {
            cloudData->fileName = fileInfo.fileShowName;
            cloudData->openTime = fileInfo.uploadTime;
        }
        mListView->getModel()->updateItem(cloudData);
        mListView->getModel()->sortItems(mSortType);
    }
    // update icon
    QString fileIdPath = QDir::fromNativeSeparators(CloudPredefine::workSpaceDir() + QDir::separator() + QString::number(fileInfo.fileId));
    QDir dir(fileIdPath);
    if (!dir.exists()) {
        dir.mkdir(fileIdPath);
    }
    QString logoPath = QDir::fromNativeSeparators(fileIdPath + QDir::separator() + "logo.png");
    if (!QFileInfo(logoPath).exists()  && !fileInfo.logoUrl.isEmpty()) {
        OssDownloadParam downloadParam;
        downloadParam.fileInfo = fileInfo;
        downloadParam.fileInfo.logoName = "logo.png";
        mMainWindow->getTransportController()->downloadThumbnail(downloadParam);
    }
    QString filePath = QDir::fromNativeSeparators(fileIdPath + QDir::separator() + fileInfo.fileShowName + PDF_FILE_SUFFIX);
    if (!fileInfo.filePath.contains(fileIdPath) && !QFileInfo(filePath).exists()) {
        QFile::copy(fileInfo.filePath, filePath);
    }
    mMainWindow->getCloudManager()->getDiskInfo();
}

void CloudToolsView::onFileOssUploadResult(bool success, const OssUploadParam &param)
{
    if (success) {
        emit SignalCenter::instance()->updateBarIcon(param.fileInfo.fileId, "CloudSync", 0, mMainWindow);
    } else {
        emit SignalCenter::instance()->updateBarIcon(param.fileInfo.fileId, "CloudNosync", 0, mMainWindow);
        CloudFilePropInfo fileInfo = param.fileInfo;
        fileInfo.transFail = true;
        onUpdateListItem(fileInfo);
    }
}

void CloudToolsView::onFileUploadProgress(const OssUploadParam &param)
{
    double progress = param.uploadSize * 1.0 / param.fileInfo.fileSize * 100;
    progress = qMax(1.0, progress);
    emit SignalCenter::instance()->updateBarIcon(param.fileInfo.fileId, "CloudTrans", progress, mMainWindow);
    CloudFileDetailData itemData;
    itemData.fileId = param.fileInfo.fileId;
    int row = mListView->getModel()->itemRow(&itemData);
    if (row < 0) {
        return;
    }
    CloudFileDetailData *cloudData = dynamic_cast<CloudFileDetailData *>(mListView->getModel()->getItemByRow(row));
    cloudData->setProgress(progress);
    mListView->getModel()->updateItem(cloudData);
}

void CloudToolsView::onFileDownloadResult(bool success, const OssDownloadParam &param)
{
    if (success) {
        CloudFilePropInfo fileInfo = param.fileInfo;
        if (!param.saveDir.isEmpty()) { // 另存
            QString saveToFilePath = QDir::fromNativeSeparators(param.saveDir + QDir::separator() + param.fileInfo.fileShowName + PDF_FILE_SUFFIX);
            if (QFile::exists(saveToFilePath)) {
                QFile::remove(saveToFilePath);
            }
            // 记录
            CloudFilePropInfo propInfo = param.fileInfo;
            propInfo.filePath = saveToFilePath;
            propInfo.createTime = QDateTime::currentDateTime();
            SerializeConfig::instance()->addSaveAsRecord(propInfo);

            QString tempFilePath = param.saveDir + QDir::separator() + "temp.pdf";
            QtConcurrent::run([&, tempFilePath, saveToFilePath] {
                QFile::rename(tempFilePath, saveToFilePath);
                GuiUtil::openFilePath(saveToFilePath);
            });
        } else {
            // 文件打开
            if (mMainWindow->isOpenCloudFile(fileInfo.fileId)) {
                mMainWindow->closeCloudFile(fileInfo.fileId);
            }
            QString fileIdPath = QDir::fromNativeSeparators(CloudPredefine::workSpaceDir() + QDir::separator() + QString::number(fileInfo.fileId));
            if (fileInfo.fileShowName.isEmpty()) {
                fileInfo.fileShowName = QFileInfo(fileInfo.filePath).completeBaseName();
            }
            QString filePath = QDir::fromNativeSeparators(fileIdPath + QDir::separator() + fileInfo.fileShowName + PDF_FILE_SUFFIX);
            if (QFile::exists(filePath)) {
                QFile::remove(filePath);
            }
            QFile::rename(fileIdPath + QDir::separator() + "temp.pdf", filePath);
            emit openFile(filePath, param.fileInfo.fileId);
            SerializeConfig::instance()->updateTransDoneFileConfig(fileInfo, Download_File_Prefix);
            emit SignalCenter::instance()->updateBarIcon(param.fileInfo.fileId, "CloudSync", 0, mMainWindow);
       }
       emit mMainWindow->getTransportController()->transferStateSignal(param.fileInfo.fileId, TransferSuccess);
    }
}

void CloudToolsView::onDownloadThumbnailDone(const OssDownloadParam &param)
{
    QString fileIdPath = QDir::fromNativeSeparators(CloudPredefine::workSpaceDir() + QDir::separator() + QString::number(param.fileInfo.fileId));
    QString logoPath = QDir::fromNativeSeparators(fileIdPath + QDir::separator() + "logo.png");

    CloudFileDetailData itemData;
    itemData.fileId = param.fileInfo.fileId;
    int row = mListView->getModel()->itemRow(&itemData);
    if (row < 0) {
        return;
    }
    CloudFileDetailData *cloudData = dynamic_cast<CloudFileDetailData *>(mListView->getModel()->getItemByRow(row));
    cloudData->imgPath = logoPath;
    mListView->getModel()->updateItem(cloudData);
}

void CloudToolsView::onTransferState(qint64 fileId, TransferState state)
{
    CloudFileDetailData itemData;
    itemData.fileId = fileId;
    int row = mListView->getModel()->itemRow(&itemData);
    if (row < 0) {
        return;
    }
    CloudFileDetailData *cloudData = dynamic_cast<CloudFileDetailData *>(mListView->getModel()->getItemByRow(row));
    cloudData->setTransFerState(state);
    mListView->getModel()->updateItem(cloudData);
}

void CloudToolsView::onRefresh()
{
    startLoadingAnimation();
    if (mTypeStackedWidget->currentIndex() == CloudList) {
        mMainWindow->getCloudManager()->getDiskInfo();
        mMainWindow->getCloudManager()->getCloudList(CloudSpace, 0);
    } else if (mTypeStackedWidget->currentIndex() == UploadsList) {
        mUploadsToolsView->refreshList();
    } else if (mTypeStackedWidget->currentIndex() == DownloadsList) {
        mDownloadsToolsView->refreshList();
    }
}

void CloudToolsView::onLoadingTimeout()
{
    QString indexString = QString::number(mIndex);
    QString imagePath = QString("://updf-win-icon/ui2/2x/welcome-page/refresh/Comp 1_000%1.png").arg(indexString.rightJustified(2, '0'));
    mAnimationLabel->setPixmap(GuiUtil::generateSmoothPic(imagePath, mAnimationLabel->size()));
    if (--mIndex < 0) {
        mIndex = 24;
    }
}

void CloudToolsView::startLoadingAnimation()
{
    //animation
    mAnimationLabel->setFixedSize(mRefreshBtn->size());
    QPoint pos = mapToParent(mRefreshBtn->pos());
    mAnimationLabel->setGeometry(QRect(pos + QPoint(0, 178), mAnimationLabel->size()));
    mAnimationLabel->raise();
    mAnimationLabel->show();

    if (mTimer.isActive()) {
        mTimer.stop();
    }
    mIndex = 20;
    mTimer.start();
    mMinTimeAnimation = false;
    mCanStop = false;
    const int speed = 500;
    QTimer::singleShot(speed, this, [&] { // 保证至少动画旋转
        mMinTimeAnimation = true;
        stopAnimation();
    });
    mAnimationTimer.start();
}

void CloudToolsView::stopAnimation()
{
    mCanStop = true;
    stopAnimationImpl();
}

void CloudToolsView::stopAnimationImpl()
{
    if (!mMinTimeAnimation) {
        return;
    }
    if (!mCanStop) {
        return;
    }
    if (mTimer.isActive()) {
        mTimer.stop();
    }
    if (mAnimationTimer.isActive()) {
        mAnimationTimer.stop();
    }
    mAnimationLabel->hide();
}


void CloudToolsView::onUploadWidthNewCloudVersion(const CloudFilePropInfo &fileInfo, bool *continueUpload)
{
    *continueUpload = false;
    MessageWidget confirmWidget({tr("The document has been modified in the cloud. How would you like to proceed?")},
                                {tr("Confirm"),"",""},
                                MessageWidget::CT_CloudUpLoad);
    confirmWidget.resetIconImage(MessageWidget::IT_Cloud);
    connect(&confirmWidget, &MessageWidget::cloudFileUpdate, this, [=](int type) {
        if (type == MessageWidget::CCT_Update) {
            mMainWindow->getCloudManager()->preDownloadFile(fileInfo.fileId);
        } else if (type == MessageWidget::CCT_Discard) {
            *continueUpload = true;
        } else if (type == MessageWidget::CCT_SaveAs) {
            // 等待另存完成进行上传
            mMainWindow->getCloudManager()->cacheUploadWaitForDownload(fileInfo);
            QTimer::singleShot(10, this, [=] {
                QString docDir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
                QString saveDir = QFileDialog::getExistingDirectory(nullptr, tr("Save"), docDir, QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
                if (saveDir.isEmpty()) {
                    return;
                }
                mMainWindow->getCloudManager()->preDownloadFile(fileInfo.fileId, saveDir);
            });
        }
    });
    confirmWidget.exec();
}

void CloudToolsView::onDownloadWidthNewCloudVersion(const CloudFilePropInfo &fileInfo, bool *continueDownload)
{
    *continueDownload = false;
    if (!mReferences) {
        mReferences = new PreferencesMgr(this);
    }
    QString choice = PreferencesMgrData::getPreferencesValue(mReferences->getMetaEnum().valueToKey(PreferencesMgr::OpenNewCloudFile));
    if ("true" == choice || "false" == choice) {
        *continueDownload = "true" == choice;
        return;
    }

    MessageWidget confirmWidget({tr("The document has been updated in the cloud. Would you like to open this file with the latest version?")},
                                {tr("Yes"),"",tr("No")},MessageWidget::CT_CloudDownLoad);
    confirmWidget.resetIconImage(MessageWidget::IT_Cloud);
    connect(&confirmWidget, &MessageWidget::cloudVersionUpdate,this,[=](bool continueAction, bool check) {
        *continueDownload = continueAction;
        if (check) {
            PreferencesMgrData::updatePreferencesMap(mReferences->getMetaEnum().valueToKey(PreferencesMgr::OpenNewCloudFile), continueAction ? "true" : "false");
            mReferences->syncDataToConfigFile();
        }
    });
    confirmWidget.exec();
}

void CloudToolsView::onLogin()
{
    if (mBuyBtn->isHidden()) {
        mBuyBtn->show();
    }
    mOutterStackedWidget->setCurrentIndex(LoginView);
}

void CloudToolsView::onLogout()
{
    mMainStackedWidget->setCurrentIndex(CloudView);
    mOutterStackedWidget->setCurrentIndex(UnloginView);
}

void CloudToolsView::onStartUploadTask(const OssUploadParam &param)
{
    if (mMainStackedWidget->currentIndex() == CloudView) {
        if (mTypeStackedWidget->currentIndex() == UploadsList) {
            mUploadsToolsView->loadUploadTaskList();
        } else if(mTypeStackedWidget->currentIndex() == DownloadsList) {
            mDownloadsToolsView->loadDownloadTaskList();
        }
    }
}

void CloudToolsView::onAnimationTimeout()
{
    stopAnimation();
}

void CloudToolsView::onFileDownloadProgress(const OssDownloadParam &param)
{
    double progress = param.downloadSize * 1.0 / param.fileInfo.fileSize * 100;
    progress = qMax(1.0, progress);
    if (param.saveDir.isEmpty()) { //  非另存
        emit SignalCenter::instance()->updateBarIcon(param.fileInfo.fileId, "CloudTrans", progress, mMainWindow);
    }
    CloudFileDetailData itemData;
    itemData.fileId = param.fileInfo.fileId;
    int row = mListView->getModel()->itemRow(&itemData);
    if (row < 0) {
        return;
    }
    CloudFileDetailData *cloudData = dynamic_cast<CloudFileDetailData *>(mListView->getModel()->getItemByRow(row));
    cloudData->setProgress(progress);
    mListView->getModel()->updateItem(cloudData);
}

void CloudToolsView::onUpdateDiskInfo(const CloudDiskInfo &diskInfo)
{
    setDiskInfo(BaseFileDefine::formatShowStorage(diskInfo.usedSize) + " | " + BaseFileDefine::formatShowStorage(diskInfo.totalSize));
    setDiskCapacity(diskInfo.usedSize * 1.0 / diskInfo.totalSize * 100);
}

void CloudToolsView::onRenameFileFinished(const RenameStruct &renameStruct)
{
    CloudFileDetailData data;
    data.fileId = renameStruct.fileId;
    int row = mListView->getModel()->itemRow(&data);
    QString fileName = renameStruct.newName;
    if (fileName.contains(PDF_FILE_SUFFIX)) {
        fileName.replace(PDF_FILE_SUFFIX, "");
    }
    if (fileName.contains(PDF_FILE_UPSUFFIX)) {
        fileName.replace(PDF_FILE_UPSUFFIX, "");
    }
    if (row != -1) {
        BaseFileDetailData* baseData = mListView->getModel()->getItemByRow(row);
        baseData->fileName = fileName;
        mListView->getModel()->updateItem(baseData);
    }

    CloudFilePropInfo filePropInfo = mMainWindow->getCloudManager()->findFileInfoFromCache(renameStruct.fileId);
    if (filePropInfo.fileId <= 0) {
        return;
    }
    QString fileIdPath = QDir::fromNativeSeparators(CloudPredefine::workSpaceDir() + QDir::separator() + QString::number(renameStruct.fileId));
    QString filePath = QDir::fromNativeSeparators(fileIdPath + QDir::separator() + filePropInfo.fileShowName + PDF_FILE_SUFFIX);
    QString newPath = QDir::fromNativeSeparators(fileIdPath + QDir::separator() + renameStruct.newName);
    // 文件打开
    bool bCloseDoc = false;
    if (mMainWindow->isOpenCloudFile(renameStruct.fileId)) {
        mMainWindow->closeCloudFile(renameStruct.fileId);
        bCloseDoc = true;
    }
    if (QFile::exists(filePath)) {
        QFile::rename(filePath, newPath);
    }

    filePropInfo.fileShowName = fileName;
    filePropInfo.filePath = newPath;
    mMainWindow->getCloudManager()->updateFileInfoForCache(filePropInfo);

    if (bCloseDoc) {
        emit openFile(newPath, renameStruct.fileId);
    }
}

void CloudToolsView::onMoveToRecycleFinished(const DeleteOprStruct &deleteOpr)
{
    for (qint64 fileId : deleteOpr.fileIds) {
        CloudFileDetailData cloudItem;
        cloudItem.fileId = fileId;
        mListView->getModel()->removeItem(&cloudItem);
    }
    showStackWidget(mListView->getModel()->rowCount() > 0 ? FileView : EmptyFileView);
}

void CloudToolsView::showStackWidget(ViewType viewType)
{
    mStackedWidget->setCurrentIndex(viewType);
    toolsEnable(viewType != EmptyFileView);
    mRefreshBtn->setEnabled(true);
}

void CloudToolsView::toolsEnable(bool enable)
{
    mSortCombobox->setEnabled(enable);
    mListViewBtn->setEnabled(enable);
    mThumbnailBtn->setEnabled(enable);
//    mRemoveBtn->setEnabled(enable);
}

void CloudToolsView::setDiskInfo(const QString &text)
{
    mDiskInfoLabel->setText(text);
}

void CloudToolsView::setDiskCapacity(double ratio)
{
    mDiskCapacityBar->setValue(ratio);
}

void CloudToolsView::clearListView()
{
    mListView->getModel()->clearItems();
    dynamic_cast<RecycleBinToolsView *>(mMainStackedWidget->widget(RecycleView))->clearListView();
}

void CloudToolsView::locateItem(const CloudFilePropInfo &fileInfo)
{
    CloudFileDetailData data;
    data.fileId = fileInfo.fileId;
    int row = mListView->getModel()->itemRow(&data);
    if (row < 0) {
        return;
    }
    mMainStackedWidget->setCurrentIndex(CloudView);
    QModelIndex modelIndex = mListView->getModel()->currentIndexByRow(row);
    mListView->scrollTo(modelIndex);
    mListView->setCurrentIndex(modelIndex);

    BaseFileDetailData *baseData = mListView->getModel()->getItemByRow(row);
    if (!baseData) {
        return;
    }
    baseData->setHighlight(true);
    mListView->getModel()->updateItem(baseData);
    QTimer::singleShot(800, this, [=] {
        BaseFileDetailData *baseData = mListView->getModel()->getItemByRow(row);
        if (!baseData) {
            return;
        }
        baseData->setHighlight(false);
        mListView->getModel()->updateItem(baseData);
    });
}

//////////////////////////
UploadFileWidget::UploadFileWidget(QWidget *parent ):QWidget(parent)
{
    setWindowFlags(Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint);
    setFixedSize(808,96); // 488
    initView();
}

QStringList UploadFileWidget::getFilePathList()
{
    QStringList list;
    for(int i = 0;i<42; i++){
        list.push_back(QString(":/updf-win-icon/ui2/2x/welcome-page/cloudseq/Comp 1_%1.png").arg(QString::number(i).rightJustified(5, '0')));
    }

    return list;
}

QGraphicsDropShadowEffect* UploadFileWidget::getShadowEffect()
{
    return mShadow;
}

void UploadFileWidget::runAnimation(bool left)
{
    QPoint startPos, endPos;
    QPropertyAnimation *pOpacityAnimation = new QPropertyAnimation(mOpenBtn, "pos");
    if(!left){
        mOpenBtn->move(mOldPoint);//快速进出 按钮复原
        startPos = mOpenBtn->pos();
        endPos = startPos + QPoint(8,0);
        pOpacityAnimation->setDuration(100);
    } else{
        startPos = mOpenBtn->pos();
        endPos = startPos + QPoint(-8,0);
        pOpacityAnimation->setDuration(60);
    }
    pOpacityAnimation->setStartValue(startPos);
    QEasingCurve easingCurve;
    easingCurve.addCubicBezierSegment(QPointF(0,0),QPointF(0.6,0.6),QPointF(1.0,1.0));
    pOpacityAnimation->setEasingCurve(QEasingCurve::InOutQuad);
    pOpacityAnimation->setEndValue(endPos);
    pOpacityAnimation->start(QPropertyAnimation::DeleteWhenStopped);
}

void UploadFileWidget::initView()
{
    mShadow = new QGraphicsDropShadowEffect(this);
    mShadow->setOffset(0, 4);
    mShadow->setColor(QColor(0,0,0,0));
    mShadow->setBlurRadius(16);
    setGraphicsEffect(mShadow);
    auto mainLayout = LayoutUtil::CreateLayout(Qt::Horizontal,0,0);
    auto bkFrame = new QFrame();
    bkFrame->setObjectName("openFilebkFrame");
    auto hLayout = new QHBoxLayout();
    hLayout->setSpacing(0);
    hLayout->setContentsMargins(24,8,32,8);
    auto leftFrame = new QFrame();
    leftFrame->setFixedHeight(64);
    auto leftLayout = new QHBoxLayout();
    leftFrame->setLayout(leftLayout);
    leftLayout->setSpacing(8);
    leftLayout->setMargin(0);
    auto openFrame = new QFrame();
    openFrame->setFixedHeight(40);
    auto openLayout = new QVBoxLayout();
    openLayout->setSpacing(0);
    openLayout->setMargin(0);
    openFrame->setLayout(openLayout);
    auto frame = new QFrame();
    frame->setFixedSize(44,52);

    auto layout = new QVBoxLayout();
    layout->setSpacing(0);
    layout->setContentsMargins(0,0,0,8);
    frame->setLayout(layout);
    auto openWin = new sequenceFrameAnimation(QSize(46,56));
    layout->addWidget(openWin,0,Qt::AlignTop);
    openWin->initData(getFilePathList());
    openWin->start();
    auto titleLab = new QLabel(tr("Upload File"));
    QFont font = QApplication::font();
    font.setWeight(60);
    titleLab->setFont(font);
    titleLab->setObjectName("titleLab");
    auto tipLab = new QLabel(tr("Drag and drop file here to upload"));
    tipLab->setObjectName("tipLab");
    openLayout->addWidget(titleLab);
    openLayout->addWidget(tipLab);
    leftLayout->addWidget(frame,0,Qt::AlignVCenter);
    leftLayout->addWidget(openFrame);
    mOpenBtn = new QPushButton();
    mOpenBtn->setFixedSize(20,20);
    mOpenBtn->setObjectName("openBtn");
    connect(mOpenBtn, &QPushButton::clicked, this, &UploadFileWidget::openDir);

    hLayout->addWidget(leftFrame,0,Qt::AlignVCenter);
    hLayout->addStretch();
    hLayout->addWidget(mOpenBtn,0,Qt::AlignVCenter);

    bkFrame->setLayout(hLayout);
    mainLayout->addWidget(bkFrame);
    mainLayout->setMargin(0);
    setLayout(mainLayout);
}

void UploadFileWidget::enterEvent(QEvent *event)
{
    mShadow->setColor(QColor(0,0,0,0));
    runAnimation(false);
    QWidget::enterEvent(event);
}

void UploadFileWidget::leaveEvent(QEvent *event)
{
    runAnimation(true);
    mShadow->setColor(QColor(0,0,0,0));
    QWidget::leaveEvent(event);
}

void UploadFileWidget::mouseReleaseEvent(QMouseEvent *event)
{
    emit openDir();
    QWidget::mouseReleaseEvent(event);
}

void UploadFileWidget::showEvent(QShowEvent *event)
{
    mOldPoint = mOpenBtn->pos();
    QWidget::showEvent(event);
}


SortItemCell::SortItemCell(int type, const QString &text, QWidget *parent)
    : QWidget(parent)
    , mType(type)
    , mText(text)
    , mChecked(false)
{
    setFixedHeight(32);
    mBGLabel = new QLabel(this);
    mBGLabel->setObjectName("bgLabel");
    mTextLable = new QLabel(this);
    mTextLable->setText(mText);
    mTextLable->setObjectName("textLabel");
    mTextLable->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    mIconLable = new QLabel(this);
    mIconLable->setObjectName("iconLabel");
    mIconLable->setFixedSize(24, 24);
    QHBoxLayout *layout = new QHBoxLayout(mBGLabel);
    layout->setContentsMargins(16, 4, 16, 4);
    layout->addWidget(mTextLable);
    layout->addStretch();
    layout->addWidget(mIconLable);

    QHBoxLayout *contentlayout = new QHBoxLayout();
    contentlayout->setSpacing(0);
    contentlayout->setMargin(0);
    contentlayout->addWidget(mBGLabel);
    setLayout(contentlayout);

    setAttribute(Qt::WA_Hover);
    installEventFilter(this);
}

void SortItemCell::setChecked(bool check)
{
    mChecked = check;
    mIconLable->setProperty("checked", check);
    mIconLable->style()->unpolish(mIconLable);
    mIconLable->style()->polish(mIconLable);
}

bool SortItemCell::isChecked()
{
    return mChecked;
}

void SortItemCell::setHoverStyle(bool hover)
{
    setProperty("hover", hover);
    mBGLabel->style()->unpolish(mBGLabel);
    mBGLabel->style()->polish(mBGLabel);
}

int SortItemCell::type()
{
    return mType;
}

QString SortItemCell::text()
{
    return mText;
}

void SortItemCell::setText(const QString &text)
{
    mTextLable->setText(text);
}

bool SortItemCell::eventFilter(QObject *watched, QEvent *event)
{
    if (this == watched) {
        if (event->type() == QEvent::HoverEnter) {
//            setHoverStyle(true);
            mBGLabel->setStyleSheet("background: #EAEAEA;border-radius: 4px;");
        } else if (event->type() == QEvent::HoverLeave) {
//            setHoverStyle(false);
            mBGLabel->setStyleSheet("background: transparent;");
        }
    }
    return QWidget::eventFilter(watched, event);
}

void SortItemCell::mousePressEvent(QMouseEvent *event)
{
    QWidget::mousePressEvent(event);
    if (!mChecked) {
        setChecked(true);
        emit checked();
    }
}

SortTypeWidget::SortTypeWidget(QWidget *parent)
    : FloatingWidget(parent)
{
    const auto language = SettingUtil::language();
    if ("fr" == language) {
        setFixedSize(220 + 16, 206 + 16);
        visiblePanel()->setFixedSize(220, 206);
    } else {
        setFixedSize(190 + 16, 206 + 16);
        visiblePanel()->setFixedSize(190, 206);
    }

    initView();
    visiblePanel()->setStyleSheet("background:#F7F7F7;");
}

void SortTypeWidget::initView()
{
    SortItemCell *timeCell = new SortItemCell(SortNameType::Time, tr("Last modified"));
    SortItemCell *nameCell = new SortItemCell(SortNameType::FileName, tr("File name"));
    SortItemCell *sizeCell = new SortItemCell(SortNameType::FileSize, tr("File size"));
    timeCell->setChecked(true);
    mCheckedType = SortNameType::Time;
    mTypeGroup.append(timeCell);
    mTypeGroup.append(nameCell);
    mTypeGroup.append(sizeCell);
    for (SortItemCell *itemCell : mTypeGroup) {
        connect(itemCell, &SortItemCell::checked, this, &SortTypeWidget::onTypeChecked);
    }
    SortItemCell *ascCell = new SortItemCell(SortNameType::Asc, tr("Newest first"));
    SortItemCell *descCell = new SortItemCell(SortNameType::Desc, tr("Oldest first"));
    ascCell->setChecked(true);
    mCheckedDirection = SortNameType::Asc;
    mDirectionGroup.append(ascCell);
    mDirectionGroup.append(descCell);
    for (SortItemCell *itemCell : mDirectionGroup) {
        connect(itemCell, &SortItemCell::checked, this, &SortTypeWidget::onDirectionChecked);
    }
    QFrame *lineFrame = new QFrame(this);
    lineFrame->setFixedHeight(1);
    lineFrame->setStyleSheet("background:#EAEAEA;");
    QVBoxLayout *layout = new QVBoxLayout;
    layout->setContentsMargins(8, 8, 8, 8);
    layout->addWidget(timeCell);
    layout->addWidget(nameCell);
    layout->addWidget(sizeCell);
    layout->addWidget(lineFrame);
    layout->addWidget(ascCell);
    layout->addWidget(descCell);
    visiblePanel()->setLayout(layout);
}

void SortTypeWidget::onTypeChecked()
{
    SortItemCell *checkedItemCell = qobject_cast<SortItemCell *>(sender());
    for (SortItemCell *itemCell : mTypeGroup) {
        if (checkedItemCell != itemCell && itemCell->isChecked()) {
            itemCell->setChecked(false);
        }
    }
    mCheckedType = checkedItemCell->type();
    emit sortInfo(mCheckedType, mCheckedDirection);
    emit textChanged(checkedItemCell->text());
    switch (checkedItemCell->type()) {
    case SortNameType::Time:
        mDirectionGroup.value(0)->setText(tr("Newest first"));
        mDirectionGroup.value(1)->setText(tr("Oldest first"));
        break;
    case SortNameType::FileName:
        mDirectionGroup.value(0)->setText(tr("A to Z"));
        mDirectionGroup.value(1)->setText(tr("Z to A"));
        break;
    case SortNameType::FileSize:
        mDirectionGroup.value(0)->setText(tr("Smallest first"));
        mDirectionGroup.value(1)->setText(tr("Largest first"));
        break;
    default:
        break;
    }

}
void SortTypeWidget::onDirectionChecked()
{
    SortItemCell *checkedItemCell = qobject_cast<SortItemCell *>(sender());
    for (SortItemCell *itemCell : mDirectionGroup) {
        if (checkedItemCell != itemCell && itemCell->isChecked()) {
            itemCell->setChecked(false);
        }
    }
    mCheckedDirection = checkedItemCell->type();
    emit sortInfo(mCheckedType, mCheckedDirection);
}

CheckComboBox::CheckComboBox(QWidget *parent)
{
    mView = new SortTypeWidget(this);
    connect(mView, &SortTypeWidget::sortInfo, this, [=](int type, int direction) {
        emit sortTypeChanged(direction - 10 + type);
    });
    connect(mView, &SortTypeWidget::textChanged, this, [=](const QString &text) {
        setCurrentText(text);
    });
    mLineEdit = new LineEdit(this);
    connect(mLineEdit, &LineEdit::mousePressed, this, [=] {
        showPopup();
    });
    mLineEdit->setContextMenuPolicy(Qt::NoContextMenu);
    mLineEdit->setReadOnly(true);
    mLineEdit->setStyleSheet("border:hidden");
    setLineEdit(mLineEdit);

    mView->hide();
    setCurrentText(tr("Last modified"));
    setMinimumWidth(DrawUtil::textWidth(this->font(),mLineEdit->text()) + 50);
    setContextMenuPolicy(Qt::NoContextMenu);
}

void CheckComboBox::setCurrentText(const QString &text)
{
    mLineEdit->setText(text);
}

void CheckComboBox::showPopup()
{
    QPoint pos = mapToGlobal(QPoint(0,0));
    mView->show();

    if (mView->width() > width()) {
        int xOffset = (mView->width() - width()) / 2;

        mView->move(pos + QPoint(-xOffset, 32));
    } else {
        mView->move(pos.x(), pos.y() + 32);
    }
}

void CheckComboBox::hidePopup()
{
    mView->hide();
}
