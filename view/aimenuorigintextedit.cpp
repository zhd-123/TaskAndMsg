#include "aimenuorigintextedit.h"
#include "./src/util/guiutil.h"
#include <QTextEdit>
#include <QGraphicsEffect>
#include "./src/pdfeditor/priv/markuppagectrlpriv.h"
#include "./src/pdfannot/annotitemdata.h"
#include "./src/pdfannot/annotbaseitem.h"
#include "./src/pdfeditor/pdfeditor.h"
#include "./src/mainview.h"
#include "./src/chatgpt/chatgptmanager.h"
#include "./src/chatgpt/chatmainwidget.h"
#include "chatgpttypedefine.h"
#include <QApplication>
#include <QClipboard>
#include "./src/basicwidget/scrollareawidget.h"
#include "./src/baseabstract/abstracttoolswidget.h"
#include "./src/pdfeditor/pdfpageview.h"
#include "./src/util/configutil.h"
#include <QSettings>
#include "utils/langutil.h"
#include <QPropertyAnimation>
#include "./src/util/fontutil.h"
#include "./src/customwidget/lefticonbtn.h"
#include "./src/style/stylesheethelper.h"

#define PlainTextWidth() 320

GenerateWating::GenerateWating(QWidget* parent)
    : QLabel(parent),
      mIndexAnimation(nullptr)
{
    setFixedSize(24,24);
}

int GenerateWating::animationIndex()
{
    return mAnimationIndex;
}

void GenerateWating::setAnimationIndex(int index)
{
    mAnimationIndex = index;
    auto indexStr =  index < 10?QString("0%1").arg(index):QString::number(index);
    setStyleSheet(QString("image: url(:/updf-win-icon/ui2/2x/chatgpt/chat-loading/icon animation V2_000%1.png)").arg(indexStr));
}

void GenerateWating::runAnimation(bool run)
{
    if(mIndexAnimation == nullptr){
        mIndexAnimation = new QPropertyAnimation(this, "animationIndex");
        mIndexAnimation->setDuration(2500);
        mIndexAnimation->setStartValue(0);
        mIndexAnimation->setEndValue(49);
        mIndexAnimation->setLoopCount(100);
    }
    run?mIndexAnimation->start():
        mIndexAnimation->stop();
}

void GenerateWating::showEvent(QShowEvent* e)
{
    __super::showEvent(e);
    runAnimation(true);
}

void GenerateWating::closeEvent(QCloseEvent* e)
{
    __super::closeEvent(e);
    runAnimation(false);
}


OriginTextEdit::OriginTextEdit(QWidget* parent)
    : AbstractItemViewType<QtTextEdit>(parent)
{
    setObjectName("OriginTextEdit");
    initView();
}

void OriginTextEdit::initView()
{
    setAcceptRichText(false);
    setWordWrapMode(QTextOption::WrapAnywhere);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto barContainer = new ScrollBarContainer(this, this);
    barContainer->setFixedWidth(10);

    mViewContainer = new QWidget(this);
    auto barLayout = LayoutUtil::CreateHLayout(barContainer,0,0);
    barLayout->addItem(new QSpacerItem(0,0,QSizePolicy::Expanding,QSizePolicy::Expanding));
    barLayout->addWidget(verticalScrollBar());
    mViewContainer->setLayout(barLayout);

    mExpandBtn = new QPushButton(this);
    mExpandBtn->setObjectName("SpacerBtn");
    mExpandBtn->setCheckable(true);
    connect(mExpandBtn,&QPushButton::clicked,this,&OriginTextEdit::onExpandBtnClicked);
    connect(this, &QTextEdit::textChanged, this, [=] {
        mText = toPlainText();
        emit sameText(mOriginText == mText);
        QFontMetrics fontMet(font());
        bool moreThanOneLine = fontMet.horizontalAdvance(mText) > PlainTextWidth();
        if (mExpandBtn->isHidden()) {
            if (moreThanOneLine) {
                mExpandBtn->show();
                mExpandBtn->setChecked(true);
                adjustShow(isExpand());
            }
        } else {
            if(!moreThanOneLine){
                mExpandBtn->hide();
            }
        }
    });
}

bool OriginTextEdit::isExpand()
{
    return mExpandBtn->isChecked();
}

void OriginTextEdit::onExpandBtnClicked(bool checked)
{
    if(!checked){
        mText = toPlainText();
    }
    adjustHeight(checked);
    setFocus();
    emit expand(checked);
}

void OriginTextEdit::setOriginText(const QString& originText)
{
    mOriginText = originText;
    mText = originText;
    setPlainText(originText);
    setFont(FontUtil::getSmartFont(originText));
}

void OriginTextEdit::updateLineHeight()
{
    QTextCursor textCursor = this->textCursor();
    QTextBlockFormat textBlockFormat;
    textBlockFormat.setLineHeight(20, QTextBlockFormat::FixedHeight);//设置固定行高
    textCursor.setBlockFormat(textBlockFormat);
    setTextCursor(textCursor);
}

QString OriginTextEdit::originText()
{
    if(mExpandBtn->isChecked()){
        mText = toPlainText();
    }
    return mText;
}

int OriginTextEdit::maxHeight(int max)
{
    auto docHeight = int(document()->size().height())+4;
    return docHeight > max?max:docHeight;
}

void OriginTextEdit::adjustHeight(bool open)
{
    auto plainText = open?mText:(GuiUtil::elideLabel(mText,font(),PlainTextWidth(),Qt::ElideRight).replace("\n",""));
    blockSignals(true);
    setPlainText(plainText);
    QTimer::singleShot(100, this, [&] { blockSignals(false); });

    //不相等时为有缩略号状态
    if(plainText == mText){
        if(!open){
            mExpandBtn->hide();
        }
        setReadOnly(false);
    }
    else{
        setReadOnly(true);
    }
    adjustShow(open);
}

void OriginTextEdit::adjustShow(bool open)
{
    updateLineHeight();
    setFixedHeight(open?maxHeight(140):32);
    QTimer::singleShot(100,this,[this](){
        updateBtnPos();
        if(mExpandBtn->isChecked()){
            moveCursor(QTextCursor::End);
            setFocus();
        }
        else{
            clearFocus();
        }
    });
}

void OriginTextEdit::updateBtnPos()
{
    //4 4 内边距
    mExpandBtn->move(QPoint(width()-mExpandBtn->width() - 4,
                            height() - mExpandBtn->height() - 4));
    mViewContainer->move(mExpandBtn->x()+(mExpandBtn->width() - mViewContainer->width()),4);
}

void OriginTextEdit::showEvent(QShowEvent *e)
{
    __super::showEvent(e);
    adjustHeight(false);
}

void OriginTextEdit::resizeEvent(QResizeEvent* e)
{
    __super::resizeEvent(e);
    mViewContainer->resize(QSize(10,e->size().height() - mExpandBtn->height() - 4*2));
}

void OriginTextEdit::mousePressEvent(QMouseEvent* event)
{
    __super::mousePressEvent(event);
    if(!mExpandBtn->isHidden() && !mExpandBtn->isChecked()){
        mExpandBtn->setChecked(true);
        onExpandBtnClicked(mExpandBtn->isChecked());
    }
}
