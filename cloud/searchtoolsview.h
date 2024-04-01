#ifndef SEARCHTOOLSVIEW_H
#define SEARCHTOOLSVIEW_H

#include "floatingwidget.h"
#include <QLabel>
#include <QPushButton>
#include "./src/basicwidget/lineedit.h"
#include <QListView>
#include <QTimer>
#include <QStackedWidget>
#include <QAbstractListModel>
#include <QStyledItemDelegate>
#include "cloudpredefine.h"

class MainWindow;
class SearchFileModel;
class SearchFileItemDelegate;
class SearchToolsView : public FloatingWidget
{
    Q_OBJECT
public:
    enum ViewType {
        EmptyView = 0,
        ListView,
    };
    explicit SearchToolsView(MainWindow*mainWindow, QWidget *parent = nullptr);
    ~SearchToolsView();
    void initView();
    void showContentWidget(bool show);
    QWidget* createEmptyWidget();
    QWidget* createListWidget();
    virtual void unActivateAction() override;
protected:
    void mousePressEvent(QMouseEvent *event);
    void mouseMoveEvent(QMouseEvent *event);
    void mouseReleaseEvent(QMouseEvent *event);
    void showEvent(QShowEvent *event);
signals:
    void locatePos(const CloudFilePropInfo &fileInfo);
    void itemClick(const CloudFilePropInfo &fileInfo);

private slots:
    void onConfirmTimeout();
    void onTextChanged(const QString &text);
    void onGetMatchFiles(const CloudFilePropInfos &fileInfos);
private:
    MainWindow* mMainWindow;
    bool   mDrag;
    QPoint mDragPosition;//点击拖拽的位置
    QLabel* mIconLabel;
    QLineEdit* mLineEdit;
    QPushButton* mClearBtn;
    QTimer mConfirmTimer;
//    QStackedWidget* mStackedWidget;
    QWidget* mContentWidget;
    QListView* mListView;
    SearchFileModel* mListModel;
    SearchFileItemDelegate* mItemDelegate;
    int mCount;
    int mNum;

    QLabel* mResultNumLabel;
    QWidget* mListWidget;
    QWidget* mEmptyWidget;
    QWidget* mSpaceWidget;
};


class SearchFileModel : public QAbstractListModel
{
    Q_OBJECT
public:
    explicit SearchFileModel(QObject *parent = nullptr);
    QVariant data(const QModelIndex &index, int role) const;
    int rowCount(const QModelIndex &parent = QModelIndex()) const;
    int columnCount(const QModelIndex &parent = QModelIndex()) const;

    void appendItem(const CloudFilePropInfo &item);
    void appendItems(const QList<CloudFilePropInfo> &items);
    void insertItem(int index, const CloudFilePropInfo &item);
    int itemRow(const CloudFilePropInfo &item);
    void removeItem(const CloudFilePropInfo &item);
    void removeItem(int row);
    void clearItems();

    CloudFilePropInfo getItemByRow(int row);
    QModelIndex currentIndexByRow(int row);
public slots:
    void onItemUpdate(int row);
protected:
    QList<CloudFilePropInfo> mItems;
};

class SearchFileItemDelegate : public QStyledItemDelegate
{
    Q_OBJECT
public:
    enum MouseState{
        Normal  = 0,
        Hover,
        Pressed
    };
    explicit SearchFileItemDelegate(QObject *parent = nullptr);
    void setSearchText(const QString &text);
    void setDrawHoverStyle(bool draw);
    void paint(QPainter * painter,const QStyleOptionViewItem & option,const QModelIndex & index) const;
    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const;
    bool editorEvent(QEvent *event, QAbstractItemModel *model, const QStyleOptionViewItem &option, const QModelIndex &index);

signals:
    void updateRow(int row);
    void locatePos(const CloudFilePropInfo &fileInfo);
    void itemClick(const CloudFilePropInfo &fileInfo);
protected:
    QString mSearchText;
    QPixmap mLocalFilePixmap;
    QPixmap mCloudSyncFilePixmap;
    QPixmap mLocateFilePixmap;
    int mPressRow;

    QMap<int,MouseState> mMenuBtnState = QMap<int,MouseState>{};

};
#endif // SEARCHTOOLSVIEW_H
