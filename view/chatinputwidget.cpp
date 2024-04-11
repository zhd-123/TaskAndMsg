#include "chatinputwidget.h"
#include <QFontMetrics>
#include <QScrollBar>
#include "./src/util/fontutil.h"
#include "./src/util/guiutil.h"
#include "./utils/settingutil.h"

#define MaxInputHeight 125
ChatInputWidget::ChatInputWidget(QWidget *parent)
    : InputTextEdit{parent}
    , mFuncBtnVisiable(false)
{
    setAcceptRichText(false);
    horizontalScrollBar()->setVisible(false);
    mSendBtn = new QPushButton(this);
    mSendBtn->setShortcut(QKeySequence(Qt::Key_Return));
    mSendBtn->setObjectName("sendBtn");
    mSendBtn->setFixedSize(32, 32);
    connect(mSendBtn, &QPushButton::clicked, this, &ChatInputWidget::commit);

    mFuncBtn = new CShowTipsBtn(this);
    mFuncBtn->setFixedSize(24, 24);
    mFuncBtn->setObjectName("funcBtn");
    mFuncBtn->setToolTip("");
    mFuncBtn->setArrowPoint(CShowTipsBtn::BOTTOM);
    mFuncBtn->setCheckable(true);
    connect(mFuncBtn, &QPushButton::clicked, this, &ChatInputWidget::triggerFunc);

    connect(this, &ChatInputWidget::editFinished, this, &ChatInputWidget::commit);
    connect(this, &ChatInputWidget::textChanged, this, &ChatInputWidget::onTextChanged);

    mTopGradientLabel = new QLabel(this);
    mTopGradientLabel->setObjectName("topGradient");

    setFuncButtonVisiable(false);
}

void ChatInputWidget::setFuncButtonVisiable(bool visiable)
{
    mFuncBtnVisiable = visiable;
    mFuncBtn->setVisible(visiable);
}

void ChatInputWidget::setFuncButtonChecked(bool checked)
{
    mFuncBtn->setChecked(checked);
}

void ChatInputWidget::setFuncButtonStyle(const QString &type)
{
    mFuncBtn->setProperty("type", type);
    mFuncBtn->style()->unpolish(mFuncBtn);
    mFuncBtn->style()->polish(mFuncBtn);
}

bool ChatInputWidget::funButtonVisiable()
{
    return mFuncBtnVisiable;
}

bool ChatInputWidget::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == this) {
        if (QEvent::EnabledChange == event->type()) {
            mSendBtn->setEnabled(isEnabled());
        }
    }
    return InputTextEdit::eventFilter(watched, event);
}

void ChatInputWidget::resizeEvent(QResizeEvent *event)
{
    InputTextEdit::resizeEvent(event);
    onTextChanged();
    mSendBtn->setGeometry(width() - 4 - mSendBtn->width(), height() - 4 - mSendBtn->height(), mSendBtn->width(), mSendBtn->height());
    mFuncBtn->setGeometry(width() - 8 - mFuncBtn->width(), 12, mFuncBtn->width(), mFuncBtn->height());
    mTopGradientLabel->setGeometry(8, 5, width() - 16, 6);
}

void ChatInputWidget::commit()
{
    QString content = toPlainText().trimmed();
    if (content.isEmpty()) {
        return;
    }
    setPlainText("");
    emit commitText(content);
}

void ChatInputWidget::onTextChanged()
{
    QString content = toPlainText();
    QFont f = FontUtil::getSmartFont(content);
    if (f.family() != font().family()) {
        setFont(f);
    }
    QFontMetrics fm(f);
    int line = content.split("\n").count() - 1;
    int w = width() - 8 - 36;
    int textLen = fm.width(content);
    line += textLen / w;
//    line += textLen % w == 0 ? 0 : 1; // 考虑符号情况
    int textHeight = line * fm.height();
    textHeight += (line - 1) * 6;
    textHeight += 24;
    emit adjustHeight(qMin(textHeight, MaxInputHeight));
}

