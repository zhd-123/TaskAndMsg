#include "recyclebintoolsview.h"
#include "./src/customwidget/chimgandtextbtn.h"
#include "./src/util/guiutil.h"
#include "./src/toolsview/basemodelview/basefiledefine.h"
#include "recyclebinlistview.h"
#include "./utils/settingutil.h"
#include <QComboBox>
#include <QButtonGroup>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QDragEnterEvent>
#include <QMimeData>
#include <QTimer>
#include <QDir>
#include <QMenu>
#include <QStackedWidget>
#include <QWidgetAction>
#include "./src/style/stylesheethelper.h"
#include "./src/basicwidget/scrollareawidget.h"
#include "./src/util/historyfilesutil.h"
#include "../cloudmanager.h"
#include "../transportcontroller.h"
#include "../utils/commonutils.h"
#include "clicklabel.h"
#include "./src/mainwindow.h"
#include "./src/pageengine/pdfcore/PDFDocument.h"
#include "./src/basicwidget/messagewidget.h"
#include "ucombobox.h"

RecycleBinToolsView::RecycleBinToolsView(MainWindow*mainWindow, QWidget *parent)
    : QWidget{parent}
    , mMainWindow(mainWindow)
    , mListView(nullptr)
{
    initView();
    setAcceptDrops(false);
    connect(mMainWindow->getCloudManager(), &CloudManager::getRecycleBinFileList, this, &RecycleBinToolsView::onGetCloudFileList);
//    connect(mMainWindow->getCloudManager(), &CloudManager::returnFromRecycleFinished, this, &RecycleBinToolsView::onReturnFromRecycleFinished);
//    connect(mMainWindow->getCloudManager(), &CloudManager::deleteFromRecycleFinished, this, &RecycleBinToolsView::onDeleteFromRecycleFinished);
    connect(mMainWindow->getTransportController(), &TransportController::downloadThumbnailDone, this, &RecycleBinToolsView::onDownloadThumbnailDone);

    installEventFilter(this);
}

RecycleBinToolsView::~RecycleBinToolsView()
{
    if(mListView != nullptr){
        mListView->deleteLater();
    }
}

void RecycleBinToolsView::initView()
{
    mStackedWidget = new QStackedWidget();
    mStackedWidget->setObjectName("stackedWidget");

    mListView = new RecycleBinListView(this);
    connect(mListView,&RecycleBinListView::itemClick,this,[=](BaseFileDetailData *data){

    });
    connect(mListView,&RecycleBinListView::popMenu,this,[=](BaseFileDetailData *data){
        onOpenMenu(data);
    });
    mStackedWidget->insertWidget(EmptyFileView, createEmptyFileWidget());

    QWidget *viewContainer = new QWidget(this);
    MAKE_SCROLLBAR_CONTAINER(mListView, 6, viewContainer)
    mStackedWidget->insertWidget(FileView, viewContainer);

    QFrame* mainFrame = new QFrame();
    mainFrame->setObjectName("mainFrame");
    QVBoxLayout* mainFrameLayout = new QVBoxLayout();
    mainFrameLayout->setMargin(0);
    mainFrameLayout->setSpacing(0);
    mainFrameLayout->addWidget(createToolBar(), 1);
    mainFrameLayout->addWidget(mStackedWidget, 100);
    mainFrameLayout->addSpacing(8);
    mainFrameLayout->addWidget(createBottomToolWidget(), 1);
    mainFrameLayout->addSpacing(12);
    mainFrame->setLayout(mainFrameLayout);

    QVBoxLayout* bigLayout = new QVBoxLayout();
    bigLayout->setContentsMargins(0,40,0,0);
    bigLayout->addWidget(mainFrame);
    setLayout(bigLayout);
}

QFrame* RecycleBinToolsView::createEmptyFileWidget()
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
    mTipsLabel = new QLabel(tr("No files in recycle bin."));
    mTipsLabel->setObjectName("tipsLabel");
    mTipsLabel->setFont(font);
    mTipsLabel->setAlignment(Qt::AlignHCenter);
    mTipsLabel1 = new QLabel(tr("Files deleted will appear here for %1 days.").arg(30));
    mTipsLabel1->setObjectName("tipsLabel1");
    mTipsLabel1->setAlignment(Qt::AlignHCenter);
    tipLayout->addWidget(mTipsLabel,0,Qt::AlignHCenter);
    tipLayout->addWidget(mTipsLabel1,0,Qt::AlignHCenter);

    vlayout->addItem(new QSpacerItem(0,0,QSizePolicy::Minimum,QSizePolicy::Expanding));
    vlayout->addWidget(mIconLabel,0,Qt::AlignCenter);
    vlayout->addWidget(tipFrame);
    vlayout->addSpacing(100);
    vlayout->addItem(new QSpacerItem(0,0,QSizePolicy::Minimum,QSizePolicy::Expanding));

    frame->setLayout(vlayout);
    return frame;
}

QFrame *RecycleBinToolsView::createBottomToolWidget()
{
    QFrame* toolFrame = new QFrame();
    toolFrame->setObjectName("toolFrame");
    toolFrame->setFixedHeight(32);
    QHBoxLayout* frameLayout = new QHBoxLayout();
    frameLayout->setMargin(0);
    frameLayout->setSpacing(0);

    QLabel *tipLabel = new QLabel(this);
    tipLabel->setObjectName("tipLabel");
    tipLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    tipLabel->setText(tr("Items deleted within the last %1 days can be restored from here.").arg(30));

    mRecycleBinBtn = new QPushButton(this);
    mRecycleBinBtn->setObjectName("clearBtn");
    mRecycleBinBtn->setFixedSize(16, 16);
    connect(mRecycleBinBtn, &QPushButton::clicked, this, &RecycleBinToolsView::onClearList);

    mRecycleLabel = new ClickLabel(this);
    connect(mRecycleLabel, &ClickLabel::clicked, this, &RecycleBinToolsView::onClearList);
    mRecycleLabel->setObjectName("recycleLabel");
    mRecycleLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    mRecycleLabel->setText(tr("Empty Bin"));

    frameLayout->addWidget(tipLabel, 0, Qt::AlignCenter);
    frameLayout->addStretch();
    frameLayout->addWidget(mRecycleBinBtn, 0, Qt::AlignCenter);
    frameLayout->addSpacing(4);
    frameLayout->addWidget(mRecycleLabel, 0, Qt::AlignCenter);
    frameLayout->setContentsMargins(20, 0, 12, 0);

    toolFrame->setLayout(frameLayout);
    return toolFrame;
}

QFrame* RecycleBinToolsView::createToolBar()
{
    QFrame* toolBar = new QFrame();
    toolBar->setObjectName("toolBar");
    toolBar->setFixedHeight(60);

    QHBoxLayout* hlayout = new QHBoxLayout();
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

    connect(mListViewBtn,&QPushButton::released,this,&RecycleBinToolsView::onViewTypeChanged);
    connect(mThumbnailBtn,&QPushButton::released,this,&RecycleBinToolsView::onViewTypeChanged);

    mRefreshBtn = new CShowTipsBtn();
    mRefreshBtn->setObjectName("refreshBtn");
    mRefreshBtn->setToolTip(tr("Refresh"));
    mRefreshBtn->setFixedSize(24, 24);
    connect(mRefreshBtn,&QPushButton::clicked,this,&RecycleBinToolsView::onRefresh);

    mTimer.setInterval(40);
    connect(&mTimer, &QTimer::timeout, this, &RecycleBinToolsView::onLoadingTimeout);

    mAnimationTimer.setInterval(1000 * 5);
    connect(&mAnimationTimer, &QTimer::timeout, this, &RecycleBinToolsView::onAnimationTimeout);
    mAnimationLabel = new QLabel(this);
    mAnimationLabel->setStyleSheet("background:#FBFBFB;");
    mAnimationLabel->hide();

    mNameLabel = new QLabel(tr("Recycle bin"));
    mNameLabel->setObjectName("nameLabel");
    mNameLabel->setSizePolicy(QSizePolicy::Minimum,QSizePolicy::Minimum);

    mSortCombobox = new UComboBox();
    mSortCombobox->setObjectName("sortCombobox");
    GuiUtil::setComboboxRoundStyle(mSortCombobox,QColor(0,0,0,150));

    mSortCombobox->addItem(tr("Newest First"),Qt::DescendingOrder);
    mSortCombobox->addItem(tr("Oldest First"),Qt::AscendingOrder);

    connect(mSortCombobox, static_cast<void(QComboBox::*)(int)>(&QComboBox::activated),
            [this](int index){
        mListView->getModel()->sortItems(SortType(index));
    });

    QPushButton *returnBtn = new QPushButton(this);
    returnBtn->setFixedSize(16, 16);
    returnBtn->setObjectName("returnBtn");
    connect(returnBtn, &QPushButton::clicked, this, &RecycleBinToolsView::returnClicked);

    hlayout->addWidget(returnBtn);
    hlayout->addSpacing(4);
    hlayout->addWidget(mNameLabel);
    hlayout->addItem(new QSpacerItem(10,10,QSizePolicy::Expanding,QSizePolicy::Expanding));
    hlayout->addWidget(mSortCombobox);
    hlayout->addSpacing(100);
    hlayout->addWidget(mListViewBtn);
    hlayout->addSpacing(16);
    hlayout->addWidget(mThumbnailBtn);
    hlayout->addSpacing(16);
    hlayout->addWidget(mRefreshBtn);

    toolBar->setLayout(hlayout);
    return toolBar;
}

void RecycleBinToolsView::onOpenMenu(BaseFileDetailData *data)
{
    QMenu menu;
    menu.setObjectName("customMenu");
    GuiUtil::setMenuRoundStyle(&menu);

    QAction* action_revert = new QAction(tr("Restore"));
    QAction* action_delete = new QAction(tr("Delete forever"));

    QActionGroup* actionGroup = new QActionGroup(&menu);
    QWidgetAction *widgetAction = new QWidgetAction(&menu);
    widgetAction->setDefaultWidget(new QLabel(&menu));
    actionGroup->addAction(widgetAction); // 添加空action
    actionGroup->addAction(action_revert);
    actionGroup->addAction(action_delete);

    connect(actionGroup,&QActionGroup::triggered,this,[&](QAction *action){
        CloudFileDetailData *cloudData = dynamic_cast<CloudFileDetailData *>(data);
        if (!cloudData) {
            return;
        }
        if(action == action_revert){
            QList<qint64> fileIds;
            QList<qint64> dirIds;
            fileIds.append(cloudData->fileId);
            mMainWindow->getCloudManager()->returnFromRecycle(fileIds, dirIds);
            DeleteOprStruct deleteOpr;
            deleteOpr.fileIds = fileIds;
            deleteOpr.dirIds = dirIds;
            onReturnFromRecycleFinished(deleteOpr);
        } else if(action == action_delete){
            MessageWidget msgTipsWin({tr("This file will be deleted forever and you won’t be able to restore it.")},
                                     {tr("Delete"),"",tr("Cancel")});
            msgTipsWin.resetIconImage(MessageWidget::IT_Clear);
            if (msgTipsWin.exec() == MessageWidget::Accepted) {
                QList<qint64> fileIds;
                QList<qint64> dirIds;
                bool clearAll = false;
                fileIds.append(cloudData->fileId);
                mMainWindow->getCloudManager()->deleteFromRecycle(fileIds, dirIds, clearAll);
                // remove earlier
                DeleteOprStruct deleteOpr;
                deleteOpr.fileIds = fileIds;
                deleteOpr.dirIds = dirIds;
                deleteOpr.clearAll = clearAll;
                onDeleteFromRecycleFinished(deleteOpr);
            }
        }
    });
    for(auto action:actionGroup->actions()){
        menu.addAction(action);
    }

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
    layout->setContentsMargins(13, 8, 13, 0);
    layout->addWidget(contentLabel);

    QHBoxLayout *innerLayout = new QHBoxLayout(contentLabel);
    innerLayout->setContentsMargins(4, 0, 0, 0);
    innerLayout->setSpacing(8);
    innerLayout->addWidget(iconLabel, 0, Qt::AlignCenter);
    innerLayout->addWidget(titleLabel, 0, Qt::AlignCenter);
    innerLayout->addStretch();
    titleLabel->setText(tr("Cloud File"));
//    widget->setMinimumWidth(menu.sizeHint().width());
    widget->setMinimumWidth(144);
    menu.setMinimumWidth(144);
    QTimer::singleShot(10, this, [&menu, widget]{
        widget->setFixedWidth(menu.width());
    });
    menu.exec(QCursor::pos());
}

void RecycleBinToolsView::onGetCloudFileList(const CloudFilePropInfos &fileInfos)
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
                                                                logoPath, fileInfo.updateTime, QDateTime(),
                                                                fileInfo.currentPageIndex,fileInfo.totalPage, fileInfo.fileSize,
                                                                fileInfo.isStarred, false, false, fileInfo.havePwd);
        detailData->setTransFerState(TransferSuccess);
        mListView->getModel()->appendItem(detailData);
    }
    mListView->getModel()->sortItems((SortType)mSortCombobox->currentIndex());
    showStackWidget(mListView->getModel()->rowCount() > 0 ? FileView : EmptyFileView);
}

void RecycleBinToolsView::onDownloadThumbnailDone(const OssDownloadParam &param)
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

void RecycleBinToolsView::onReturnFromRecycleFinished(const DeleteOprStruct &deleteOpr)
{
    for (qint64 fileId : deleteOpr.fileIds) {
        CloudFileDetailData cloudItem;
        cloudItem.fileId = fileId;
        mListView->getModel()->removeItem(&cloudItem);
    }
    showStackWidget(mListView->getModel()->rowCount() > 0 ? FileView : EmptyFileView);
}

void RecycleBinToolsView::onDeleteFromRecycleFinished(const DeleteOprStruct &deleteOpr)
{
    for (qint64 fileId : deleteOpr.fileIds) {
        // 删除本地缓存
        QString fileIdPath = QDir::fromNativeSeparators(CloudPredefine::workSpaceDir() + QDir::separator() + QString::number(fileId));
        CommonUtils::rmDirRecursive(fileIdPath);

        CloudFileDetailData cloudItem;
        cloudItem.fileId = fileId;
        mListView->getModel()->removeItem(&cloudItem);
    }
    showStackWidget(mListView->getModel()->rowCount() > 0 ? FileView : EmptyFileView);
}

void RecycleBinToolsView::onRefresh()
{
    mMainWindow->getCloudManager()->getCloudList(RecycleBin, 0);
    startLoadingAnimation();
}

void RecycleBinToolsView::onViewTypeChanged()
{
    if(sender() == mListViewBtn){
        mListView->setListViewMode(QListView::ListMode);
    }
    else if(sender() == mThumbnailBtn){
        mListView->setListViewMode(QListView::IconMode);
    }
    mListView->update();
}

void RecycleBinToolsView::onClearList()
{
    MessageWidget win(QStringList{tr("All items in the recycle bin will be deleted forever and you won’t be able to restore them.")},
                      QStringList{tr("OK"),"",tr("Cancel")});
    win.resetIconImage(MessageWidget::IT_Clear);
    if (win.exec() != CFramelessWidget::Accepted) {
        return;
    }
    QList<qint64> fileIds;
    QList<qint64> dirIds;
    for (auto item : mListView->getModel()->items()){
        CloudFileDetailData *cloudData = dynamic_cast<CloudFileDetailData *>(item);
        if (!cloudData) {
            continue;
        }
        fileIds.append(cloudData->fileId);
    }
    bool clearAll = true;
    if (fileIds.size() > 0 || dirIds.size() > 0) {
        mMainWindow->getCloudManager()->deleteFromRecycle(fileIds, dirIds, clearAll);
    }
    clearListView();
    showStackWidget(EmptyFileView);
}

void RecycleBinToolsView::showStackWidget(ViewType viewType)
{
    mStackedWidget->setCurrentIndex(viewType);
    bool empty = true;
    if (viewType == EmptyFileView) {
        empty = true;
    } else {
        empty = false;
    }
    mNameLabel->setDisabled(empty);
    mSortCombobox->setDisabled(empty);
    mListViewBtn->setDisabled(empty);
    mThumbnailBtn->setDisabled(empty);
//    mRemoveBtn->setDisabled(empty);
    mRecycleBinBtn->setDisabled(empty);;
    mRecycleLabel->setDisabled(empty);;
}

void RecycleBinToolsView::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    onRefresh();
}

bool RecycleBinToolsView::eventFilter(QObject *watched, QEvent *event)
{
    if (this == watched) {
        if (event->type() == QEvent::WindowActivate) {

        }
    }
    return QWidget::eventFilter(watched, event);
}

void RecycleBinToolsView::clearListView()
{
    mListView->getModel()->clearItems();
}

void RecycleBinToolsView::onLoadingTimeout()
{
    QString indexString = QString::number(mIndex);
    QString imagePath = QString("://updf-win-icon/ui2/2x/welcome-page/refresh/Comp 1_000%1.png").arg(indexString.rightJustified(2, '0'));
    mAnimationLabel->setPixmap(GuiUtil::generateSmoothPic(imagePath, mAnimationLabel->size()));
    if (--mIndex < 0) {
        mIndex = 24;
    }
}

void RecycleBinToolsView::onAnimationTimeout()
{
    stopAnimation();
}
void RecycleBinToolsView::startLoadingAnimation()
{
    //animation
    mAnimationLabel->setFixedSize(mRefreshBtn->size());
    QPoint pos = mapToParent(mRefreshBtn->pos());
    mAnimationLabel->setGeometry(QRect(pos + QPoint(0, 40), mAnimationLabel->size()));
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

void RecycleBinToolsView::stopAnimation()
{
    mCanStop = true;
    stopAnimationImpl();
}

void RecycleBinToolsView::stopAnimationImpl()
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

