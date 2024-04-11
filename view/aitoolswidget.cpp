#include "aitoolswidget.h"
#include <QAction>
#include "umenu.h"
#include <QLabel>
#include <QLineEdit>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QApplication>
#include <QClipboard>
#include "lineedit.h"
#include "./src/util/guiutil.h"
#include "sidebasicbutton.h"
#include "./src/basicwidget/scrollareawidget.h"
#include "./src/util/gamanager.h"
#include "ucombobox.h"

AIToolsWidget::AIToolsWidget(QWidget *parent)
    : QWidget{parent}
{
    initView();
}

void AIToolsWidget::initView()
{
    mListView = new AIToolsListView(this);
    connect(mListView, &AIToolsListView::fetchMoreData, this, &AIToolsWidget::fetchMoreData);
    connect(mListView, &AIToolsListView::verticalBarValueChanged, this, &AIToolsWidget::verticalBarValueChanged);
    connect(mListView, &AIToolsListView::setEditorCopyAction, this, &AIToolsWidget::setEditorCopyAction);
    connect(mListView, &AIToolsListView::exportTrigger, this, &AIToolsWidget::exportTrigger);
    connect(mListView, &AIToolsListView::regenerateTrigger, this, &AIToolsWidget::regenerateTrigger);
    connect(mListView, &AIToolsListView::retryTrigger, this, &AIToolsWidget::retryTrigger);
    connect(mListView, &AIToolsListView::likeItem, this, &AIToolsWidget::likeItem);
    connect(mListView, &AIToolsListView::checkItem, this, &AIToolsWidget::checkItem);
    QWidget *viewContainer = new QWidget(this);
    MAKE_SCROLLBAR_CONTAINER(mListView, 2, viewContainer)

    mToolsWidget = new AIChatToolsWidget(this);
    connect(mToolsWidget, &AIChatToolsWidget::commitText, this, &AIToolsWidget::commitText);
    connect(mToolsWidget, &AIChatToolsWidget::cancelExport, this, &AIToolsWidget::cancelExport);
    connect(mToolsWidget, &AIChatToolsWidget::confirmExport, this, &AIToolsWidget::confirmExport);
    connect(mToolsWidget, &AIChatToolsWidget::getInstruction, this, &AIToolsWidget::getInstruction);
    connect(mToolsWidget, &AIChatToolsWidget::stopReqTask, this, [&] {
        emit stopReqTask();
        removeWaitingAnimation();
    });
    QHBoxLayout *toolLayout = new QHBoxLayout;
    toolLayout->addWidget(mToolsWidget);
    toolLayout->addSpacing(8);
    toolLayout->setMargin(0);
    toolLayout->setSpacing(0);

    QVBoxLayout *layout = new QVBoxLayout;
    layout->addSpacing(8);
    layout->addWidget(viewContainer, 100);
    layout->addSpacing(8);
    layout->addLayout(toolLayout, 1);
    layout->addSpacing(8);
    layout->setMargin(0);
    layout->setSpacing(0);
    setLayout(layout);

    mAnimationTimer.setInterval(60);
    connect(&mAnimationTimer, &QTimer::timeout, this, &AIToolsWidget::onAnimationTimeout);
}

void AIToolsWidget::setCurrentToolState(int state)
{
    mToolsWidget->setCurrentToolState((AIChatToolsWidget::ToolsState)state);
}

int AIToolsWidget::currentToolState()
{
    return mToolsWidget->currentToolState();
}

void AIToolsWidget::addChatItem(const RspGptStruct &itemData)
{
    mListView->addItemCell(itemData);
}

void AIToolsWidget::insertChatItem(int row, const RspGptStruct &itemData)
{
    mListView->insertItemCell(row, itemData);
}

void AIToolsWidget::updateChatItem(int row, const RspGptStruct &itemData)
{
    mListView->updateItemCell(row, itemData);
}

void AIToolsWidget::removeChatItem(int row)
{
    mListView->removeItemCell(row);
}
void AIToolsWidget::updateLastItem(const RspGptStruct &itemData)
{
    mListView->updateLastItem(itemData);
}
void AIToolsWidget::updateAnimationItem()
{
    mListView->updateAnimationItem();
}

void AIToolsWidget::removeAnimationItem()
{
    mListView->removeAnimationItem();
}

void AIToolsWidget::removeFetchItem()
{
    mListView->removeFetchItem();
}

int AIToolsWidget::indexAnimationItem()
{
    return mListView->indexAnimationItem();
}

int AIToolsWidget::modelSize()
{
    return mListView->modelSize();
}

void AIToolsWidget::clearItems()
{
    mListView->clearItems();
}

RspGptStruct AIToolsWidget::getChatItem(int row)
{
    return mListView->getItemCell(row);
}
void AIToolsWidget::scrollToViewBottom()
{
    mListView->scrollToBottom();
}
void AIToolsWidget::addWaitingAnimation()
{
    QString text = "";
    RspGptStruct rspGptStruct;
    rspGptStruct.content = text;
    rspGptStruct.messageType = AI_TYPE_NAME;
    rspGptStruct.waitingAnimation = true;
    addChatItem(rspGptStruct);
    scrollToViewBottom();

    mAnimationTimer.start();
}

void AIToolsWidget::removeWaitingAnimation()
{
    mAnimationTimer.stop();
    removeAnimationItem();
}

void AIToolsWidget::setInputFocus()
{
    mToolsWidget->setInputFocus();
}

CommitRspCallback AIToolsWidget::getCommitRspFun(int state)
{
    return mToolsWidget->getCommitRspFun(state);
}

void AIToolsWidget::setExportNum(int num)
{
    mToolsWidget->setExportNum(num);
}

void AIToolsWidget::onAnimationTimeout()
{
    updateAnimationItem();
}

void AIToolsWidget::showEvent(QShowEvent *event)
{
    __super::showEvent(event);
    GaMgr->takeGaTask(GaManager::ai_panel_show);
}

AIChatToolsWidget::AIChatToolsWidget(QWidget *parent)
    : QWidget{parent}
    , mCommitAssistantRspFun(nullptr)
    , mCommitTranslateRspFun(nullptr)
    , mCommitSummarizeRspFun(nullptr)
    , mCommitExplainRspFun(nullptr)
    , mAssistantFocusFun(nullptr)
    , mTranslateFocusFun(nullptr)
    , mSummarizeFocusFun(nullptr)
    , mExplainFocusFun(nullptr)
    , mFirstShow(false)
    , mAIAssistantIntroduce(false)
    , mTranslateIntroduce(false)
    , mSummarizeIntroduce(false)
    , mExplainIntroduce(false)
{
    mLifePtr = QSharedPointer<bool>(new bool);
    initView();

    mShadow = new QGraphicsDropShadowEffect(this);
    mShadow->setOffset(0, 0);
    mShadow->setColor(QColor(0,0,0,25));
    mShadow->setBlurRadius(16);
    setGraphicsEffect(mShadow);
    mToolsStateComboBox->installEventFilter(this);
}

void AIChatToolsWidget::initView()
{
    mStackedWidget = new QStackedWidget(this);
    mStackedWidget->insertWidget(Chat, createChatWidget());
    mStackedWidget->insertWidget(Summarize, createSummarizeWidget());
    mStackedWidget->insertWidget(Translate, createTranslateWidget());
    mStackedWidget->insertWidget(Explain, createExplainWidget());
    mStackedWidget->insertWidget(StopChat, createStopChatWidget());
    mStackedWidget->insertWidget(ExportMsg, createExportMsgWidget());
    mStackedWidget->setObjectName("stackedWidget");
    QVBoxLayout *layout = new QVBoxLayout;
    layout->addWidget(mStackedWidget);
    layout->setContentsMargins(2, 0, 2, 0);
    layout->setSpacing(0);
    setLayout(layout);
}

QWidget *AIChatToolsWidget::createChatWidget()
{
    QWidget *widget = new QWidget(this);
    widget->setObjectName("toolsWidget");
    mToolsStateComboBox = new ComboBox(this);
    mToolsStateComboBox->addItem(QIcon("://updf-win-icon/ui2/2x/chatgpt/Summarize.png"), tr("Summarize"));
    mToolsStateComboBox->addItem(QIcon("://updf-win-icon/ui2/2x/chatgpt/Translate.png"), tr("Translate"));
    mToolsStateComboBox->addItem(QIcon("://updf-win-icon/ui2/2x/chatgpt/Explain.png"), tr("Explain"));
    GuiUtil::setComboboxRoundStyle(mToolsStateComboBox);
    mToolsStateComboBox->setObjectName("comboBox");
    mToolsStateComboBox->setFixedHeight(40);
    LineEdit *lineEdit = new LineEdit(this);
    lineEdit->setStyleSheet("border:hidden");
    connect(lineEdit, &LineEdit::mousePressed, this, [=] {
        QTimer::singleShot(150,this, [this](){
            if(!mToolsStateComboBox->view()->isVisible()){
                mToolsStateComboBox->showPopup();
            }
        });
    });
    lineEdit->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    lineEdit->setReadOnly(true);

    mToolsStateComboBox->setLineEdit(lineEdit);
    mToolsStateComboBox->setContextMenuPolicy(Qt::NoContextMenu);
    QString tipText = tr("Prompt");
    mToolsStateComboBox->lineEdit()->setText(tipText);
    mToolsStateComboBox->setIconSize(QSize(24, 24));
    connect(mToolsStateComboBox, QOverload<int>::of(&QComboBox::activated), this, [&, tipText](int index) {
        mToolsStateComboBox->lineEdit()->setText(tipText);

        setCurrentToolState((ToolsState)(index + Summarize));
    });

    ChatInputWidget* chatInputWidget = new ChatInputWidget(this);
    chatInputWidget->setPlaceholderText(tr("Ask something"));
    chatInputWidget->setFixedHeight(40);
    chatInputWidget->setFocusoutCommit(false);
    connect(chatInputWidget, &ChatInputWidget::adjustHeight, this, [&, chatInputWidget] (int height) {
        adjustH(chatInputWidget, height);
    });
    QWeakPointer<bool> alive = getLifeWeakPtr();
    auto rspFun = [this, alive, chatInputWidget] {
        if (!alive.lock()) {
            return;
        }
        int height = 40;
        chatInputWidget->setPlainText("");
        adjustH(chatInputWidget, height);
        setCurrentToolState(ToolsState::Chat);
    };
    mCommitAssistantRspFun = rspFun;
    mAssistantFocusFun = [chatInputWidget] {
        chatInputWidget->setPlainText("");
        chatInputWidget->setFocus();
    };
    connect(chatInputWidget, &ChatInputWidget::commitText, this, [&, chatInputWidget, rspFun] (const QString &text) {
        emit commitText(GptRandomChat, text, "", rspFun);
    });

    QHBoxLayout *comboLayout = new QHBoxLayout;
    comboLayout->setContentsMargins(0, 0, 0, 0);
    comboLayout->addWidget(mToolsStateComboBox);
    QVBoxLayout *layout = new QVBoxLayout;
    layout->addLayout(comboLayout);
    layout->addStretch();
    layout->addWidget(chatInputWidget);
    layout->setMargin(1);
    layout->setSpacing(0);
    widget->setLayout(layout);
    return widget;
}

QWidget *AIChatToolsWidget::createTranslateWidget()
{
     QWidget *widget = new QWidget(this);
     widget->setObjectName("toolsWidget");

     QLabel *iconLabel = new QLabel(this);
     iconLabel->setFixedSize(24, 24);
     iconLabel->setObjectName("iconTranslate");

     QLabel *titleLabel = new QLabel(this);
     titleLabel->setFixedHeight(24);
     titleLabel->setText(tr("Translate"));
     titleLabel->setObjectName("titleLabel");
//     QPushButton *pinBtn = new QPushButton(this);
//     pinBtn->setFixedSize(16, 16);
//     pinBtn->setObjectName("pinBtn");
//     pinBtn->setCheckable(true);

     QPushButton *closeBtn = new QPushButton(this);
     closeBtn->setFixedSize(24, 24);
     closeBtn->setObjectName("closeBtn");
     connect(closeBtn, &QPushButton::clicked, this, &AIChatToolsWidget::returnToAiAssistant);
     QHBoxLayout *topLayout = new QHBoxLayout;
     topLayout->addSpacing(8);
     topLayout->addWidget(iconLabel);
     topLayout->addSpacing(8);
     topLayout->addWidget(titleLabel);
     topLayout->addStretch();
//     topLayout->addWidget(pinBtn, 0, Qt::AlignCenter);
     topLayout->addSpacing(8);
     topLayout->addWidget(closeBtn);
     topLayout->addSpacing(8);

     QFrame *line = new QFrame(this);
     line->setObjectName("lineFrame");

     mLanguageComboBox = new UComboBox();
     auto langList = ChatgptTypeDefine::getLangList(false);
     for (auto lan : langList) {
         mLanguageComboBox->addItem(lan.first, lan.second);
     }
//     mLanguageComboBox->view()->setMinimumHeight(20 + langList.size() * 32);
     mLanguageComboBox->setObjectName("comboBox1");
     connect(mLanguageComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &AIChatToolsWidget::onLanguageChange);
     int index = mLanguageComboBox->findData(loadLangSetting());
     mLanguageComboBox->setCurrentIndex(index == -1?0:index);

     QHBoxLayout *midLayout = new QHBoxLayout;
     midLayout->addSpacing(0);
     midLayout->addWidget(mLanguageComboBox);
     midLayout->addSpacing(0);
     mLanguageComboBox->setFixedHeight(32);

     ChatInputWidget* chatInputWidget = new ChatInputWidget(this);
     chatInputWidget->setPlaceholderText(tr("Ask something"));
     chatInputWidget->setFixedHeight(40);
     chatInputWidget->setFocusoutCommit(false);
     connect(chatInputWidget, &ChatInputWidget::adjustHeight, this, [&, chatInputWidget] (int height) {
         adjustH(chatInputWidget, height);
     });

     QWeakPointer<bool> alive = getLifeWeakPtr();
     auto rspFun = [this, alive, chatInputWidget/*, pinBtn*/] {
         if (!alive.lock()) {
             return;
         }
         int height = 40;
         chatInputWidget->setPlainText("");
         adjustH(chatInputWidget, height);
         if (0/*!pinBtn->isChecked()*/) {
             returnToAiAssistant();
         } else {
             setCurrentToolState(ToolsState::Translate);
         }
     };
     mCommitTranslateRspFun = rspFun;
     connect(chatInputWidget, &ChatInputWidget::commitText, this, [&, chatInputWidget, rspFun/*, pinBtn*/] (const QString &text) {
         QString lang = mLanguageComboBox->currentData().toString();
         saveLangSetting(lang);
         emit commitText(GptTranslate, text, lang, rspFun);
     });
     mTranslateFocusFun = [chatInputWidget] {
         chatInputWidget->setPlainText("");
         chatInputWidget->setFocus();
     };
     QVBoxLayout *layout = new QVBoxLayout;
     layout->addSpacing(8);
     layout->addLayout(topLayout);
     layout->addSpacing(8);
     layout->addWidget(line);
     layout->addSpacing(12);
     layout->addLayout(midLayout);
     layout->addStretch();
     layout->addWidget(chatInputWidget);
     layout->setMargin(1);
     layout->setSpacing(0);
     widget->setLayout(layout);
     return widget;
}

QWidget *AIChatToolsWidget::createSummarizeWidget()
{
    QWidget *widget = new QWidget(this);
    widget->setObjectName("toolsWidget");

    QLabel *iconLabel = new QLabel(this);
    iconLabel->setFixedSize(24, 24);
    iconLabel->setObjectName("iconSummarize");

    QLabel *titleLabel = new QLabel(this);
    titleLabel->setFixedHeight(24);
    titleLabel->setText(tr("Summarize"));
    titleLabel->setObjectName("titleLabel");
//    QPushButton *pinBtn = new QPushButton(this);
//    pinBtn->setFixedSize(16, 16);
//    pinBtn->setObjectName("pinBtn");
//    pinBtn->setCheckable(true);

    QPushButton *closeBtn = new QPushButton(this);
    closeBtn->setFixedSize(24, 24);
    closeBtn->setObjectName("closeBtn");
    connect(closeBtn, &QPushButton::clicked, this, &AIChatToolsWidget::returnToAiAssistant);
    QHBoxLayout *topLayout = new QHBoxLayout;
    topLayout->addSpacing(8);
    topLayout->addWidget(iconLabel);
    topLayout->addSpacing(8);
    topLayout->addWidget(titleLabel);
    topLayout->addStretch();
//    topLayout->addWidget(pinBtn, 0, Qt::AlignCenter);
    topLayout->addSpacing(8);
    topLayout->addWidget(closeBtn);
    topLayout->addSpacing(8);

    ChatInputWidget* chatInputWidget = new ChatInputWidget(this);
    chatInputWidget->setPlaceholderText(tr("Ask something"));
    chatInputWidget->setFixedHeight(40);
    chatInputWidget->setFocusoutCommit(false);
    connect(chatInputWidget, &ChatInputWidget::adjustHeight, this, [&, chatInputWidget] (int height) {
        adjustH(chatInputWidget, height);
    });
    QWeakPointer<bool> alive = getLifeWeakPtr();
    auto rspFun = [this, alive, chatInputWidget/*, pinBtn*/] {
        if (!alive.lock()) {
            return;
        }
        int height = 40;
        chatInputWidget->setPlainText("");
        adjustH(chatInputWidget, height);
        if (/*!pinBtn->isChecked()*/0) {
           returnToAiAssistant();
        } else {
            setCurrentToolState(ToolsState::Summarize);
        }
    };
    mCommitSummarizeRspFun = rspFun;
    connect(chatInputWidget, &ChatInputWidget::commitText, this, [&, chatInputWidget, rspFun/*, pinBtn*/] (const QString &text) {
        emit commitText(GptSummarize, text, "", rspFun);
    });
    mSummarizeFocusFun = [chatInputWidget] {
        chatInputWidget->setPlainText("");
        chatInputWidget->setFocus();
    };
    QVBoxLayout *layout = new QVBoxLayout;
    layout->addSpacing(11);
    layout->addLayout(topLayout);
    layout->addStretch();
    layout->addWidget(chatInputWidget);
    layout->setMargin(1);
    layout->setSpacing(0);
    widget->setLayout(layout);
    return widget;
}
void AIChatToolsWidget::adjustH(ChatInputWidget *chatInputWidget, int height) {
    height = qMax(40,height);
    int oldHeight = chatInputWidget->height();
    int diff = height - oldHeight;
    setFixedHeight(this->height() + diff);
    chatInputWidget->setFixedHeight(height);
}

QWeakPointer<bool> AIChatToolsWidget::getLifeWeakPtr()
{
    return mLifePtr.toWeakRef();
}

void AIChatToolsWidget::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    if (!mFirstShow) {
        mFirstShow = true;
        setCurrentToolState(Chat);
    }
}

bool AIChatToolsWidget::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == mToolsStateComboBox) {
        if (event->type() == QEvent::Wheel) {
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}

QWidget *AIChatToolsWidget::createExplainWidget()
{
    QWidget *widget = new QWidget(this);
    widget->setObjectName("toolsWidget");

    QLabel *iconLabel = new QLabel(this);
    iconLabel->setFixedSize(24, 24);
    iconLabel->setObjectName("iconExplain");

    QLabel *titleLabel = new QLabel(this);
    titleLabel->setFixedHeight(24);
    titleLabel->setText(tr("Explain"));
    titleLabel->setObjectName("titleLabel");
//    QPushButton *pinBtn = new QPushButton(this);
//    pinBtn->setFixedSize(16, 16);
//    pinBtn->setObjectName("pinBtn");
//    pinBtn->setCheckable(true);

    QPushButton *closeBtn = new QPushButton(this);
    closeBtn->setFixedSize(24, 24);
    closeBtn->setObjectName("closeBtn");
    connect(closeBtn, &QPushButton::clicked, this, &AIChatToolsWidget::returnToAiAssistant);
    QHBoxLayout *topLayout = new QHBoxLayout;
    topLayout->addSpacing(8);
    topLayout->addWidget(iconLabel);
    topLayout->addSpacing(8);
    topLayout->addWidget(titleLabel);
    topLayout->addStretch();
//    topLayout->addWidget(pinBtn, 0, Qt::AlignCenter);
    topLayout->addSpacing(8);
    topLayout->addWidget(closeBtn);
    topLayout->addSpacing(8);

    ChatInputWidget* chatInputWidget = new ChatInputWidget(this);
    chatInputWidget->setPlaceholderText(tr("Ask something"));
    chatInputWidget->setFixedHeight(40);
    chatInputWidget->setFocusoutCommit(false);
    connect(chatInputWidget, &ChatInputWidget::adjustHeight, this, [&, chatInputWidget] (int height) {
        adjustH(chatInputWidget, height);
    });
    QWeakPointer<bool> alive = getLifeWeakPtr();
    auto rspFun = [this, alive, chatInputWidget/*, pinBtn*/] {
        if (!alive.lock()) {
            return;
        }
        int height = 40;
        chatInputWidget->setPlainText("");
        adjustH(chatInputWidget, height);
        if (/*!pinBtn->isChecked()*/0) {
           returnToAiAssistant();
        } else {
           setCurrentToolState(ToolsState::Explain);
        }
    };
    mCommitExplainRspFun = rspFun;
    connect(chatInputWidget, &ChatInputWidget::commitText, this, [&, chatInputWidget, rspFun/*, pinBtn*/] (const QString &text) {
        emit commitText(GptExplain, text, "", rspFun);
    });
    mExplainFocusFun = [chatInputWidget] {
        chatInputWidget->setPlainText("");
        chatInputWidget->setFocus();
    };
    QVBoxLayout *layout = new QVBoxLayout;
    layout->addSpacing(11);
    layout->addLayout(topLayout);
    layout->addStretch();
    layout->addWidget(chatInputWidget);
    layout->setMargin(1);
    layout->setSpacing(0);
    widget->setLayout(layout);
    return widget;
}

QWidget *AIChatToolsWidget::createStopChatWidget()
{
    QWidget *widget = new QWidget(this);
    widget->setObjectName("stopChatWidget");

    LeftIconButton *stopChatBtn = new LeftIconButton(this);
    stopChatBtn->setLeftMargin(12);
    stopChatBtn->setRightMargin(18);
    stopChatBtn->setSpacing(6);
    stopChatBtn->setClickUncheck(false);
    stopChatBtn->create();
    QFont font;
    font.setPixelSize(14);
    QFontMetrics fm(font);
    QString showText = tr("Stop");
    stopChatBtn->setFixedSize(62 + fm.horizontalAdvance(showText), 40);
    stopChatBtn->setText(showText);
    stopChatBtn->setObjectName("stopChatBtn");
    connect(stopChatBtn, &LeftIconButton::clicked, this, &AIChatToolsWidget::onStopChat);

    QHBoxLayout *hLayout = new QHBoxLayout;
    hLayout->addStretch();
    hLayout->addWidget(stopChatBtn);

    QVBoxLayout *layout = new QVBoxLayout;
    layout->addStretch();
    layout->addLayout(hLayout);
    layout->setMargin(0);
    layout->setSpacing(0);
    widget->setLayout(layout);
    return widget;
}

QWidget *AIChatToolsWidget::createExportMsgWidget()
{
    QWidget *widget = new QWidget(this);
    widget->setObjectName("stopChatWidget");

    QPushButton *cancelBtn = new QPushButton(this);
    cancelBtn->setFixedHeight(32);
    cancelBtn->setText(tr("Cancel"));
    cancelBtn->setObjectName("frameBtn");
    connect(cancelBtn, &QPushButton::clicked, this, &AIChatToolsWidget::cancelExport);

    DiffColorTextButton *exportBtn = new DiffColorTextButton(this);
    mExportBtn = exportBtn;
    exportBtn->setFixedHeight(32);
    exportBtn->setText(tr("Export (%1)").arg(0));
    exportBtn->setEnabled(false);
    exportBtn->setObjectName("confirmBtn");
    connect(exportBtn, &QPushButton::clicked, this, [=] {
        UMenu *menu = new UMenu();
        GuiUtil::setMenuRoundStyle(menu);
        menu->setAttribute(Qt::WA_DeleteOnClose);
        menu->addAction(tr("Markdown (.md)"), this, [=] {
            emit confirmExport(ExportMarkdown);
        });
        menu->addAction(tr("Text (.txt)"), this, [=] {
            emit confirmExport(ExportText);
        });
        auto pos = exportBtn->mapToGlobal(exportBtn->pos()) + QPoint(-172, -106);
        menu->exec(pos);
    });

    QHBoxLayout *hLayout = new QHBoxLayout;
    hLayout->addWidget(cancelBtn);
    hLayout->addSpacing(16);
    hLayout->addWidget(exportBtn);
    hLayout->setMargin(0);
    hLayout->setSpacing(0);
    widget->setLayout(hLayout);
    return widget;
}

void AIChatToolsWidget::onStopChat()
{
    emit stopReqTask();
}

void AIChatToolsWidget::onLanguageChange(int index)
{

}

void AIChatToolsWidget::returnToAiAssistant()
{
    setCurrentToolState(ToolsState::Chat);
}

void AIChatToolsWidget::setCurrentToolState(ToolsState state)
{
    QString introduceType = "";
    switch (state) {
        case Chat:
            if (!mAIAssistantIntroduce) {
                mAIAssistantIntroduce = true;
                introduceType = "random_talk";
            }
        break;
        case Summarize:
            if (!mSummarizeIntroduce) {
                mSummarizeIntroduce = true;
                introduceType = "summary";
            }
        break;
        case Translate:
            if (!mTranslateIntroduce) {
                mTranslateIntroduce = true;
                introduceType = "translate";
            }
        break;
        case Explain:
            if (!mExplainIntroduce) {
                mExplainIntroduce = true;
                introduceType = "explain";
            }
        break;
        default:
            break;
    }
    if (!introduceType.isEmpty()) {
        emit getInstruction(introduceType);
    }
    setInputFocus();
    resizeByState(state);
    mStackedWidget->setCurrentIndex(state);
}

AIChatToolsWidget::ToolsState AIChatToolsWidget::currentToolState()
{
    return (AIChatToolsWidget::ToolsState)mStackedWidget->currentIndex();
}

void AIChatToolsWidget::setInputFocus()
{
    switch (mStackedWidget->currentIndex()) {
    case Chat:
    {
        if (mAssistantFocusFun) { mAssistantFocusFun(); }
    }
        break;
    case Translate:
    {
        if (mTranslateFocusFun) { mTranslateFocusFun(); }
    }
        break;
    case Summarize:
    {
        if (mSummarizeFocusFun) { mSummarizeFocusFun(); }
    }
        break;
    case Explain:
    {
        if (mExplainFocusFun) { mExplainFocusFun(); }
    }
        break;
    default:
        break;
    }
}

CommitRspCallback AIChatToolsWidget::getCommitRspFun(int state)
{
    CommitRspCallback callback = nullptr;
    switch (ToolsState(state)) {
    case Chat:
    {
        callback = mCommitAssistantRspFun;
    }
        break;
    case Translate:
    {
        callback = mCommitTranslateRspFun;
    }
        break;
    case Summarize:
    {
        callback = mCommitSummarizeRspFun;
    }
        break;
    case Explain:
    {
        callback = mCommitExplainRspFun;
    }
        break;
    default:
        break;
    }
    return callback;
}

void AIChatToolsWidget::setExportNum(int num)
{
    mExportBtn->setEnabled(num > 0);
    mExportBtn->setText(tr("Export (%1)").arg(num));
}

void AIChatToolsWidget::resizeByState(ToolsState state)
{
    switch (state) {
    case Chat:
        {
            setFixedHeight(82);
        }
        break;
    case Translate:
        {
            setFixedHeight(140);
        }
        break;
    case Summarize:
    case Explain:
    case StopChat:
        {
            setFixedHeight(88);
        }
        break;
    case ExportMsg:
        {
            setFixedHeight(32);
        }
        break;
    default:
        break;
    }
}

