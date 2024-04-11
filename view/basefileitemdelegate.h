#ifndef BASEFILEITEMDELEGATE_H
#define BASEFILEITEMDELEGATE_H

#include <QStyledItemDelegate>
#include <QTimer>
#include "basefiledefine.h"

class BaseFileView;
class BaseFileItemDelegate : public QStyledItemDelegate
{
    Q_OBJECT
public:
    enum MouseState{
        Normal  = 0,
        Hover,
        Pressed
    };
    explicit BaseFileItemDelegate(BaseFileView *listView, QObject *parent = nullptr);
    virtual ~BaseFileItemDelegate();
    void initData();
    void setDrawHoverStyle(bool draw);
    void setCheckFileExist(bool check);
    void paint(QPainter * painter,const QStyleOptionViewItem & option,const QModelIndex & index) const;
    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const;
    bool editorEvent(QEvent *event, QAbstractItemModel *model, const QStyleOptionViewItem &option, const QModelIndex &index);

    void drawIcon(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const;
    virtual void drawIconExpand(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const;
    void drawList(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const;
    virtual void drawListExpand(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const;
    void drawHoverStyle(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const;
    QRect getFinalRect(const QSize&,bool list) const;
protected:
    void updateRowByFileId(qint64 fileId);
signals:
    void updateRow(int row);
    void popMenu(BaseFileDetailData *data);
    void itemClick(BaseFileDetailData *data);
public slots:
    void onShineTimeout();
protected:
    BaseFileView* mListView;
    QFont mNormalFont;
    QFont mLittleFont;
    QPixmap mMenuPixmap;
    QPixmap mPagePixmap;
    QPixmap mLocalFilePixmap;
    QPixmap mCloudSyncFilePixmap;
    QPixmap mCloudNosyncFilePixmap;
    QPixmap mMenuHoverPixmap;
    QPixmap mMenuPressedPixmap;
    QPixmap mStarredPixmap;
    QPixmap mPinPixmap;
    QPixmap mDefaultIconPixmap;

    QPixmap mTransSuccessPixmap;
    QPixmap mTransFailPixmap;
    QPixmap mlockImg;

    QImage mListBgImg;
    QImage mIconBgImg;

    mutable int mHoverRow;
    bool mDrawHoverStyle;
    bool mCheckFileExist;
    QMap<int,MouseState> mMenuBtnState = QMap<int,MouseState>{};
    mutable QMap<qint64, int> mItemShineStateMap; // 用户呼吸闪烁
    mutable QMap<qint64, QTimer*> mItemShineTimerMap; // 用户呼吸闪烁
};

#endif // BASEFILEITEMDELEGATE_H
