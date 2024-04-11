#ifndef AITOOLSWIDGET_H
#define AITOOLSWIDGET_H

#include <QWidget>
#include <QComboBox>
#include <QStackedWidget>
#include <QGraphicsDropShadowEffect>
#include "modelview/aitoolslistview.h"
#include "chatinputwidget.h"
#include "chatgpttypedefine.h"
#include <QPair>
#include <QProxyStyle>
#include <QStylePainter>
#include <QSharedPointer>
#include <QWeakPointer>
#include <QTimer>
#include <QApplication>

class UComboBox;
class AIChatToolsWidget;
class AIToolsWidget : public QWidget
{
    Q_OBJECT
public:
    explicit AIToolsWidget(QWidget *parent = nullptr);
    void initView();
    void setCurrentToolState(int state);
    int currentToolState();
    void addChatItem(const RspGptStruct &itemData);
    void insertChatItem(int row, const RspGptStruct &itemData);
    void updateChatItem(int row, const RspGptStruct &itemData);
    void removeChatItem(int row);
    void updateLastItem(const RspGptStruct &itemData);
    void updateAnimationItem();
    void removeAnimationItem();
    void removeFetchItem();
    int indexAnimationItem();
    int modelSize();
    void clearItems();
    RspGptStruct getChatItem(int row);

    void scrollToViewBottom();
    void addWaitingAnimation();
    void removeWaitingAnimation();

    void setInputFocus();
    CommitRspCallback getCommitRspFun(int state);
    void setExportNum(int num);
signals:
    void verticalBarValueChanged(int value);
    void fetchMoreData(qint64 recordId);
    void commitText(int state, const QString &text, const QString &lang, CommitRspCallback fun, int retry, qint64 singleChatId);
    void getInstruction(const QString &chatType);
    void stopReqTask();
    void setEditorCopyAction(bool enable);
    void exportTrigger(int index);
    void regenerateTrigger(int index);
    void retryTrigger(int index);
    void checkItem(int index);
    void likeItem(int index, const RspGptStruct &modelData, int oldlike);
    void cancelExport();
    void confirmExport(int type);
private slots:
    void onAnimationTimeout();

protected:
    virtual void showEvent(QShowEvent *event) override;
private:
    AIToolsListView* mListView;
    AIChatToolsWidget* mToolsWidget;
    QTimer mAnimationTimer;

};
class ComboBox : public QComboBox {
public:
    explicit ComboBox(QWidget *parent = nullptr) {setContextMenuPolicy(Qt::NoContextMenu);}
    void showPopup()
    {
        QApplication::setEffectEnabled(Qt::UI_AnimateCombo,false);
        QComboBox::showPopup();
        QWidget *popup = this->findChild<QFrame*>();
        QPoint pos = this->mapToGlobal(QPoint(0,-6-popup->height()));
        popup->move(pos);
        QApplication::setEffectEnabled(Qt::UI_AnimateCombo,true);
    }
    void paintEvent(QPaintEvent *event)
    {
        QStylePainter painter(this);
        painter.setPen(palette().color(QPalette::Text));

        QStyleOptionComboBox opt;
        initStyleOption(&opt);
        opt.currentIcon = QIcon("://updf-win-icon/ui2/2x/chatgpt/Ai assistant.png");
        opt.iconSize = QSize(24, 24);
        painter.drawComplexControl(QStyle::CC_ComboBox, opt);
        painter.drawControl(QStyle::CE_ComboBoxLabel, opt);
    }
};

class AIChatToolsWidget : public QWidget
{
    Q_OBJECT
public:
    enum ToolsState {
        Chat = 0,
        Summarize,
        Translate,
        Explain,
        StopChat,
        ExportMsg,
    };
    Q_ENUM(ToolsState)

    explicit AIChatToolsWidget(QWidget *parent = nullptr);
    void setCurrentToolState(ToolsState state);
    ToolsState currentToolState();
    void setInputFocus();
    CommitRspCallback getCommitRspFun(int state);
    void setExportNum(int num);
private:
    void initView();
    void resizeByState(ToolsState state);
    QWidget* createChatWidget();
    QWidget* createTranslateWidget();
    QWidget* createSummarizeWidget();
    QWidget* createExplainWidget();
    QWidget* createStopChatWidget();
    QWidget* createExportMsgWidget();
    void adjustH(ChatInputWidget *chatInputWidget, int height);
    QWeakPointer<bool> getLifeWeakPtr();
protected:
    void showEvent(QShowEvent *event);
    bool eventFilter(QObject *watched, QEvent *event);
signals:
    void commitText(int state, const QString &text, const QString &lang, CommitRspCallback fun, int retry=0, qint64 singleChatId=0);
    void getInstruction(const QString &chatType);
    void stopReqTask();
    void cancelExport();
    void confirmExport(int type);
private slots:
    void onLanguageChange(int index);
    void returnToAiAssistant();
    void onStopChat();
private:
    QGraphicsDropShadowEffect *mShadow;
    QStackedWidget* mStackedWidget;
    QComboBox* mToolsStateComboBox;
    UComboBox* mLanguageComboBox;

    QSharedPointer<bool> mLifePtr;
    CommitRspCallback mCommitAssistantRspFun;
    CommitRspCallback mCommitTranslateRspFun;
    CommitRspCallback mCommitSummarizeRspFun;
    CommitRspCallback mCommitExplainRspFun;
    CommitRspCallback mAssistantFocusFun;
    CommitRspCallback mTranslateFocusFun;
    CommitRspCallback mSummarizeFocusFun;
    CommitRspCallback mExplainFocusFun;

    bool mFirstShow;
    bool mAIAssistantIntroduce;
    bool mTranslateIntroduce;
    bool mSummarizeIntroduce;
    bool mExplainIntroduce;
    QPushButton* mExportBtn;

};
#endif // AITOOLSWIDGET_H
