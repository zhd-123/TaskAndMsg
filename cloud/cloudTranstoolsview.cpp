#include "cloudtranstoolsview.h"
#include "cloudlistview.h"
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QComboBox>
#include <QStackedWidget>
#include <QDir>
#include <QMimeData>

#include "./src/mainwindow.h"
#include "serializeconfig.h"
#include "cloudlistview.h"
#include "./src/util/guiutil.h"
#include "./src/customwidget/chimgandtextbtn.h"
#include "./utils/settingutil.h"
#include "./src/style/stylesheethelper.h"
#include "./src/cloud/cloudmanager.h"
#include "./src/cloud/transportcontroller.h"
#include "./src/basicwidget/scrollareawidget.h"
#include "./src/pageengine/pdfcore/PDFDocument.h"
#include "umenu.h"

CloudUploadsToolsView::CloudUploadsToolsView(MainWindow* mainWindow,QWidget *parent)
    : QWidget{parent}
    , mMainWindow(mainWindow)
    , mListView(nullptr)
    , mRetryUpload(false)
{
    initView();
    setAcceptDrops(false);
    connect(mMainWindow->getTransportController(), &TransportController::uploadProgress, this, &CloudUploadsToolsView::onFileUploadProgress);
    connect(mMainWindow->getTransportController(), &TransportController::ossUploadResult, this, &CloudUploadsToolsView::onOssUploadResult);
    connect(mMainWindow->getTransportController(), &TransportController::transferStateSignal, this, &CloudUploadsToolsView::onTransferState);
    connect(mMainWindow->getCloudManager(), &CloudManager::insertToUploadListView, this, &CloudUploadsToolsView::onInsertListView);
    connect(mMainWindow->getCloudManager(), &CloudManager::updateListItem, this, &CloudUploadsToolsView::onUpdateListItem);
    connect(mMainWindow->getCloudManager(), &CloudManager::removeListItem, this, &CloudUploadsToolsView::onRemoveListItem);
}

CloudUploadsToolsView::~CloudUploadsToolsView()
{
    if(mListView != nullptr){
        mListView->deleteLater();
    }
}

void CloudUploadsToolsView::initView()
{
    mStackedWidget = new QStackedWidget();
    mStackedWidget->setStyleSheet("background:#FBFBFB;");

    mListView = new CloudTransListView(this);
    connect(mListView,&CloudTransListView::itemClick,this,[=](BaseFileDetailData *data){
        onOpenFile(data); // 点击打开文件
    });
    connect(mListView,&CloudTransListView::popMenu,this,[=](BaseFileDetailData *data){
        onOpenMenu(data);
    });
    mStackedWidget->insertWidget(EmptyFileView, createEmptyFileWidget());

    QWidget *viewContainer = new QWidget(this);
    MAKE_SCROLLBAR_CONTAINER(mListView, 6, viewContainer)
    mStackedWidget->insertWidget(FileView, viewContainer);

    QVBoxLayout* layout = new QVBoxLayout();
    layout->setMargin(0);
    layout->setSpacing(0);
    layout->addWidget(mStackedWidget);
    setLayout(layout);
}

QFrame* CloudUploadsToolsView::createEmptyFileWidget()
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
    mTipsLabel = new QLabel(tr("No files are currently being uploaded."));
    mTipsLabel->setObjectName("tipsLabel");
    mTipsLabel->setFont(font);
    mTipsLabel->setAlignment(Qt::AlignHCenter);
    mTipsLabel1 = new QLabel(tr("Files being uploaded will appear here."));
    mTipsLabel1->setObjectName("tipsLabel1");
    mTipsLabel1->setAlignment(Qt::AlignHCenter);
    tipLayout->addWidget(mTipsLabel,0,Qt::AlignHCenter);
    tipLayout->addWidget(mTipsLabel1,0,Qt::AlignHCenter);

    vlayout->addItem(new QSpacerItem(0,0,QSizePolicy::Minimum,QSizePolicy::Expanding));
    vlayout->addWidget(mIconLabel,0,Qt::AlignCenter);
    vlayout->addWidget(tipFrame);
    vlayout->addSpacing(30);
    vlayout->addItem(new QSpacerItem(0,0,QSizePolicy::Minimum,QSizePolicy::Expanding));

    frame->setLayout(vlayout);
    return frame;
}

void CloudUploadsToolsView::showStackWidget(ViewType viewType)
{
    mStackedWidget->setCurrentIndex(viewType);
}

void CloudUploadsToolsView::removeUploadItem(BaseFileDetailData *data)
{
    CloudFileDetailData *cloudData = dynamic_cast<CloudFileDetailData *>(data);
    if (!cloudData) {
        return;
    }
    mListView->getModel()->removeItem(data);
    emit enableViewModeButton(mListView->getModel()->rowCount() > 0);
    if(mListView->getModel()->rowCount() > 0) {
        showStackWidget(FileView);
    } else {
        showStackWidget(EmptyFileView);
    }
}

void CloudUploadsToolsView::setViewMode(int viewMode)
{
    mListView->setListViewMode((QListView::ViewMode)viewMode);
    mListView->update();
}

void CloudUploadsToolsView::refreshList()
{
    for (QString configPath : mFailFileConfigMap.values()) {
        mRetryUpload = true;
        mMainWindow->getCloudManager()->resumeUpload(configPath, true);
        OssUploadParam uploadParam = SerializeConfig::instance()->readUploadParam(configPath);
        CloudFileDetailData detailData;
        detailData.fileId = uploadParam.fileInfo.fileId;
        int row = mListView->getModel()->itemRow(&detailData);
        if (row >= 0) {
            BaseFileDetailData* data = mListView->getModel()->getItemByRow(row);
            CloudFileDetailData* cloudData = dynamic_cast<CloudFileDetailData*>(data);
            emit mMainWindow->getTransportController()->transferStateSignal(cloudData->fileId, WaitTransfer);
        }
    }
    mFailFileConfigMap.clear();
    emit enableFreshButton(false);
}

void CloudUploadsToolsView::loadUploadTaskList()
{
    if (mRetryUpload) { // 点击重试，不需要更新列表
        mRetryUpload = false;
        return;
    }
    mFailFileConfigMap.clear();
    mListView->getModel()->clearItems();
    QMap<QString, QString> taskMap = SerializeConfig::instance()->loadPauseTask();
    for (QString key : taskMap.keys()) {
        if (key.contains(Upload_File_Prefix)) {
            QString configPath = taskMap.value(key);
            OssUploadParam uploadParam = SerializeConfig::instance()->readUploadParam(configPath);
            QString fileIdPath = QDir::fromNativeSeparators(CloudPredefine::workSpaceDir() + QDir::separator() + QString::number(uploadParam.fileInfo.fileId));
            if (uploadParam.fileInfo.filePath.contains(fileIdPath)) {
                continue; // 云同步任务
            }
            if (uploadParam.fileInfo.transFail) {
                mFailFileConfigMap.insert(uploadParam.fileInfo.fileId, configPath);
            }
            mCacheFileConfigMap.insert(uploadParam.fileInfo.fileId, configPath);
            addUploadItem(uploadParam.fileInfo);
        }
    }
    mMainWindow->getCloudManager()->addWaitUploadListView(SerializeConfig::instance()->loadWaitUploadFile(), false);

    emit enableFreshButton(mFailFileConfigMap.size() > 0);
    emit enableViewModeButton(mListView->getModel()->rowCount() > 0);
    showStackWidget(mListView->getModel()->rowCount() > 0 ? FileView : EmptyFileView);

}

void CloudUploadsToolsView::onInsertListView(const CloudFilePropInfo &fileInfo)
{
    CloudFileDetailData *detailData = new CloudFileDetailData(fileInfo.fileId, 0, fileInfo.fileShowName, fileInfo.filePath,
                                                            fileInfo.logoPath, QDateTime(), QDateTime(),
                                                            fileInfo.currentPageIndex,fileInfo.totalPage, fileInfo.fileSize,
                                                            false, false, false, fileInfo.havePwd);
    detailData->setTransFerState(WaitTransfer);
    mListView->getModel()->insertItem(0, detailData);
}

void CloudUploadsToolsView::onUpdateListItem(const CloudFilePropInfo &fileInfo)
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

void CloudUploadsToolsView::removeListItem(qint64 fileId)
{
    mFailFileConfigMap.remove(fileId);
    mCacheFileConfigMap.remove(fileId);
    CloudFileDetailData cloudData;
    cloudData.fileId = fileId;
    int row = mListView->getModel()->itemRow(&cloudData);
    if (row < 0) {
        return;
    }
    removeUploadItem(mListView->getModel()->getItemByRow(row));
}

void CloudUploadsToolsView::onRemoveListItem(const QString &filePath)
{
    BaseFileDetailData baseItem;
    baseItem.filePath = filePath;
    int row = mListView->getModel()->itemRow(&baseItem);
    if (row < 0) {
        return;
    }
    removeUploadItem(mListView->getModel()->getItemByRow(row));
}

void CloudUploadsToolsView::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    loadUploadTaskList();
}

void CloudUploadsToolsView::onOpenFile(BaseFileDetailData *data)
{
    CloudFileDetailData *cloudData = dynamic_cast<CloudFileDetailData *>(data);
    if (!cloudData) {
        return;
    }
}

void CloudUploadsToolsView::onOpenMenu(BaseFileDetailData *data)
{
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
    if (cloudData->getTransFerState() == TransferFail) {
        actionGroup->addAction(action_retry);
    }
    if (cloudData->getTransFerState() == Transferring) {
        actionGroup->addAction(action_cancel);
    }
    actionGroup->addAction(action_delete);
    connect(actionGroup,&QActionGroup::triggered,this,[&](QAction *action){
        CloudFileDetailData *cloudData = dynamic_cast<CloudFileDetailData *>(data);
        if (!cloudData) {
            return;
        }
        if(action == action_retry){
            if (mFailFileConfigMap.contains(cloudData->fileId)) {
                mRetryUpload = true;
                mMainWindow->getCloudManager()->resumeUpload(mFailFileConfigMap.take(cloudData->fileId), true);
                emit mMainWindow->getTransportController()->transferStateSignal(cloudData->fileId, WaitTransfer);
            }
            emit enableFreshButton(mFailFileConfigMap.size() > 0);

        } else if(action == action_cancel){
            mMainWindow->getTransportController()->abortFileUpload(cloudData->fileId);
            emit mMainWindow->getTransportController()->transferStateSignal(cloudData->fileId, TransferFail);
            mMainWindow->getCloudManager()->finishUploadTask();
        } else if(action == action_delete){
            // ucloud 进行itemRemove
            if (cloudData->fileId <= 0) {
                emit removeItemByPath(cloudData->filePath);
                mMainWindow->getCloudManager()->removeWaitForUploadFileList(cloudData->filePath);
                SerializeConfig::instance()->removeWaitUploadFile(cloudData->filePath);
                onRemoveListItem(cloudData->filePath);
            } else {
                emit removeItem(cloudData->fileId);
                mMainWindow->getCloudManager()->removeWaitForUploadFileList(cloudData->filePath);
                SerializeConfig::instance()->removeWaitUploadFile(cloudData->filePath);
                SerializeConfig::instance()->removeTransportTask(cloudData->fileId, true);
                mMainWindow->getTransportController()->abortFileUpload(cloudData->fileId);
                emit mMainWindow->getTransportController()->transferStateSignal(cloudData->fileId, TransferFail);
                removeListItem(cloudData->fileId);
            }
            mMainWindow->getCloudManager()->finishUploadTask();
        }
    });
    for(auto action:actionGroup->actions()){
        menu.addAction(action);
    }
    menu.setAnimationPoint(QCursor::pos());
}

void CloudUploadsToolsView::onFileUploadProgress(const OssUploadParam &param)
{
    CloudFileDetailData itemData;
    itemData.fileId = param.fileInfo.fileId;
    int row = mListView->getModel()->itemRow(&itemData);
    if (row < 0) {
        return;
    }
    CloudFileDetailData *cloudData = dynamic_cast<CloudFileDetailData *>(mListView->getModel()->getItemByRow(row));
    cloudData->setProgress(param.uploadSize * 1.0 / param.fileInfo.fileSize * 100);
    mListView->getModel()->updateItem(cloudData);
}

void CloudUploadsToolsView::onOssUploadResult(bool success, const OssUploadParam &param)
{
    CloudFileDetailData data;
    data.fileId = param.fileInfo.fileId;
    int row = mListView->getModel()->itemRow(&data);
    if (row == -1) {
        return;
    }
    BaseFileDetailData* baseData = mListView->getModel()->getItemByRow(row);
    CloudFileDetailData *cloudData = dynamic_cast<CloudFileDetailData *>(baseData);
    if (!success) {
        cloudData->setTransFerState(TransferFail);
        mListView->getModel()->updateItem(cloudData);
        if (mCacheFileConfigMap.contains(param.fileInfo.fileId)) {
            mFailFileConfigMap.insert(param.fileInfo.fileId, mCacheFileConfigMap.value(param.fileInfo.fileId));
        }
        emit enableFreshButton(true);
    } else {
        cloudData->setTransFerState(TransferSuccess);
    }
}
void CloudUploadsToolsView::onTransferState(qint64 fileId, TransferState state)
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

void CloudUploadsToolsView::addUploadItem(const CloudFilePropInfo &fileInfo)
{
    CloudFileDetailData *detailData = new CloudFileDetailData(fileInfo.fileId, 0, fileInfo.fileShowName, fileInfo.filePath,
                                                            fileInfo.logoPath, QDateTime(), QDateTime(),
                                                            fileInfo.currentPageIndex,fileInfo.totalPage, fileInfo.fileSize,
                                                            false, false, false, fileInfo.havePwd);
    if (fileInfo.transFail) {
        detailData->setTransFerState(TransferFail);
    } else {
        detailData->setTransFerState(WaitTransfer);
    }

    int row = mListView->getModel()->itemRow(detailData);
    if (row != -1) {
        mListView->getModel()->removeItem(row);
    }
    mListView->getModel()->appendItem(detailData);
}

/////////////////////


CloudDownloadsToolsView::CloudDownloadsToolsView(MainWindow* mainWindow,QWidget *parent)
    : QWidget{parent}
    , mMainWindow(mainWindow)
    , mListView(nullptr)
{
    initView();
    setAcceptDrops(false);
    connect(mMainWindow->getTransportController(), &TransportController::downloadProgress, this, &CloudDownloadsToolsView::onFileDownloadProgress);
    connect(mMainWindow->getTransportController(), &TransportController::ossDownloadResult, this, &CloudDownloadsToolsView::onOssDownloadResult);
    connect(mMainWindow->getTransportController(), &TransportController::transferStateSignal, this, &CloudDownloadsToolsView::onTransferState);
    connect(mMainWindow->getCloudManager(), &CloudManager::cloudFileError, this, &CloudDownloadsToolsView::onCloudFileError);

}

CloudDownloadsToolsView::~CloudDownloadsToolsView()
{
    if(mListView != nullptr){
        mListView->deleteLater();
    }
}

void CloudDownloadsToolsView::initView()
{
    mStackedWidget = new QStackedWidget();
    mStackedWidget->setStyleSheet("background:#FBFBFB;");

    mListView = new CloudTransListView(this);
    connect(mListView,&CloudTransListView::itemClick,this,[=](BaseFileDetailData *data){
        onOpenFile(data); // 点击打开文件
    });
    connect(mListView,&CloudTransListView::popMenu,this,[=](BaseFileDetailData *data){
        onOpenMenu(data);
    });
    mStackedWidget->insertWidget(EmptyFileView, createEmptyFileWidget());
    CloudTransItemDelegate* mItemDelegate = qobject_cast<CloudTransItemDelegate*>(mListView->itemDelegate());
    mItemDelegate->setCheckFileExist(true);

    QWidget *viewContainer = new QWidget(this);
    MAKE_SCROLLBAR_CONTAINER(mListView, 6, viewContainer)
    mStackedWidget->insertWidget(FileView, viewContainer);

    QVBoxLayout* layout = new QVBoxLayout();
    layout->setMargin(0);
    layout->setSpacing(0);
    layout->addWidget(mStackedWidget);
    setLayout(layout);
}

QFrame* CloudDownloadsToolsView::createEmptyFileWidget()
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
    mTipsLabel = new QLabel(tr("No download records yet."));
    mTipsLabel->setObjectName("tipsLabel");
    mTipsLabel->setFont(font);
    mTipsLabel->setAlignment(Qt::AlignHCenter);
    mTipsLabel1 = new QLabel(tr("Files downloaded from the cloud to local will appear here."));
    mTipsLabel1->setObjectName("tipsLabel1");
    mTipsLabel1->setAlignment(Qt::AlignHCenter);
    tipLayout->addWidget(mTipsLabel,0,Qt::AlignHCenter);
    tipLayout->addWidget(mTipsLabel1,0,Qt::AlignHCenter);

    vlayout->addItem(new QSpacerItem(0,0,QSizePolicy::Minimum,QSizePolicy::Expanding));
    vlayout->addWidget(mIconLabel,0,Qt::AlignCenter);
    vlayout->addWidget(tipFrame);
    vlayout->addSpacing(30);
    vlayout->addItem(new QSpacerItem(0,0,QSizePolicy::Minimum,QSizePolicy::Expanding));

    frame->setLayout(vlayout);
    return frame;
}

void CloudDownloadsToolsView::showStackWidget(ViewType viewType)
{
    mStackedWidget->setCurrentIndex(viewType);
}

void CloudDownloadsToolsView::removeDownloadItem(BaseFileDetailData *data)
{
    // 移除任务
    CloudFileDetailData *cloudData = dynamic_cast<CloudFileDetailData *>(data);
    if (!cloudData) {
        return;
    }
    SerializeConfig::instance()->removeTransportTask(cloudData->fileId, false);

    SerializeConfig::instance()->removeSaveAsRecord(cloudData->fileId, cloudData->filePath);
    mListView->getModel()->removeItem(data);
    emit enableViewModeButton(mListView->getModel()->rowCount() > 0);
    if(mListView->getModel()->rowCount() > 0) {
        showStackWidget(FileView);
    } else {
        showStackWidget(EmptyFileView);
    }
}

void CloudDownloadsToolsView::setViewMode(int viewMode)
{
    mListView->setListViewMode((QListView::ViewMode)viewMode);
    mListView->update();
}

void CloudDownloadsToolsView::refreshList()
{
    for (QString configPath : mFailFileConfigMap.values()) {
        mMainWindow->getCloudManager()->resumeDownload(configPath, true);
        OssDownloadParam downloadParam = SerializeConfig::instance()->readDownloadParam(configPath);
        CloudFileDetailData detailData;
        detailData.fileId = downloadParam.fileInfo.fileId;
        int row = mListView->getModel()->itemRow(&detailData);
        if (row >= 0) {
            BaseFileDetailData* data = mListView->getModel()->getItemByRow(row);
            CloudFileDetailData* cloudData = dynamic_cast<CloudFileDetailData*>(data);
            onTransferState(cloudData->fileId, WaitTransfer);
        }
    }
    mFailFileConfigMap.clear();
    emit enableFreshButton(false);
}

void CloudDownloadsToolsView::loadDownloadTaskList()
{
    mFailFileConfigMap.clear();
    mListView->getModel()->clearItems();
    CloudFilePropInfos propInfos = SerializeConfig::instance()->loadSaveAsRecord();
    for (CloudFilePropInfo propInfo : propInfos) {
        addDownloadItem(propInfo, true);
    }

    QMap<QString, QString> taskMap = SerializeConfig::instance()->loadPauseTask();
    for (QString key : taskMap.keys()) {
        if (key.contains(Download_File_Prefix)) {
            QString configPath = taskMap.value(key);
            if (!QFileInfo(configPath).exists()) {
                SerializeConfig::instance()->removeTransportTask(key.replace(Download_File_Prefix, "").toLongLong(), false);
                continue;
            }
            OssDownloadParam downloadParam = SerializeConfig::instance()->readDownloadParam(configPath);
            if (downloadParam.saveDir.isEmpty()) {
                continue; // 云同步任务
            }
            if (downloadParam.fileInfo.transFail) {
                mFailFileConfigMap.insert(downloadParam.fileInfo.fileId, configPath);
            }
            mCacheFileConfigMap.insert(downloadParam.fileInfo.fileId, configPath);
            downloadParam.fileInfo.filePath = QDir::fromNativeSeparators(downloadParam.saveDir + QDir::separator() + downloadParam.fileInfo.fileShowName + PDF_FILE_SUFFIX);
            addDownloadItem(downloadParam.fileInfo, false);
        }
    }

    emit enableFreshButton(mFailFileConfigMap.size() > 0);
    emit enableViewModeButton(mListView->getModel()->rowCount() > 0);
    showStackWidget(mListView->getModel()->rowCount() > 0 ? FileView : EmptyFileView);
}

void CloudDownloadsToolsView::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    loadDownloadTaskList();
}

void CloudDownloadsToolsView::onOpenFile(BaseFileDetailData *data)
{
    CloudFileDetailData *cloudData = dynamic_cast<CloudFileDetailData *>(data);
    if (!cloudData) {
        return;
    }
}

void CloudDownloadsToolsView::onOpenMenu(BaseFileDetailData *data)
{
    UMenu menu;
    GuiUtil::setMenuRoundStyle(&menu);

    QAction* action_retry = new QAction(tr("Retry"));
    QAction* action_cancel = new QAction(tr("Cancel"));
    QAction* action_locate = new QAction(tr("Locate"));
    QAction* action_delete = new QAction(tr("Delete"));

    QActionGroup* actionGroup = new QActionGroup(&menu);
    CloudFileDetailData *cloudData = dynamic_cast<CloudFileDetailData *>(data);
    if (!cloudData) {
        return;
    }
    if (cloudData->getTransFerState() == TransferFail) {
        actionGroup->addAction(action_retry);
    }
    if (QFileInfo(cloudData->filePath).exists() && cloudData->getTransFerState() == TransferSuccess) {
        actionGroup->addAction(action_locate);
    }
    if (cloudData->getTransFerState() == Transferring) {
        actionGroup->addAction(action_cancel);
    }
    actionGroup->addAction(action_delete);

    connect(actionGroup,&QActionGroup::triggered,this,[&](QAction *action){
        CloudFileDetailData *cloudData = dynamic_cast<CloudFileDetailData *>(data);
        if (!cloudData) {
            return;
        }
        if(action == action_retry){
            if (mFailFileConfigMap.contains(cloudData->fileId)) {
                mMainWindow->getCloudManager()->resumeDownload(mFailFileConfigMap.take(cloudData->fileId), true);
                onTransferState(cloudData->fileId, WaitTransfer);
            }
            emit enableFreshButton(mFailFileConfigMap.size() > 0);

        } else if(action == action_cancel){
            mMainWindow->getTransportController()->abortFileDownload(cloudData->fileId);
        }  else if(action == action_delete){
            mMainWindow->getTransportController()->abortFileDownload(cloudData->fileId);
            removeDownloadItem(cloudData);
        } else if(action == action_locate){
            GuiUtil::openFilePath(cloudData->filePath);
        }
    });
    for(auto action:actionGroup->actions()){
        menu.addAction(action);
    }
    menu.setAnimationPoint(QCursor::pos());
}

void CloudDownloadsToolsView::onFileDownloadProgress(const OssDownloadParam &param)
{
    CloudFileDetailData itemData;
    itemData.fileId = param.fileInfo.fileId;
    int row = mListView->getModel()->itemRow(&itemData);
    if (row < 0) {
        return;
    }
    CloudFileDetailData *cloudData = dynamic_cast<CloudFileDetailData *>(mListView->getModel()->getItemByRow(row));
    cloudData->setProgress(param.downloadSize * 1.0 / param.fileInfo.fileSize * 100);
    mListView->getModel()->updateItem(cloudData);
}

void CloudDownloadsToolsView::onOssDownloadResult(bool success, const OssDownloadParam &param)
{
    CloudFileDetailData data;
    data.fileId = param.fileInfo.fileId;
    int row = mListView->getModel()->itemRow(&data);
    if (row == -1) {
        return;
    }
    BaseFileDetailData* baseData = mListView->getModel()->getItemByRow(row);
    CloudFileDetailData *cloudData = dynamic_cast<CloudFileDetailData *>(baseData);
    if (!cloudData) {
        return;
    }

    if (!success) { // 失败后可以retry
        if (param.fileInfo.transFail) {
            cloudData->setTransFerState(TransferFail);
            if (mCacheFileConfigMap.contains(param.fileInfo.fileId)) {
                mFailFileConfigMap.insert(param.fileInfo.fileId, mCacheFileConfigMap.value(param.fileInfo.fileId));
            }
            emit enableFreshButton(true);
            mListView->getModel()->updateItem(cloudData);
        }
    } else {
        QTimer::singleShot(1000, this, [&, row] {
            BaseFileDetailData* baseData = mListView->getModel()->getItemByRow(row);
            CloudFileDetailData *cloudData = dynamic_cast<CloudFileDetailData *>(baseData);
            if (!cloudData) {
                return;
            }
            cloudData->setTransFerState(TransferSuccess);
            cloudData->setFileWriteDown(true);
            mListView->getModel()->updateItem(cloudData);
        });
    }

}
void CloudDownloadsToolsView::onTransferState(qint64 fileId, TransferState state)
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
void CloudDownloadsToolsView::onCloudFileError(qint64 fileId, int code)
{
    CloudFileDetailData cloudItem;
    cloudItem.fileId = fileId;
    int row = mListView->getModel()->itemRow(&cloudItem);
    if (row == -1) {
        return;
    }
    BaseFileDetailData *data = mListView->getModel()->getItemByRow(row);
    removeDownloadItem(data);
    if (mFailFileConfigMap.contains(fileId)) {
        mFailFileConfigMap.remove(fileId);
    }
}
void CloudDownloadsToolsView::addDownloadItem(const CloudFilePropInfo &fileInfo, bool saveAsRecord)
{
    QString fileIdPath = QDir::fromNativeSeparators(CloudPredefine::workSpaceDir() + QDir::separator() + QString::number(fileInfo.fileId));
    QString logoPath = QDir::fromNativeSeparators(fileIdPath + QDir::separator() + "logo.png");
    CloudFileDetailData *detailData = new CloudFileDetailData(fileInfo.fileId, 0, fileInfo.fileShowName, fileInfo.filePath,
                                                            logoPath, fileInfo.createTime, QDateTime(),
                                                            fileInfo.currentPageIndex,fileInfo.totalPage, fileInfo.fileSize,
                                                            false, false, false, fileInfo.havePwd);
    if (saveAsRecord) {
        detailData->setTransFerState(TransferSuccess);
        detailData->setFileWriteDown(true);
    } else {
        detailData->setTransFerState(WaitTransfer);
    }

    if (fileInfo.transFail) {
       detailData->setTransFerState(TransferFail);
    }
    int row = mListView->getModel()->itemRow(detailData);
    if (row != -1) {
        mListView->getModel()->removeItem(row);
    }
    mListView->getModel()->appendItem(detailData);
    showStackWidget(mListView->getModel()->rowCount() > 0 ? FileView : EmptyFileView);
}
