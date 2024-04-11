#ifndef BASECHATDELEGATE_H
#define BASECHATDELEGATE_H

#include <QStyledItemDelegate>
#include "../chatgpttypedefine.h"
#include <QPointer>
#include <QTextEdit>
#include <QKeyEvent>
#include <QApplication>
#include <QClipboard>
#include <QAction>
#include "./src/util/guiutil.h"
#include "../chatmainwidget.h"

class ToolTipWin;
class AIToolsDelegate : public QStyledItemDelegate
{
    Q_OBJECT
public:
    explicit AIToolsDelegate(QObject *parent = nullptr);
    ~AIToolsDelegate();
    void paint(QPainter * painter,const QStyleOptionViewItem & option,const QModelIndex & index) const;
    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const;
    bool editorEvent(QEvent *event, QAbstractItemModel *model, const QStyleOptionViewItem &option,
                             const QModelIndex &index);
    QWidget* createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const;
    void updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option, const QModelIndex &index) const;
    void setEditorData(QWidget *editor, const QModelIndex &index) const;
    void hideTip();
private:
    QString getTitle(AskGptType type) const;
    void drawToolBar(QPainter *painter, const QStyleOptionViewItem &option, const RspGptStruct &modelData) const;
    void drawWaiting(QPainter *painter, const QStyleOptionViewItem &option, const RspGptStruct &modelData) const;
    void drawContent(QPainter *painter, const QRect &contentRect, const QStyleOptionViewItem &option, const RspGptStruct &modelData) const;
    void showTip(const QString &tip, const QPoint &pos);
signals:
    void refreshItem(int row);
    void triggerEditor(const QPoint &pos);
    void setEditorCopyAction(bool enable) const;
    void exportTrigger(int index);
    void regenerateTrigger(int index);
    void retryTrigger(int index);
    void checkItem(int index);
    void likeItem(int index, const RspGptStruct &modelData, int oldlike);
    void updateItem(int index, const RspGptStruct &modelData);
private:
    QPointer<ToolTipWin> mTipsWidget;
    mutable int mPicIndex;
    bool mInCopyRect;
    bool mPressCopyRect;
    bool mInExportRect;
    bool mPressExportRect;
    bool mInRegenerateRect;
    bool mPressRegenerateRect;
    bool mInAgreeRect;
    bool mPressAgreeRect;
    bool mInDisagreeRect;
    bool mPressDisagreeRect;
    bool mInCheckBoxRect;
    bool mPressCheckBoxRect;
    bool mInRefreshRect;
    bool mPressRefreshRect;
};

class ChatPdfDelegate : public QStyledItemDelegate
{
    Q_OBJECT
public:
    explicit ChatPdfDelegate(QObject *parent = nullptr);
    ~ChatPdfDelegate();
    void paint(QPainter * painter,const QStyleOptionViewItem & option,const QModelIndex & index) const;
    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const;
    bool editorEvent(QEvent *event, QAbstractItemModel *model, const QStyleOptionViewItem &option,
                             const QModelIndex &index);
    QWidget* createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const;
    void updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option, const QModelIndex &index) const;
    void setEditorData(QWidget *editor, const QModelIndex &index) const;
    void hideTip();
private:
    void drawToolBar(QPainter *painter, const QStyleOptionViewItem &option, const GptRspPDFStruct &modelData) const;
    void drawWaiting(QPainter *painter, const QStyleOptionViewItem &option, const GptRspPDFStruct &modelData) const;
    void drawContent(QPainter *painter, const QRect &contentRect, const QStyleOptionViewItem &option, GptRspPDFStruct *modelData) const;
    void drawPage(QPainter *painter, const QRect &contentRect, const QStyleOptionViewItem &option, GptRspPDFStruct *modelData) const;
    void calPagePos(const QStyleOptionViewItem &option, GptRspPDFStruct *modelData) const;
    void showTip(const QString &tip, const QPoint &pos);
    int calPageItemHeight(int contentWidth, const GptRspPDFStruct &modelData) const;

signals:
    void clickPageItem(const RefMetaData &metaData);
    void refreshItem(int row);
    void triggerEditor(const QPoint &pos);
    void setEditorCopyAction(bool enable) const;
    void exportTrigger(int index);
    void regenerateTrigger(int index);
    void retryTrigger(int index);
    void ocrTrigger();
    void checkItem(int index);
    void likeItem(int index, const GptRspPDFStruct &modelData, int oldlike);
    void updateItem(int index, const GptRspPDFStruct &modelData);
private:
    QPointer<ToolTipWin> mTipsWidget;
    mutable int mPicIndex;
    bool mInCopyRect;
    bool mPressCopyRect;
    bool mInExportRect;
    bool mPressExportRect;
    bool mInRegenerateRect;
    bool mPressRegenerateRect;
    bool mInAgreeRect;
    bool mPressAgreeRect;
    bool mInDisagreeRect;
    bool mPressDisagreeRect;
    bool mInCheckBoxRect;
    bool mPressCheckBoxRect;
    bool mInRefreshRect;
    bool mPressRefreshRect;
};
#endif // BASECHATDELEGATE_H
