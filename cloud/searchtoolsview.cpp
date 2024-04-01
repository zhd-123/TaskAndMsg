#include "searchtoolsview.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPainter>
#include <QPainterPath>
#include <QFontMetrics>
#include <QMouseEvent>
#include <QRegExpValidator>
#include "cloudmanager.h"
#include "./src/mainwindow.h"


SearchToolsView::SearchToolsView(MainWindow*mainWindow,QWidget *parent)
    : FloatingWidget{parent}
    , mMainWindow(mainWindow)
    , mDrag(false)
{
    setFixedSize(608 + 16, 248 + 16);
    visiblePanel()->setFixedSize(608, 248);

    initView();
}

SearchToolsView::~SearchToolsView()
{
    if (mListModel) {
        mListModel->clearItems();
    }
}

void SearchToolsView::initView()
{
    mConfirmTimer.setInterval(500);
    mConfirmTimer.setSingleShot(true);
    connect(&mConfirmTimer, &QTimer::timeout, this, &SearchToolsView::onConfirmTimeout);

    mIconLabel = new QLabel(this);
    mIconLabel->setFixedSize(24, 24);
    mIconLabel->setObjectName("searchIcon");
    mLineEdit = new LineEdit(this);
    mLineEdit->setMaxLength(250);
    mLineEdit->setPlaceholderText(tr("Search"));
//    mLineEdit->setValidator(new QRegExpValidator(QRegExp("^[/\]"), this));
    connect(mLineEdit, &QLineEdit::textChanged, this, &SearchToolsView::onTextChanged);
    mLineEdit->setFixedHeight(24);
    mLineEdit->setObjectName("lineEdit");
    mClearBtn = new QPushButton(this);
    mClearBtn->setFixedSize(16, 16);
    mClearBtn->setObjectName("clearBtn");
    mClearBtn->setVisible(false);
    connect(mClearBtn, &QPushButton::clicked, this, [=] {
        mLineEdit->setText("");
        mLineEdit->setFocus();
    });

    QHBoxLayout *topLayout = new QHBoxLayout;
    topLayout->setContentsMargins(8, 8, 12, 8);
    topLayout->setSpacing(0);
    topLayout->addWidget(mIconLabel);
    topLayout->addWidget(mLineEdit);
    topLayout->addWidget(mClearBtn, 0, Qt::AlignCenter);

    mContentWidget = new QWidget(this);
    mContentWidget->setObjectName("contentWidget");
    QFrame* lineFrame = new QFrame();
    lineFrame->setObjectName("lineFrame");

//    mStackedWidget = new QStackedWidget(this);
//    mStackedWidget->setObjectName("stackedWidget");
//    mStackedWidget->insertWidget(EmptyView, createEmptyWidget());
//    mStackedWidget->insertWidget(ListView, createListWidget());

    mListWidget = createListWidget();
    mEmptyWidget = createEmptyWidget();
    mSpaceWidget = new QWidget(this);
    QVBoxLayout *contentLayout =new QVBoxLayout(mContentWidget);
    contentLayout->addWidget(lineFrame, 0);
//    contentLayout->addWidget(mStackedWidget);
    contentLayout->addWidget(mSpaceWidget, 1);
    contentLayout->setSpacing(0);
    contentLayout->setMargin(0);

    mEmptyWidget->hide();
    mListWidget->hide();
    QVBoxLayout *layout =new QVBoxLayout;
    layout->setSpacing(0);
    layout->setMargin(0);
    layout->addLayout(topLayout);
    layout->addWidget(mContentWidget);
    visiblePanel()->setLayout(layout);

    connect(mMainWindow->getCloudManager(), &CloudManager::localMatchFiles, this, &SearchToolsView::onGetMatchFiles);
    connect(mMainWindow->getCloudManager(), &CloudManager::cloudMatchFiles, this, &SearchToolsView::onGetMatchFiles);
}

void SearchToolsView::showContentWidget(bool show)
{
    mContentWidget->setVisible(show);
    if (show) {
        setFixedSize(608 + 16, 248 + 16);
        visiblePanel()->setFixedSize(608, 248);
    } else {
        setFixedSize(608 + 16, 42 + 16);
        visiblePanel()->setFixedSize(608, 42);
    }
}

QWidget *SearchToolsView::createEmptyWidget()
{
    QWidget *widget = new QWidget(this);

    QLabel *emptyIcon = new QLabel(this);
    emptyIcon->setFixedSize(150, 104);
    emptyIcon->setObjectName("emptyIcon");

    QLabel *emptyTip = new QLabel(this);
    emptyTip->setObjectName("emptyTip");
    emptyTip->setFixedHeight(16);
    emptyTip->setAlignment(Qt::AlignCenter);
    emptyTip->setText(tr("No relevant results were found"));

    QVBoxLayout *layout = new QVBoxLayout(widget);
    layout->setContentsMargins(20, 42, 20, 42);
    layout->setSpacing(8);
    layout->addWidget(emptyIcon, 0, Qt::AlignCenter);
    layout->addWidget(emptyTip);
    return widget;
}

QWidget *SearchToolsView::createListWidget()
{
    QWidget *widget = new QWidget(this);
    mResultNumLabel = new QLabel(this);
    mResultNumLabel->setObjectName("normalLabel");
    mResultNumLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    mResultNumLabel->setFixedHeight(16);
    mResultNumLabel->setMinimumWidth(200);

    QHBoxLayout *topLayout = new QHBoxLayout;
    topLayout->addWidget(mResultNumLabel);
    topLayout->addStretch();
    topLayout->setSpacing(0);
    topLayout->setMargin(0);
    mListView = new QListView(this);
    mListView->setLayoutMode(QListView::SinglePass);
    mListView->setAutoScroll(false);
    mListView->setUniformItemSizes(true);

    //像素滚动
    mListView->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    mListView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    mListView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    mListView->verticalScrollBar()->setContextMenuPolicy(Qt::NoContextMenu);
    mListView->horizontalScrollBar()->setContextMenuPolicy(Qt::NoContextMenu);
    // 默认列表模式
    mListView->setViewMode(QListView::ListMode);
    mListView->setFlow(QListView::TopToBottom);
    mListView->setSpacing(0);
    mListView->setMouseTracking(true);

    mListModel = new SearchFileModel(this);
    mItemDelegate = new SearchFileItemDelegate(this);
    connect(mItemDelegate,&SearchFileItemDelegate::updateRow, mListModel, &SearchFileModel::onItemUpdate);
    connect(mItemDelegate,&SearchFileItemDelegate::itemClick,this, [=](const CloudFilePropInfo &fileInfo) {
        this->hide();
        emit itemClick(fileInfo);
    });
    connect(mItemDelegate,&SearchFileItemDelegate::locatePos,this, [=](const CloudFilePropInfo &fileInfo) {
        this->hide();
        emit locatePos(fileInfo);
    });
    mListView->setItemDelegate(mItemDelegate);
    mListView->setModel(mListModel);

    QVBoxLayout *layout = new QVBoxLayout(widget);
    layout->setContentsMargins(8, 12, 8, 12);
    layout->setSpacing(8);
    layout->addLayout(topLayout);
    layout->addWidget(mListView);
    return widget;
}

void SearchToolsView::unActivateAction()
{
    this->hide();
    if (mConfirmTimer.isActive()) {
        mConfirmTimer.stop();
    }
}

void SearchToolsView::showEvent(QShowEvent *event)
{
    FloatingWidget::showEvent(event);
    mLineEdit->setText("");
    mLineEdit->setFocus();
    mListModel->clearItems();
    showContentWidget(false);
}

void SearchToolsView::mousePressEvent(QMouseEvent *event)
{
    //点击左边鼠标
    if (event->button() == Qt::LeftButton)
    {
        //globalPos()获取根窗口的相对路径，frameGeometry().topLeft()获取主窗口左上角的位置
        mDrag = true;
        mDragPosition = event->globalPos() - this->pos();
        //鼠标事件被系统接收
        event->accept();
    }
    QWidget::mousePressEvent(event);
}

void SearchToolsView::mouseMoveEvent(QMouseEvent *event)
{
    if (mDrag && event->buttons() == Qt::LeftButton)
    {
        //移动窗口
        move(event->globalPos() - mDragPosition);
        event->accept();
    }
    QWidget::mouseMoveEvent(event);
}

void SearchToolsView::mouseReleaseEvent(QMouseEvent *event)
{
    QWidget::mouseReleaseEvent(event);
    mDrag = false;
}
void SearchToolsView::onConfirmTimeout()
{
    if (mConfirmTimer.isActive()) {
        mConfirmTimer.stop();
    }
    QString searchText = mLineEdit->text().trimmed();
    if (searchText.isEmpty() || searchText.size() < 1) {
        return;
    }
    // clear data
    mResultNumLabel->setText(QString(tr("result: %1")).arg(0));
    mListModel->clearItems();
    mItemDelegate->setSearchText(searchText);
    mCount = 0;
    mNum = 0;
    mMainWindow->getCloudManager()->searchFile(searchText);
}

void SearchToolsView::onTextChanged(const QString &text)
{
    if (mConfirmTimer.isActive()) {
        mConfirmTimer.stop();
    }
    if (text.trimmed().isEmpty() || text.size() < 1) {
        if (text.trimmed().isEmpty()) {
            showContentWidget(false);
        }
        mResultNumLabel->setText(QString(tr("result: %1")).arg(0));
        mListModel->clearItems();
        mClearBtn->setVisible(false);
        return;
    }
    mClearBtn->setVisible(true);
    mConfirmTimer.start();
}

void SearchToolsView::onGetMatchFiles(const CloudFilePropInfos &fileInfos)
{
    mNum += fileInfos.size();
    mResultNumLabel->setText(tr("result: %1").arg(mNum));
    mListModel->appendItems(fileInfos);

    showContentWidget(true);
    if (mNum > 0) {
        mListWidget->setGeometry(mSpaceWidget->geometry());
        mListWidget->move(mSpaceWidget->pos()+QPoint(8, 42));
        mListWidget->show();
        mEmptyWidget->hide();
//        mStackedWidget->setCurrentIndex(ListView);
    } else {
        if (++mCount >= 2) {
            mEmptyWidget->setGeometry(mSpaceWidget->geometry());
            mEmptyWidget->move(mSpaceWidget->pos()+QPoint(8, 42));
            mListWidget->hide();
            mEmptyWidget->show();
//            mStackedWidget->setCurrentIndex(EmptyView);
        }
    }
}

/////////////////////

SearchFileModel::SearchFileModel(QObject *parent)
    : QAbstractListModel{parent}
{
}

QVariant SearchFileModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid()) {
        return QVariant();
    }
    if (mItems.size() <= index.row()) {
        return QVariant();
    }
    QVariant variant;
    if (Qt::UserRole == role) {
         variant.setValue(mItems.at(index.row()));
    }
    return variant;
}

int SearchFileModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {

    }
    return mItems.size();
}

int SearchFileModel::columnCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {

    }
    return 1;
}

void SearchFileModel::appendItem(const CloudFilePropInfo &item)
{
    beginInsertRows(QModelIndex(), mItems.count(), mItems.count());
    mItems.append(item);
    endInsertRows();
}

void SearchFileModel::insertItem(int index, const CloudFilePropInfo &item)
{
    beginInsertRows(QModelIndex(), index, index);
    mItems.insert(index, item);
    endInsertRows();
}

int SearchFileModel::itemRow(const CloudFilePropInfo &item)
{
    int row = -1;
    for (int i = 0; i < rowCount(); ++i) {
        if (item.fileId <= 0) {
            if (getItemByRow(i).filePath == item.filePath) {
                row = i;
                break;
            }
        } else {
            if (getItemByRow(i).fileId == item.fileId) {
                row = i;
                break;
            }
        }
    }
    return row;
}

void SearchFileModel::removeItem(const CloudFilePropInfo &item)
{
    int row = itemRow(item);
    removeItem(row);
}

void SearchFileModel::appendItems(const QList<CloudFilePropInfo> &items)
{
    emit layoutAboutToBeChanged();
    for (CloudFilePropInfo item : items) {
        mItems.append(item);
    }
    emit layoutChanged();
}

void SearchFileModel::removeItem(int row)
{
    if (row < 0 || mItems.size() <= row) {
        return;
    }
    beginRemoveRows(QModelIndex(), row, row);
    mItems.removeAt(row);
    endRemoveRows();
}

void SearchFileModel::clearItems()
{
    beginResetModel();
    mItems.clear();
    endResetModel();
}

void SearchFileModel::onItemUpdate(int row)
{
    if (row == -1) {
        return;
    }
    emit dataChanged(createIndex(row, 0), createIndex(row, columnCount()));
}

QModelIndex SearchFileModel::currentIndexByRow(int row)
{
    return createIndex(row, 0);
}

CloudFilePropInfo SearchFileModel::getItemByRow(int row)
{
    if (mItems.size() <= row || row < 0) {
        return CloudFilePropInfo();
    }
    return mItems.at(row);
}

SearchFileItemDelegate::SearchFileItemDelegate(QObject *parent)
    : QStyledItemDelegate{parent}
{
    mPressRow = -1;
    mLocalFilePixmap.load(":/updf-win-icon/ui2/2x/welcome-page/local_file.png");
    mCloudSyncFilePixmap.load(":/updf-win-icon/ui2/2x/welcome-page/cloud_file1.png");
    mLocateFilePixmap.load("://updf-win-icon/ui2/2x/welcome-page/locate.png");
}

void SearchFileItemDelegate::setSearchText(const QString &text)
{
    mSearchText = text;
}

void SearchFileItemDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    QStyledItemDelegate::paint(painter,option,index);

    painter->setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing | QPainter::SmoothPixmapTransform);
    if (index.isValid()) {
        if (option.state.testFlag(QStyle::State_MouseOver)) {
            QPainterPath path;
            path.addRoundedRect(option.rect, 4, 4);
            painter->fillPath(path, QColor("#F7F7F7"));
        } else if (mPressRow == index.row()) {
            QPainterPath path;
            path.addRoundedRect(option.rect, 4, 4);
            painter->fillPath(path, QColor("#E9DAFF"));
        }

        QVariant userData = index.data(Qt::UserRole);
        CloudFilePropInfo fileInfo = userData.value<CloudFilePropInfo>();
        QRect paintRect = option.rect.adjusted(8,0,8,0);
        const int maxTextWidth = 380;
        QFont font;
        font.setPixelSize(11);
        painter->setFont(font);
        QFontMetrics fontMetrics = painter->fontMetrics();
        QString showStr = fontMetrics.elidedText(fileInfo.fileShowName + PDF_FILE_SUFFIX, Qt::ElideMiddle, maxTextWidth);
        int indexPos = showStr.indexOf(mSearchText, 0, Qt::CaseInsensitive);
        QString leftStr,midStr,rightStr;
        if (indexPos == -1) {
            leftStr = "";
            midStr = "";
            rightStr = showStr;
        } else {
            leftStr = showStr.left(indexPos);
            midStr = showStr.mid(indexPos, mSearchText.length());
            rightStr = showStr.mid(indexPos + mSearchText.length());
        }

        QPen pen;
        pen.setColor("#8A8A8A");
        QPen highlightPen;
        highlightPen.setColor("#964FF2");

        painter->setPen(pen);
        painter->drawText(paintRect, Qt::AlignLeft | Qt::AlignVCenter, leftStr);
        painter->setPen(highlightPen);
        painter->drawText(paintRect.adjusted(fontMetrics.width(leftStr), 0,0,0), Qt::AlignLeft | Qt::AlignVCenter, midStr);
        painter->setPen(pen);
        painter->drawText(paintRect.adjusted(fontMetrics.width(leftStr + midStr), 0,0,0), Qt::AlignLeft | Qt::AlignVCenter, rightStr);

        // icon
        QRect iconRect = QRect(paintRect.width() + paintRect.x() - 160, paintRect.y() + 8,16,16);
        if (fileInfo.fileId <= 0) {
//            painter->drawPixmap(iconRect, mLocalFilePixmap);
        } else {
            painter->drawPixmap(iconRect, mCloudSyncFilePixmap);
        }

        if (option.state.testFlag(QStyle::State_MouseOver)) {
            QRect locateIconRect = QRect(paintRect.width() + paintRect.x() - 86, paintRect.y() + 8,16,16);
            painter->drawPixmap(locateIconRect, mMenuBtnState[index.row()] == MouseState::Normal ? mLocateFilePixmap :
                                                mMenuBtnState[index.row()] == MouseState::Hover ?  mLocateFilePixmap : mLocateFilePixmap);
            painter->drawText(paintRect.adjusted(locateIconRect.width() + locateIconRect.x() - 8,0,0,0), Qt::AlignLeft | Qt::AlignVCenter, tr("Locate"));
        }
    }
}

bool SearchFileItemDelegate::editorEvent(QEvent *event, QAbstractItemModel *model, const QStyleOptionViewItem &option, const QModelIndex &index)
{
    QMouseEvent *pEvent = static_cast<QMouseEvent *>(event);
    QVariant userData = index.data(Qt::UserRole);
    CloudFilePropInfo fileInfo = userData.value<CloudFilePropInfo>();
    QRect paintRect = option.rect.adjusted(8,0,8,0);
    QRect locateIconRect = QRect(paintRect.width() + paintRect.x() - 72, paintRect.y() + 8,50,16);
    int row = index.row();
    mMenuBtnState[row] = MouseState::Normal;
    if (locateIconRect.contains(pEvent->pos())) {
        if(event->type() == QEvent::MouseButtonRelease){
            if(pEvent->button() == Qt::LeftButton){
                emit locatePos(fileInfo);
            }
        } else if (event->type() == QEvent::MouseButtonPress) {
            mMenuBtnState[row] = MouseState::Pressed;
        } else if (event->type() == QEvent::MouseMove) {
            mMenuBtnState[row] = MouseState::Hover;
        }
    } else {
        if(event->type() == QEvent::MouseButtonRelease){
            if(pEvent->button() == Qt::LeftButton){
                emit itemClick(fileInfo);
                mPressRow = -1;
            }
        } else if (event->type() == QEvent::MouseButtonPress){
            if(pEvent->button() == Qt::LeftButton){
                mPressRow = row;
            }
        }
    }
    emit updateRow(row);
    return QStyledItemDelegate::editorEvent(event, model, option, index);
}

QSize SearchFileItemDelegate::sizeHint(const QStyleOptionViewItem &, const QModelIndex &) const
{
    return QSize(592, 32);
}
