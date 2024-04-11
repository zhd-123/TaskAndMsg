#include "basechatdelegate.h"
#include <QEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QTextBlock>
#include <QTextDocument>
#include <QTextCharFormat>
#include <QApplication>
#include <QTextEdit>
#include <QPropertyAnimation>
#include <QParallelAnimationGroup>
#include <QDebug>
#include <QAbstractTextDocumentLayout>
#include <QListView>
#include "./src/customwidget/tooltipwin.h"
#include "utils/langutil.h"
#include "utils/settingutil.h"
#include "./src/style/stylesheethelper.h"
#include "./src/util/fontutil.h"
#include "./src/util/cursorutil.h"
#include "umenu.h"

#define ContentFontSize 13
#define ToolBarFontSize 10
#define Padding 12
#define Spacing 4
#define ToolBarHeight 28
#define ContentRectYOffset 28 //ToolBarHeight + Spacing
#define IconWidth 24
#define RectBlankHeight 4//16
#define TextYPadding 2
#define PageGap 4
#define PageLineGap 6
#define PageLineHeight 24
#define MinWidgetWidth 162
#define CopyHeight 32
#define CheckBoxWidth 16
#define CheckBoxFullWidth 32

void showAnimation(const QRect &rect, const QString &picPath, QWidget *parent)
{
    QLabel *label = new QLabel(parent);
    label->setAttribute(Qt::WA_TranslucentBackground);
    label->setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
    label->setGeometry(rect);
    label->show();
    int duration = 600;
    QPropertyAnimation *pOpacityAnimation1 = new QPropertyAnimation(label, "windowOpacity");
    QObject::connect(pOpacityAnimation1, &QPropertyAnimation::finished, label, &QLabel::deleteLater);
    QObject::connect(pOpacityAnimation1, &QPropertyAnimation::valueChanged, label, [=](const QVariant &value) {
        double opacity = value.value<double>();
        if (opacity < 0.8) {
            return;
        }
        QSize s = rect.size() * opacity;
        label->setPixmap(GuiUtil::generateSmoothPic(picPath, s));
    });
    pOpacityAnimation1->setDuration(duration);
    pOpacityAnimation1->setKeyValueAt(0, 0.90);
    pOpacityAnimation1->setKeyValueAt(0.5, 1.0);
    pOpacityAnimation1->setKeyValueAt(1, 0.90);
    pOpacityAnimation1->start(QPropertyAnimation::DeleteWhenStopped);
}

QFont getSmartFont(const QString &text)
{
    QFont font = FontUtil::getSmartFont(text);
    font.setPixelSize(ContentFontSize);
    return font;
}
template <typename T>
int calcRectWidth(const QStyleOptionViewItem &option, const T &modelData)
{
    int fixWidth = 0;
    int widgetWidth = 0;
    QFontMetrics fm(getSmartFont(modelData.content));
    int len = fm.horizontalAdvance(modelData.content);
    if (modelData.messageType == AI_TYPE_NAME && !modelData.checkBox) {
        widgetWidth = qMax(MinWidgetWidth, len + 2 * Padding);
        // fix
//        if (modelData.content.startsWith("- ") || modelData.content.startsWith("1.") || modelData.content.startsWith("* ")
//                || modelData.content.startsWith("+ ")) {
//            if (widgetWidth < option.rect.width()) {
//                widgetWidth +=  40;
//                widgetWidth = qMin(widgetWidth, option.rect.width());
//            }
//        }
    } else {
        widgetWidth = len + 2 * Padding;
    }
    int w = option.rect.width();
    if (modelData.checkBox) {
        w -= CheckBoxFullWidth;
    }
    fixWidth = widgetWidth < w ? widgetWidth : w;
    return fixWidth;
}

AIToolsDelegate::AIToolsDelegate(QObject *parent)
    : QStyledItemDelegate(parent)
    , mTipsWidget(nullptr)
    , mPicIndex(0)
    , mInCopyRect(false)
    , mPressCopyRect(false)
    , mInExportRect(false)
    , mPressExportRect(false)
    , mInRegenerateRect(false)
    , mPressRegenerateRect(false)
    , mInAgreeRect(false)
    , mPressAgreeRect(false)
    , mInDisagreeRect(false)
    , mPressDisagreeRect(false)
    , mInCheckBoxRect(false)
    , mPressCheckBoxRect(false)
    , mInRefreshRect(false)
    , mPressRefreshRect(false)
{
}

AIToolsDelegate::~AIToolsDelegate()
{
    if (mTipsWidget) {
        mTipsWidget->close();
        mTipsWidget->deleteLater();
        mTipsWidget = nullptr;
    }
}

void AIToolsDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    painter->setRenderHint(QPainter::Antialiasing);
    QVariant userData = index.data(Qt::UserRole);
    if (userData.canConvert<RspGptStruct>()) {
        RspGptStruct modelData = userData.value<RspGptStruct>();
        if (modelData.fetchData) {
            QPen pen;
            pen.setColor(QColor("#353535"));
            painter->setPen(pen);
            painter->setFont(getSmartFont(modelData.content));
            painter->drawText(option.rect, tr("Loading…"), Qt::AlignHCenter | Qt::AlignVCenter);
        } else if (modelData.waitingAnimation) {
            drawWaiting(painter, option, modelData);
        } else {
            // toolbar
            drawToolBar(painter, option, modelData);

            int fixWidth = calcRectWidth(option, modelData);
            QRect contentRect;
            QBrush brush;
            int leftMargin = 0;
            if (modelData.checkBox) {
                leftMargin = CheckBoxFullWidth;
            }
            if (modelData.messageType == AI_TYPE_NAME) {
                contentRect = QRect(option.rect.x() + leftMargin, option.rect.y() + ContentRectYOffset, fixWidth, option.rect.height() - ContentRectYOffset - RectBlankHeight - Spacing);
                brush = QBrush(Qt::white);
            } else {
                if (modelData.type != GptRandomChat) {
                    contentRect = QRect(option.rect.x() + option.rect.width() - fixWidth, option.rect.y() + ContentRectYOffset, fixWidth, option.rect.height() - ContentRectYOffset - RectBlankHeight - Spacing);
                } else {
                    contentRect = QRect(option.rect.x() + option.rect.width() - fixWidth, option.rect.y(), fixWidth, option.rect.height() - RectBlankHeight - Spacing);
                }
                QLinearGradient lineGradient(contentRect.topLeft(), contentRect.topRight());
                lineGradient.setColorAt(0, "#AE72FF");
                lineGradient.setColorAt(1, "#868EFF");
                brush = QBrush(lineGradient);
            }
            if (modelData.checkBox) {
                QString path = modelData.isChecked ? ":/updf-win-icon/ui2/2x/print/checkbox/checkbox-select-1.png" : ":/updf-win-icon/ui2/2x/print/checkbox/checkbox-normal-1.png";
                painter->drawPixmap(QRect(option.rect.x(), contentRect.y() + (contentRect.height() - CheckBoxWidth)/2, CheckBoxWidth + 1, CheckBoxWidth + 1), GuiUtil::generateSmoothPic(path, QSize(CheckBoxWidth, CheckBoxWidth)));
            }
            QPainterPath path;
            path.addRoundedRect(contentRect, 8, 8);
            painter->fillPath(path, brush);
            // showContent
            drawContent(painter, contentRect, option, modelData);
        }
    }
}
void AIToolsDelegate::drawWaiting(QPainter *painter, const QStyleOptionViewItem &option, const RspGptStruct &modelData) const
{
    painter->save();
    QPen pen;
    pen.setColor(QColor("#353535"));
    painter->setPen(pen);

    QFontMetrics fm(getSmartFont(modelData.content));
    int len = fm.horizontalAdvance(modelData.content);
    int minWidth = 76;
    int width = 56 + 8 + len > minWidth ? 56 + 8 + len : minWidth;
    QRect iconRect;
    QRect contentRect = QRect(option.rect.x(), option.rect.y(), width, option.rect.height() - RectBlankHeight);
    QPainterPath path;
    path.addRoundedRect(contentRect, 8, 8);
    painter->fillPath(path, QBrush(Qt::white));
    if (modelData.content.isEmpty()) {
        iconRect = QRect(option.rect.x() + (contentRect.width() - IconWidth) / 2, option.rect.y() + (option.rect.height()- RectBlankHeight - IconWidth) / 2, IconWidth, IconWidth);
    } else {
        iconRect = QRect(option.rect.x() + 8, option.rect.y() + (option.rect.height()-RectBlankHeight - IconWidth) / 2, IconWidth, IconWidth);
        QRect textRect = QRect(option.rect.x() + 8 + IconWidth + 8, option.rect.y(), width - IconWidth - 8 - 8, option.rect.height() - RectBlankHeight);
        painter->drawText(textRect, modelData.content, Qt::AlignLeft | Qt::AlignVCenter);
    }
    QString imagePath = QString("://updf-win-icon/ui2/2x/chatgpt/chat-loading/icon animation V2_000%1.png").arg(QString::number(mPicIndex).rightJustified(2, '0'));
    painter->drawPixmap(iconRect, GuiUtil::generateSmoothPic(imagePath, iconRect.size()));
    if (++mPicIndex > 49) {
        mPicIndex = 0;
    }
    painter->restore();
}

void AIToolsDelegate::drawToolBar(QPainter *painter, const QStyleOptionViewItem &option, const RspGptStruct &modelData) const
{
    painter->save();
    QRect toolBarRect = QRect(option.rect.x(), option.rect.y(), option.rect.width(), ToolBarHeight);
    QRect textRect;
    QRect iconRect;

    QFont font = FontUtil::getSmartFont(modelData.content);
    font.setPixelSize(ToolBarFontSize);
    painter->setFont(font);
    QPen pen;
    if (modelData.messageType == AI_TYPE_NAME) {
        pen.setColor(QColor("#8A8A8A"));
        painter->setPen(pen);
        textRect = QRect(toolBarRect.x(), toolBarRect.y() - TextYPadding, toolBarRect.width() - IconWidth, ToolBarHeight);
        painter->drawText(textRect, tr("UPDF AI"), Qt::AlignLeft | Qt::AlignVCenter);
        if (!modelData.checkBox) {
            QRect iconRect = QRect(toolBarRect.bottomRight() + QPoint(-24, -ToolBarHeight),QSize(IconWidth, IconWidth));
            QString imagePath = "://updf-win-icon/ui2/2x/chatgpt/copy text.png";
            if(mInCopyRect && option.state.testFlag(QStyle::State_MouseOver)){
                imagePath = "://updf-win-icon/ui2/2x/chatgpt/copy text-hover.png";
            }
            if (mPressCopyRect && option.state.testFlag(QStyle::State_MouseOver)) {
                imagePath = "://updf-win-icon/ui2/2x/chatgpt/copy text-pressed.png";
            }
            painter->drawPixmap(iconRect, GuiUtil::generateSmoothPic(imagePath, iconRect.size()));
        }
    } else {
        if (modelData.type != GptRandomChat) {
            pen.setColor(QColor("#8A8A8A"));
            painter->setPen(pen);
            QFontMetrics fm(painter->font());
            QString title = getTitle(modelData.type);
            int titleWidth = fm.horizontalAdvance(title) + 8;
            textRect = QRect(toolBarRect.x() + toolBarRect.width() - titleWidth, toolBarRect.y() - TextYPadding, titleWidth, ToolBarHeight);
            painter->drawText(textRect, title, Qt::AlignLeft | Qt::AlignVCenter);

            iconRect = QRect(toolBarRect.x() + toolBarRect.width() - IconWidth - titleWidth - 4, toolBarRect.y(), IconWidth, IconWidth);
            painter->drawPixmap(iconRect, GuiUtil::generateSmoothPic("://updf-win-icon/ui2/2x/chatgpt/Ai assistant.png", iconRect.size()));
        }
    }
    painter->restore();
}

void AIToolsDelegate::drawContent(QPainter *painter, const QRect &contentRect, const QStyleOptionViewItem &option, const RspGptStruct &modelData) const
{
    painter->save();
    QString content = modelData.content;

    auto font = getSmartFont(modelData.content);
    painter->setFont(font);
    QPen pen;
    if (modelData.messageType == AI_TYPE_NAME) {
        pen.setColor("#353535");
    } else {
        pen.setColor(Qt::white);
    }
    painter->setPen(pen);
    QTextDocument textDoc;
    textDoc.setDefaultFont(font);
    QAbstractTextDocumentLayout::PaintContext ctx;
    ctx.palette.setColor(QPalette::Text, painter->pen().color());

    painter->setClipRect(contentRect);
    painter->setClipping(true);
    textDoc.setPlainText(content);
    textDoc.setDocumentMargin(0);
    int textWidth = qMin(contentRect.width() - (2 * Padding) + 2, option.rect.width() - (2 * Padding));
    textDoc.setTextWidth(textWidth);
    painter->translate(contentRect.topLeft() + QPoint(Padding, Padding - 2));
    textDoc.documentLayout()->draw(painter, ctx);
    painter->resetTransform();
    if (modelData.messageType == AI_TYPE_NAME && modelData.finishContent && !modelData.exception) {
        if (!modelData.checkBox && !modelData.instruction) {
            if (!modelData.contentEmpty) {
                QRect exportIconRect = QRect(contentRect.bottomRight() + QPoint(-32 * 4 - 24, -32),QSize(IconWidth, IconWidth));;
                QRect regenerateIconRect = QRect(contentRect.bottomRight() + QPoint(-32 * 3 - 24, -32),QSize(IconWidth, IconWidth));;
                QRect splitIconRect = QRect(contentRect.bottomRight() + QPoint(-32 * 2 - 17, -21 - 5),QSize(1, 10));
                QRect agreeIconRect = QRect(contentRect.bottomRight() + QPoint(-32 * 2, -32),QSize(IconWidth, IconWidth));
                QRect disagreeIconRect = QRect(contentRect.bottomRight() + QPoint(-32, -32),QSize(IconWidth, IconWidth));
                QString exportImagePath = "://updf-win-icon/ui2/2x/chatgpt/export.png";
                if(mInExportRect && option.state.testFlag(QStyle::State_MouseOver)){
                    exportImagePath = "://updf-win-icon/ui2/2x/chatgpt/export-hover.png";
                }
                if (mPressExportRect && option.state.testFlag(QStyle::State_MouseOver)) {
                    exportImagePath = "://updf-win-icon/ui2/2x/chatgpt/export-pressed.png";
                }
                painter->drawPixmap(exportIconRect, GuiUtil::generateSmoothPic(exportImagePath, exportIconRect.size()));

                QString regenerateImagePath = "://updf-win-icon/ui2/2x/chatgpt/regenerate.png";
                if(mInRegenerateRect && option.state.testFlag(QStyle::State_MouseOver)){
                    regenerateImagePath = "://updf-win-icon/ui2/2x/chatgpt/regenerate-hover.png";
                }
                if (mPressRegenerateRect && option.state.testFlag(QStyle::State_MouseOver)) {
                    regenerateImagePath = "://updf-win-icon/ui2/2x/chatgpt/regenerate-pressed.png";
                }
                painter->drawPixmap(regenerateIconRect, GuiUtil::generateSmoothPic(regenerateImagePath, regenerateIconRect.size()));

                QString splitImagePath = "://updf-win-icon/ui2/2x/chatgpt/split.png";
                painter->drawPixmap(splitIconRect, GuiUtil::generateSmoothPic(splitImagePath, splitIconRect.size()));

                QString agreeImagePath = modelData.like == LikeAgree ? "://updf-win-icon/ui2/2x/chatgpt/thumb up-pressed.png"
                                                         : "://updf-win-icon/ui2/2x/chatgpt/thumb up.png";
                if(mInAgreeRect && option.state.testFlag(QStyle::State_MouseOver) && modelData.like != LikeAgree){
                    agreeImagePath = "://updf-win-icon/ui2/2x/chatgpt/thumb up-hover.png";
                }
                if (mPressAgreeRect && option.state.testFlag(QStyle::State_MouseOver)) {
                    agreeImagePath = "://updf-win-icon/ui2/2x/chatgpt/thumb up-pressed.png";
                }
                if (modelData.recordId == 0) {
                    agreeImagePath = "://updf-win-icon/ui2/2x/chatgpt/thumb up-disabled.png";
                }
                painter->drawPixmap(agreeIconRect, GuiUtil::generateSmoothPic(agreeImagePath, agreeIconRect.size()));

                QString disagreeImagePath = modelData.like == LikeDisagree ? "://updf-win-icon/ui2/2x/chatgpt/thumb down-pressed.png"
                                                         : "://updf-win-icon/ui2/2x/chatgpt/thumb down.png";
                if(mInDisagreeRect && option.state.testFlag(QStyle::State_MouseOver) && modelData.like != LikeDisagree){
                    disagreeImagePath = "://updf-win-icon/ui2/2x/chatgpt/thumb down-hover.png";
                }
                if (mPressDisagreeRect && option.state.testFlag(QStyle::State_MouseOver)) {
                    disagreeImagePath = "://updf-win-icon/ui2/2x/chatgpt/thumb down-pressed.png";
                }
                if (modelData.recordId == 0) {
                    disagreeImagePath = "://updf-win-icon/ui2/2x/chatgpt/thumb down-disabled.png";
                }
                painter->drawPixmap(disagreeIconRect, GuiUtil::generateSmoothPic(disagreeImagePath, disagreeIconRect.size()));
            }
            if (modelData.contentEmpty) {
                QString tip = tr("Retry");
                QFontMetrics fm(getSmartFont(tip));
                int len = fm.horizontalAdvance(tip) + 4;

                QRect iconRect = QRect(contentRect.bottomRight() + QPoint(-36 - len, -32), QSize(IconWidth, IconWidth));
                QRect tipRect = QRect(contentRect.bottomRight() + QPoint(-12 - len, -32), QSize(len, 24));
                QRect refreshRect = QRect(contentRect.bottomRight() + QPoint(-36 - len, -32), QSize(IconWidth + len, 24));
                QPainterPath path;
                path.addRoundedRect(refreshRect, 4, 4);
                QString imagePath = "://updf-win-icon/ui2/2x/chatgpt/refresh.png";
                if(option.state.testFlag(QStyle::State_MouseOver)){
                    if (mInRefreshRect) {
                        imagePath = "://updf-win-icon/ui2/2x/chatgpt/refresh.png";
                        painter->fillPath(path, QColor("#F7F7F7"));
                    } else {
                        pen.setColor("#ECECEC");
                        pen.setWidth(1);
                        painter->setPen(pen);
                        painter->drawPath(path);
                    }
                    if (mPressRefreshRect) {
                        imagePath = "://updf-win-icon/ui2/2x/chatgpt/refresh-hover.png";
                        painter->fillPath(path, QColor("#E9DAFF"));
                    }
                } else {
                    pen.setColor("#ECECEC");
                    pen.setWidth(1);
                    painter->setPen(pen);
                    painter->drawPath(path);
                }
                if (mPressRefreshRect && option.state.testFlag(QStyle::State_MouseOver)) {
                    pen.setColor("#964FF2");
                } else {
                    pen.setColor("#353535");
                }
                painter->setPen(pen);
                painter->drawText(tipRect.adjusted(0, 4, 0, -3), tip);
                painter->drawPixmap(iconRect, GuiUtil::generateSmoothPic(imagePath, iconRect.size()));
             }
        }
    }
    painter->restore();
}

QSize AIToolsDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    QVariant userData = index.data(Qt::UserRole);
    if (userData.canConvert<RspGptStruct>()) {
        RspGptStruct modelData = userData.value<RspGptStruct>();
        if (modelData.fetchData) {
            return QSize(option.rect.width(), 36);
        } else if (modelData.waitingAnimation) {
            return QSize(option.rect.width(), 36 + RectBlankHeight);
        }

        int height = 0;
        int contentWidth = option.rect.width() - (2 * Padding);
        if (modelData.checkBox) {
            contentWidth -= CheckBoxFullWidth;
        }
        QTextDocument textDoc;
        textDoc.setDefaultFont(getSmartFont(modelData.content));
        textDoc.setPlainText(modelData.content);
        textDoc.setDocumentMargin(0);
        textDoc.setTextWidth(contentWidth);
        height += textDoc.size().height();
        height += 2 * Padding;
        if (modelData.type != GptRandomChat || modelData.messageType == AI_TYPE_NAME) {
            height += ToolBarHeight;
        }
        if (modelData.messageType == AI_TYPE_NAME && modelData.finishContent && !modelData.checkBox && !modelData.instruction
                && !modelData.exception) {
            height += CopyHeight;
        }
        height += RectBlankHeight; // spacing
        return QSize(option.rect.width(), height);
    }
    return QStyledItemDelegate::sizeHint(option,index);
}

bool AIToolsDelegate::editorEvent(QEvent *event, QAbstractItemModel *model, const QStyleOptionViewItem &option, const QModelIndex &index)
{
    Q_UNUSED(model)
    QVariant userData = index.data(Qt::UserRole);
    if (!userData.canConvert<RspGptStruct>()) {
        return false;
    }
    RspGptStruct modelData = userData.value<RspGptStruct>();
    if (modelData.waitingAnimation) {
        return false;
    }
    int fixWidth = calcRectWidth(option, modelData);
    QRect copyIconRect;
    QRect exportIconRect;
    QRect regenerateIconRect;
    QRect agreeIconRect;
    QRect disagreeIconRect;
    QRect contentRect;
    QRect checkBoxRect;
    QRect refreshRect;

    int leftMargin = 0;
    if (modelData.checkBox) {
        leftMargin = CheckBoxFullWidth;
    }
    if (modelData.messageType == AI_TYPE_NAME) {
        contentRect = QRect(option.rect.x() + leftMargin, option.rect.y() + ContentRectYOffset, fixWidth, option.rect.height() - ContentRectYOffset - RectBlankHeight - Spacing);
    } else {
        if (modelData.type != GptRandomChat) {
            contentRect = QRect(option.rect.x() + option.rect.width() - fixWidth, option.rect.y() + ContentRectYOffset, fixWidth, option.rect.height() - ContentRectYOffset - RectBlankHeight - Spacing);
        } else {
            contentRect = QRect(option.rect.x() + option.rect.width() - fixWidth, option.rect.y(), fixWidth, option.rect.height() - RectBlankHeight - Spacing);
        }
    }

    if (modelData.messageType == AI_TYPE_NAME) {
        QRect toolBarRect = QRect(option.rect.x(), option.rect.y(), option.rect.width(), ToolBarHeight);
        copyIconRect = QRect(toolBarRect.bottomRight() + QPoint(-24, -ToolBarHeight),QSize(IconWidth, IconWidth));

        if (!modelData.exception) {
            if (modelData.contentEmpty) {
                QString tip = tr("Retry");
                QFontMetrics fm(getSmartFont(tip));
                int len = fm.horizontalAdvance(tip) + 4;
                refreshRect = QRect(contentRect.bottomRight() + QPoint(-36 - len, -32), QSize(IconWidth + len, 24));
            } else {
                if (!modelData.instruction) {
                    exportIconRect = QRect(contentRect.bottomRight() + QPoint(-32 * 4 - 24, -32),QSize(IconWidth, IconWidth));;
                    regenerateIconRect = QRect(contentRect.bottomRight() + QPoint(-32 * 3 - 24, -32),QSize(IconWidth, IconWidth));;
                    agreeIconRect = QRect(contentRect.bottomRight() + QPoint(-32 * 2, -32),QSize(IconWidth, IconWidth));
                    disagreeIconRect = QRect(contentRect.bottomRight() + QPoint(-32, -32),QSize(IconWidth, IconWidth));
                }
            }
        }
    }
    if (modelData.checkBox) {
        checkBoxRect = QRect(option.rect.x(), contentRect.y() + (contentRect.height() - CheckBoxWidth)/2, CheckBoxWidth + 1, CheckBoxWidth + 1);
    }
    if (event->type() == QEvent::MouseButtonPress) {
        QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
        if(mouseEvent->button() == Qt::LeftButton)
        {
            if (!modelData.checkBox) {
                if (copyIconRect.contains(mouseEvent->pos())) {
                    QPoint point(option.widget->mapToGlobal(QPoint(copyIconRect.center())));
                    point += QPoint(0, 12);
                    showTip(tr("Copied!"), point);
                    QApplication::clipboard()->setText(modelData.content);
                    mPressCopyRect = true;
                    emit refreshItem(index.row());
                }
            } else {
                if (checkBoxRect.contains(mouseEvent->pos())) {
                    mPressCheckBoxRect = true;
                }
            }
            if (modelData.finishContent) {
                if (!modelData.checkBox) {
                    if (modelData.contentEmpty) {
                        if (refreshRect.contains(mouseEvent->pos())) {
                            mPressRefreshRect = true;
                        }
                    } else {
                        if (!modelData.instruction) {
                            if (exportIconRect.contains(mouseEvent->pos())) {
                                mPressExportRect = true;
                            } else if (regenerateIconRect.contains(mouseEvent->pos())) {
                                mPressRegenerateRect = true;
                            } else if (agreeIconRect.contains(mouseEvent->pos())) {
                                if (modelData.recordId != 0) {
                                    mPressAgreeRect = true;
                                }
                            } else if (disagreeIconRect.contains(mouseEvent->pos())) {
                                if (modelData.recordId != 0) {
                                    mPressDisagreeRect = true;
                                }
                            }
                        }
                    }
                }
            }
        }
    } else if (event->type() == QEvent::MouseButtonRelease) {
        if (!modelData.checkBox) {
            if (mPressCopyRect) {
                mPressCopyRect = false;
                emit refreshItem(index.row());
            }
        } else {
            if (mPressCheckBoxRect) {
                mPressCheckBoxRect = false;
                emit checkItem(index.row());
            }
        }
        if (modelData.finishContent) {
            if (!modelData.checkBox) {
                if (modelData.contentEmpty) {
                    if (mPressRefreshRect) {
                        mPressRefreshRect = false;
                        emit retryTrigger(index.row());
                    }
                } else {
                    if (!modelData.instruction) {
                        if (mPressExportRect) {
                            mPressExportRect = false;
                            emit exportTrigger(index.row());
                            hideTip();
                        } else if (mPressRegenerateRect) {
                            mPressRegenerateRect = false;
                            emit regenerateTrigger(index.row());
                            emit refreshItem(index.row());
                            hideTip();
                        } else if (mPressAgreeRect) {
                            mPressAgreeRect = false;
                            int oldLike = modelData.like;
                            modelData.like = modelData.like == LikeAgree ? LikeNull : LikeAgree;
                            emit likeItem(index.row(), modelData, oldLike);
                            emit updateItem(index.row(), modelData);
                            if (modelData.like != LikeNull) {
                                QRect innerRect = agreeIconRect.adjusted(-2, -4, 6, 4);
                                showAnimation(QRect(QPoint(option.widget->mapToGlobal(innerRect.topLeft())), QPoint(option.widget->mapToGlobal(innerRect.bottomRight()))), "://updf-win-icon/ui2/2x/chatgpt/like.png", const_cast<QWidget*>(option.widget));
                            }
                        } else if (mPressDisagreeRect) {
                            mPressDisagreeRect = false;
                            int oldLike = modelData.like;
                            modelData.like = modelData.like == LikeDisagree ? LikeNull : LikeDisagree;
                            emit likeItem(index.row(), modelData, oldLike);
                            emit updateItem(index.row(), modelData);
                            if (modelData.like != LikeNull) {
                                QRect innerRect = disagreeIconRect.adjusted(0, -4, 8, 4);
                                showAnimation(QRect(QPoint(option.widget->mapToGlobal(innerRect.topLeft())), QPoint(option.widget->mapToGlobal(innerRect.bottomRight()))),  "://updf-win-icon/ui2/2x/chatgpt/dislike.png", const_cast<QWidget*>(option.widget));
                            }
                        }
                    }
               }
            }
        }
    } else if (event->type() == QEvent::MouseMove) {
        QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
        if (contentRect.contains(mouseEvent->pos()) && !modelData.checkBox && !exportIconRect.contains(mouseEvent->pos())
                && !regenerateIconRect.contains(mouseEvent->pos()) && !agreeIconRect.contains(mouseEvent->pos())
                && !disagreeIconRect.contains(mouseEvent->pos()) && !copyIconRect.contains(mouseEvent->pos())) {
            // edit mode
            emit triggerEditor(mouseEvent->pos());
        }
        if (modelData.messageType == AI_TYPE_NAME) {
            if (!modelData.checkBox) {
                if (copyIconRect.contains(mouseEvent->pos())) {
                    mInCopyRect = true;
                    if (mTipsWidget && (mTipsWidget->tips() == tr("Copied!") || mTipsWidget->tips() == tr("Copy"))) {
                    } else {
                        QPoint point(option.widget->mapToGlobal(QPoint(copyIconRect.center())));
                        point += QPoint(0, 12);
                        showTip(tr("Copy"), point);
                    }
                } else {
                    mInCopyRect = false;
                    mPressCopyRect = false;
                }
            } else {
                if (checkBoxRect.contains(mouseEvent->pos())) {
                    mInCheckBoxRect = true;
                } else {
                    mInCheckBoxRect = false;
                    mPressCheckBoxRect = false;
                }
            }
            if (modelData.finishContent) {
                if (!modelData.checkBox) {
                    if (modelData.contentEmpty) {
                        if (refreshRect.contains(mouseEvent->pos())) {
                            mInRefreshRect = true;
                        } else {
                            mInRefreshRect = false;
                            mPressRefreshRect = false;
                        }
                    } else {
                        if (!modelData.instruction) {
                            if (exportIconRect.contains(mouseEvent->pos())) {
                                mInExportRect = true;
                                if (mTipsWidget && mTipsWidget->tips() == tr("Export")) {
                                } else {
                                    QPoint point(option.widget->mapToGlobal(QPoint(exportIconRect.center())));
                                    point += QPoint(0, 12);
                                    showTip(tr("Export"), point);
                                }
                            } else {
                                mInExportRect = false;
                                mPressExportRect = false;
                            }
                            if (regenerateIconRect.contains(mouseEvent->pos())) {
                                mInRegenerateRect = true;
                                if (mTipsWidget && mTipsWidget->tips() == tr("Regenerate")) {
                                } else {
                                    QPoint point(option.widget->mapToGlobal(QPoint(regenerateIconRect.center())));
                                    point += QPoint(0, 12);
                                    showTip(tr("Regenerate"), point);
                                }
                            } else {
                                mInRegenerateRect = false;
                                mPressRegenerateRect = false;
                            }
                            if (agreeIconRect.contains(mouseEvent->pos())) {
                                mInAgreeRect = true;
                            } else {
                                mInAgreeRect = false;
                                mPressAgreeRect = false;
                            }
                            if (disagreeIconRect.contains(mouseEvent->pos())) {
                                mInDisagreeRect = true;
                            } else {
                                mInDisagreeRect = false;
                                mPressDisagreeRect = false;
                            }
                        }
                    }
                    if (!mInCopyRect && !mInExportRect && !mInRegenerateRect/* && !mInAgreeRect && !mInDisagreeRect*/) {
                        hideTip();
                    }
                }
            }
        }
        emit refreshItem(index.row());
    }
    return QStyledItemDelegate::editorEvent(event, model, option, index);
}

QWidget* AIToolsDelegate::createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    QWidget* ret = nullptr;
    QVariant userData = index.data(Qt::UserRole);
    RspGptStruct modelData = userData.value<RspGptStruct>();
    if (!modelData.content.isEmpty() && !modelData.checkBox)
    {
        QTextEdit *textEdit = new CopyTextEdit(parent);
        connect(textEdit, &QTextEdit::selectionChanged, textEdit, [=] {
            emit setEditorCopyAction(textEdit->textCursor().selectedText().isEmpty());
        });
        textEdit->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        textEdit->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        textEdit->setContextMenuPolicy(Qt::CustomContextMenu);

        QPalette p = textEdit->palette();
        p.setColor(QPalette::Inactive, QPalette::Highlight, p.color(QPalette::Active, QPalette::Highlight));
        p.setColor(QPalette::Inactive, QPalette::HighlightedText, p.color(QPalette::Active, QPalette::HighlightedText));
        textEdit->setPalette(p);
        connect(textEdit, &QTextEdit::customContextMenuRequested, this, [=] {
            if (textEdit->textCursor().selectedText().isEmpty()) {
                return;
            }
            UMenu menu;
            GuiUtil::setMenuRoundStyle(&menu);
            QAction* actionCopy = new QAction(tr("Copy"));
            actionCopy->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_C));
            connect(actionCopy, &QAction::triggered, textEdit, [=] {
                QApplication::clipboard()->setText(textEdit->textCursor().selectedText());
            });
            menu.addAction(actionCopy);
            menu.setAnimationPoint(QCursor::pos());
        });
        textEdit->setReadOnly(true);
//        textEdit->setStyleSheet("border:1px solid #E9DAFF");
        ret = textEdit;
    }
    return ret;
}

void AIToolsDelegate::updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    QVariant userData = index.data(Qt::UserRole);
    RspGptStruct modelData = userData.value<RspGptStruct>();
    if (modelData.checkBox) {
        return;
    }
    int fixWidth = calcRectWidth(option, modelData);
    QRect contentRect;
    if (modelData.messageType == AI_TYPE_NAME) {
        int addition = 0;
        if (modelData.finishContent && !modelData.instruction && !modelData.exception) {
            addition = CopyHeight;
        }
        contentRect = QRect(option.rect.x(), option.rect.y() + ContentRectYOffset, fixWidth, option.rect.height() - ContentRectYOffset - RectBlankHeight - Spacing - addition);
    } else {
        if (modelData.type != GptRandomChat) {
            contentRect = QRect(option.rect.x() + option.rect.width() - fixWidth, option.rect.y() + ContentRectYOffset, fixWidth, option.rect.height() - ContentRectYOffset - RectBlankHeight - Spacing);
        } else {
            contentRect = QRect(option.rect.x() + option.rect.width() - fixWidth, option.rect.y(), fixWidth, option.rect.height() - RectBlankHeight - Spacing);
        }
    }
    int extra = 0;
    if (fixWidth < option.rect.width()) {
        extra = 1;
    }
    editor->setGeometry(contentRect.adjusted(Padding - 8, -2 + Padding - 8, -Padding + 8 + extra, -Padding + 5 + 8));
}

void AIToolsDelegate::setEditorData(QWidget *editor, const QModelIndex &index) const
{
    QVariant userData = index.data(Qt::UserRole);
    RspGptStruct modelData = userData.value<RspGptStruct>();
    if(!modelData.content.isEmpty() && !modelData.checkBox)
    {
        QTextEdit *textEdit = dynamic_cast<QTextEdit*>(editor);
        QPalette palette = textEdit->palette();
        palette.setColor(QPalette::Text, Qt::transparent);
        textEdit->setPalette(palette);

        QTextDocument *textDoc = new QTextDocument;
        textDoc->setDefaultFont(getSmartFont(modelData.content));
        textDoc->setPlainText(modelData.content);
        textDoc->setDocumentMargin(8);
        textDoc->setTextWidth(textEdit->width());
        textEdit->setDocument(textDoc);
    }
    else
    {
        QStyledItemDelegate::setEditorData(editor, index);
    }
}
void AIToolsDelegate::hideTip()
{
    if (mTipsWidget) {
        mTipsWidget->close();
        mTipsWidget = nullptr;
    }
}

void AIToolsDelegate::showTip(const QString &tip, const QPoint &pos)
{
    hideTip();
    if (mTipsWidget == nullptr) {
        mTipsWidget = new ToolTipWin(QColor(53,53,53), tip);
        GuiUtil::setWidgetRoundStyle(mTipsWidget);
        mTipsWidget->initView();
        mTipsWidget->setArrowAZ(ToolTipWin::TOPAZ);
    }
    mTipsWidget->setEndPos(pos);
    mTipsWidget->popTips();
}

QString AIToolsDelegate::getTitle(AskGptType type) const
{
    QString title = "";
    switch (type) {
    case GptRandomChat:
        title = tr("Chat");
        break;
    case GptSummarize:
        title = tr("Summarize");
        break;
    case GptTranslate:
        title = tr("Translate");
        break;
    case GptExplain:
        title = tr("Explain");
        break;
    default:
        break;
    }
    return title;
}

ChatPdfDelegate::ChatPdfDelegate(QObject *parent)
    : QStyledItemDelegate(parent)
    , mTipsWidget(nullptr)
    , mPicIndex(0)
    , mInCopyRect(false)
    , mPressCopyRect(false)
    , mInExportRect(false)
    , mPressExportRect(false)
    , mInRegenerateRect(false)
    , mPressRegenerateRect(false)
    , mInAgreeRect(false)
    , mPressAgreeRect(false)
    , mInDisagreeRect(false)
    , mPressDisagreeRect(false)
    , mInCheckBoxRect(false)
    , mPressCheckBoxRect(false)
    , mInRefreshRect(false)
    , mPressRefreshRect(false)
{
}

ChatPdfDelegate::~ChatPdfDelegate()
{
    if (mTipsWidget) {
        mTipsWidget->close();
        mTipsWidget->deleteLater();
        mTipsWidget = nullptr;
    }
}

void ChatPdfDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    painter->setRenderHint(QPainter::Antialiasing);
    QVariant userData = index.data(Qt::UserRole);
    if (userData.canConvert<GptRspPDFStruct>()) {
        GptRspPDFStruct modelData = userData.value<GptRspPDFStruct>();
        if (modelData.fetchData) {
            QPen pen;
            pen.setColor(QColor("#353535"));
            painter->setPen(pen);

            painter->setFont(getSmartFont(modelData.content));
            painter->drawText(option.rect, tr("Loading…"), Qt::AlignHCenter | Qt::AlignVCenter);
        } else if (modelData.waitingAnimation) {
            drawWaiting(painter, option, modelData);
        } else {
            // toolbar
            drawToolBar(painter, option, modelData);
            int fixWidth = calcRectWidth(option, modelData);
            QRect contentRect;
            QBrush brush;
            int leftMargin = 0;
            if (modelData.checkBox) {
                leftMargin = CheckBoxFullWidth;
            }
            if (modelData.messageType == AI_TYPE_NAME) {
                contentRect = QRect(option.rect.x() + leftMargin, option.rect.y() + ContentRectYOffset, fixWidth, option.rect.height() - RectBlankHeight - ContentRectYOffset - Spacing);
                brush = QBrush(Qt::white);
            } else {
                contentRect = QRect(option.rect.x() + option.rect.width() - fixWidth, option.rect.y(), fixWidth, option.rect.height() - RectBlankHeight - Spacing);
                QLinearGradient lineGradient(contentRect.topLeft(), contentRect.topRight());
                lineGradient.setColorAt(0, "#AE72FF");
                lineGradient.setColorAt(1, "#868EFF");
                brush = QBrush(lineGradient);
            }
            if (modelData.checkBox) {
                QString path = modelData.isChecked ? ":/updf-win-icon/ui2/2x/print/checkbox/checkbox-select-1.png" : ":/updf-win-icon/ui2/2x/print/checkbox/checkbox-normal-1.png";
                painter->drawPixmap(QRect(option.rect.x(), contentRect.y() + (contentRect.height() - CheckBoxWidth)/2, CheckBoxWidth + 1, CheckBoxWidth + 1), GuiUtil::generateSmoothPic(path, QSize(CheckBoxWidth, CheckBoxWidth)));
            }
            QPainterPath path;
            path.addRoundedRect(contentRect, 8, 8);
            painter->fillPath(path, brush);
            // showContent
            drawContent(painter, contentRect, option, &modelData);
        }
    }
}

void ChatPdfDelegate::drawToolBar(QPainter *painter, const QStyleOptionViewItem &option, const GptRspPDFStruct &modelData) const
{
    painter->save();
    QRect toolBarRect = QRect(option.rect.x(), option.rect.y(), option.rect.width(), ToolBarHeight);
    QRect textRect;
    QRect iconRect;

    QFont font = FontUtil::getSmartFont(modelData.content);
    font.setPixelSize(ToolBarFontSize);
    painter->setFont(font);
    QPen pen;
    if (modelData.messageType == AI_TYPE_NAME) {
        pen.setColor(QColor("#8A8A8A"));
        painter->setPen(pen);
        textRect = QRect(toolBarRect.x(), toolBarRect.y() - TextYPadding, toolBarRect.width() - IconWidth, ToolBarHeight);
        painter->drawText(textRect, tr("UPDF AI"), Qt::AlignLeft | Qt::AlignVCenter);

        if (!modelData.checkBox) {
            QRect iconRect = QRect(toolBarRect.bottomRight() + QPoint(-24, -ToolBarHeight),QSize(IconWidth, IconWidth));
            QString imagePath = "://updf-win-icon/ui2/2x/chatgpt/copy text.png";
            if(mInCopyRect && option.state.testFlag(QStyle::State_MouseOver)){
                imagePath = "://updf-win-icon/ui2/2x/chatgpt/copy text-hover.png";
            }
            if (mInCopyRect && option.state.testFlag(QStyle::State_MouseOver) && mPressCopyRect) {
                imagePath = "://updf-win-icon/ui2/2x/chatgpt/copy text-pressed.png";
            }
            painter->drawPixmap(iconRect, GuiUtil::generateSmoothPic(imagePath, iconRect.size()));
        }
    }
    painter->restore();
}

void ChatPdfDelegate::drawWaiting(QPainter *painter, const QStyleOptionViewItem &option, const GptRspPDFStruct &modelData) const
{
    painter->save();
    QPen pen;
    pen.setColor(QColor("#353535"));
    painter->setPen(pen);

    QFontMetrics fm(getSmartFont(modelData.content));
    int len = fm.horizontalAdvance(modelData.content);
    int minWidth = 76;
    int width = 56 + 8 + len > minWidth ? 56 + 8 + len : minWidth;
    QRect iconRect;
    QRect contentRect = QRect(option.rect.x(), option.rect.y(), width, option.rect.height() - RectBlankHeight);
    QPainterPath path;
    path.addRoundedRect(contentRect, 8, 8);
    painter->fillPath(path, QBrush(Qt::white));
    if (modelData.content.isEmpty()) {
        iconRect = QRect(option.rect.x() + (contentRect.width() - IconWidth) / 2, option.rect.y() + (option.rect.height() -RectBlankHeight - IconWidth) / 2, IconWidth, IconWidth);
    } else {
        iconRect = QRect(option.rect.x() + 8, option.rect.y() + (option.rect.height() - RectBlankHeight - IconWidth) / 2, IconWidth, IconWidth);
        QRect textRect = QRect(option.rect.x() + 8 + IconWidth + 8, option.rect.y(), width - IconWidth - 8 - 8, option.rect.height() - RectBlankHeight);
        painter->drawText(textRect, modelData.content, Qt::AlignLeft | Qt::AlignVCenter);
    }
    QString imagePath = QString("://updf-win-icon/ui2/2x/chatgpt/chat-loading/icon animation V2_000%1.png").arg(QString::number(mPicIndex).rightJustified(2, '0'));
    painter->drawPixmap(iconRect, GuiUtil::generateSmoothPic(imagePath, iconRect.size()));
    if (++mPicIndex > 49) {
        mPicIndex = 0;
    }
    painter->restore();
}

void ChatPdfDelegate::drawContent(QPainter *painter, const QRect &contentRect, const QStyleOptionViewItem &option, GptRspPDFStruct *modelData) const
{
    painter->save();
    QString content = (*modelData).content;
    QFont font = getSmartFont(content);
    QPen pen;
    if ((*modelData).messageType == AI_TYPE_NAME) {
        pen.setColor("#353535");
    } else {
        pen.setColor(Qt::white);
    }
    QTextDocument textDoc;
    textDoc.setDefaultFont(font);
    QAbstractTextDocumentLayout::PaintContext ctx;
    ctx.palette.setColor(QPalette::Text, pen.color());

    painter->setClipRect(contentRect);
    painter->setClipping(true);
    textDoc.setPlainText(content);
    textDoc.setDocumentMargin(0);
    int textWidth = qMin(contentRect.width() - (2 * Padding) + 2, option.rect.width() - (2 * Padding));
    textDoc.setTextWidth(textWidth);
    painter->translate(contentRect.topLeft() + QPoint(Padding, Padding - 2));
    textDoc.documentLayout()->draw(painter, ctx);
    painter->restore();
    drawPage(painter, contentRect, option, modelData);
    if ((*modelData).messageType == AI_TYPE_NAME && (*modelData).finishContent && !(*modelData).exception) {
        if (!(*modelData).checkBox && !(*modelData).instruction) {
            if (!(*modelData).contentEmpty && !(*modelData).needOcr) {
                QRect exportIconRect = QRect(contentRect.bottomRight() + QPoint(-32 * 4 - 24, -32),QSize(IconWidth, IconWidth));;
                QRect regenerateIconRect = QRect(contentRect.bottomRight() + QPoint(-32 * 3 - 24, -32),QSize(IconWidth, IconWidth));;
                QRect splitIconRect = QRect(contentRect.bottomRight() + QPoint(-32 * 2 - 17, -21 - 5),QSize(1, 10));
                QRect agreeIconRect = QRect(contentRect.bottomRight() + QPoint(-32 * 2, -32),QSize(IconWidth, IconWidth));
                QRect disagreeIconRect = QRect(contentRect.bottomRight() + QPoint(-32, -32),QSize(IconWidth, IconWidth));
                QString exportImagePath = "://updf-win-icon/ui2/2x/chatgpt/export.png";
                if(mInExportRect && option.state.testFlag(QStyle::State_MouseOver)){
                    exportImagePath = "://updf-win-icon/ui2/2x/chatgpt/export-hover.png";
                }
                if (mPressExportRect && option.state.testFlag(QStyle::State_MouseOver)) {
                    exportImagePath = "://updf-win-icon/ui2/2x/chatgpt/export-pressed.png";
                }
                painter->drawPixmap(exportIconRect, GuiUtil::generateSmoothPic(exportImagePath, exportIconRect.size()));

                QString regenerateImagePath = "://updf-win-icon/ui2/2x/chatgpt/regenerate.png";
                if(mInRegenerateRect && option.state.testFlag(QStyle::State_MouseOver)){
                    regenerateImagePath = "://updf-win-icon/ui2/2x/chatgpt/regenerate-hover.png";
                }
                if (mPressRegenerateRect && option.state.testFlag(QStyle::State_MouseOver)) {
                    regenerateImagePath = "://updf-win-icon/ui2/2x/chatgpt/regenerate-pressed.png";
                }
                if ((*modelData).summary) {
                    regenerateImagePath = "://updf-win-icon/ui2/2x/chatgpt/regenerate-disabled.png";
                }
                painter->drawPixmap(regenerateIconRect, GuiUtil::generateSmoothPic(regenerateImagePath, regenerateIconRect.size()));

                QString splitImagePath = "://updf-win-icon/ui2/2x/chatgpt/split.png";
                painter->drawPixmap(splitIconRect, GuiUtil::generateSmoothPic(splitImagePath, splitIconRect.size()));

                QString agreeImagePath = (*modelData).like == LikeAgree ? "://updf-win-icon/ui2/2x/chatgpt/thumb up-pressed.png"
                                                         : "://updf-win-icon/ui2/2x/chatgpt/thumb up.png";
                if(mInAgreeRect && option.state.testFlag(QStyle::State_MouseOver) && (*modelData).like != LikeAgree){
                    agreeImagePath = "://updf-win-icon/ui2/2x/chatgpt/thumb up-hover.png";
                }
                if (mPressAgreeRect && option.state.testFlag(QStyle::State_MouseOver)) {
                    agreeImagePath = "://updf-win-icon/ui2/2x/chatgpt/thumb up-pressed.png";
                }
                if ((*modelData).recordId == 0) {
                    agreeImagePath = "://updf-win-icon/ui2/2x/chatgpt/thumb up-disabled.png";
                }
                painter->drawPixmap(agreeIconRect, GuiUtil::generateSmoothPic(agreeImagePath, agreeIconRect.size()));

                QString disagreeImagePath = (*modelData).like == LikeDisagree ? "://updf-win-icon/ui2/2x/chatgpt/thumb down-pressed.png"
                                                         : "://updf-win-icon/ui2/2x/chatgpt/thumb down.png";
                if(mInDisagreeRect && option.state.testFlag(QStyle::State_MouseOver) && (*modelData).like != LikeDisagree){
                    disagreeImagePath = "://updf-win-icon/ui2/2x/chatgpt/thumb down-hover.png";
                }
                if (mPressDisagreeRect && option.state.testFlag(QStyle::State_MouseOver)) {
                    disagreeImagePath = "://updf-win-icon/ui2/2x/chatgpt/thumb down-pressed.png";
                }
                if ((*modelData).recordId == 0) {
                    disagreeImagePath = "://updf-win-icon/ui2/2x/chatgpt/thumb down-disabled.png";
                }
                painter->drawPixmap(disagreeIconRect, GuiUtil::generateSmoothPic(disagreeImagePath, disagreeIconRect.size()));
            }
            if ((*modelData).contentEmpty || (*modelData).needOcr) {
                QString tip = tr("Retry");
                if ((*modelData).needOcr) {
                    tip = tr("OCR");
                }
                QFontMetrics fm(getSmartFont(tip));
                int len = fm.horizontalAdvance(tip) + 4;

                QRect iconRect = QRect(contentRect.bottomRight() + QPoint(-36 - len, -32), QSize(IconWidth, IconWidth));
                QRect tipRect = QRect(contentRect.bottomRight() + QPoint(-12 - len, -32), QSize(len, 24));
                QRect refreshRect = QRect(contentRect.bottomRight() + QPoint(-36 - len, -32), QSize(IconWidth + len, 24));
                QPainterPath path;
                path.addRoundedRect(refreshRect, 4, 4);
                QString imagePath = "://updf-win-icon/ui2/2x/chatgpt/refresh.png";
                if ((*modelData).needOcr) {
                    imagePath = ":/updf-win-icon/ui2/2x/others/24icon/ocr-normal.png";
                }
                if(option.state.testFlag(QStyle::State_MouseOver)){
                    if (mInRefreshRect) {
                        imagePath = "://updf-win-icon/ui2/2x/chatgpt/refresh.png";
                        if ((*modelData).needOcr) {
                            imagePath = ":/updf-win-icon/ui2/2x/others/24icon/ocr-normal.png";
                        }
                        painter->fillPath(path, QColor("#F7F7F7"));
                    } else {
                        pen.setColor("#ECECEC");
                        pen.setWidth(1);
                        painter->setPen(pen);
                        painter->drawPath(path);
                    }
                    if (mPressRefreshRect) {
                        imagePath = "://updf-win-icon/ui2/2x/chatgpt/refresh-hover.png";
                        if ((*modelData).needOcr) {
                            imagePath = ":/updf-win-icon/ui2/2x/others/24icon/ocr-pressed.png";
                        }
                        painter->fillPath(path, QColor("#E9DAFF"));
                    }
                } else {
                    pen.setColor("#ECECEC");
                    pen.setWidth(1);
                    painter->setPen(pen);
                    painter->drawPath(path);
                }
                if (mPressRefreshRect && option.state.testFlag(QStyle::State_MouseOver)) {
                    pen.setColor("#964FF2");
                } else {
                    pen.setColor("#353535");
                }
                painter->setPen(pen);
                painter->drawText(tipRect.adjusted(0, 4, 0, -3), tip);
                painter->drawPixmap(iconRect, GuiUtil::generateSmoothPic(imagePath, iconRect.size()));
             }
        }
    }
}

void ChatPdfDelegate::drawPage(QPainter *painter, const QRect &contentRect, const QStyleOptionViewItem &option, GptRspPDFStruct *modelData) const
{
    painter->save();
    if ((*modelData).messageType == AI_TYPE_NAME && (*modelData).finishContent && !(*modelData).checkBox
            && !(*modelData).needOcr && !(*modelData).contentEmpty && !(*modelData).exception) {
        calPagePos(option, modelData);
        QPen pen;
        pen.setColor(QColor("#964FF2"));
        painter->setPen(pen);
        QFont font = getSmartFont((*modelData).content);
        painter->setFont(font);
        QList<int> pages = (*modelData).metaDataMap.keys();
        for (int i = 0; i < pages.size(); ++i) {
            RefMetaData meta = (*modelData).metaDataMap.value(pages.at(i));
            QPainterPath rectPath;
            rectPath.addRoundedRect(meta.pos, 4, 4);
            painter->fillPath(rectPath, QColor("#F6F0FF"));
            painter->drawText(meta.pos.adjusted(0, -1, 0, -1), Qt::AlignCenter, QString::number(meta.page));
        }
    }
    painter->restore();
}

void ChatPdfDelegate::calPagePos(const QStyleOptionViewItem &option, GptRspPDFStruct *modelData) const
{
    int contentWidth = option.rect.width() - (2 * Padding);
    QFontMetrics fm(getSmartFont((*modelData).content));
    int pos = Padding;
    int pageY = option.rect.y() + option.rect.height() - RectBlankHeight - calPageItemHeight(contentWidth, *modelData) - Padding - CopyHeight + Spacing;
    QList<RefMetaData> metaDatas = (*modelData).metaDataMap.values();
    for (RefMetaData meta : metaDatas) {
        int pageWidth = qMax(24, fm.horizontalAdvance(QString::number(meta.page)));
        meta.pos = QRect(pos, pageY, pageWidth, PageLineHeight);
        (*modelData).metaDataMap.insert(meta.page, meta);
        pos += pageWidth;
        pos += PageGap;
        if (pos + pageWidth > Padding + contentWidth) {
            pos = Padding;
            pageY += PageLineGap;
            pageY += PageLineHeight;
        }
    }
}

QSize ChatPdfDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    QVariant userData = index.data(Qt::UserRole);
    if (userData.canConvert<GptRspPDFStruct>()) {
        GptRspPDFStruct modelData = userData.value<GptRspPDFStruct>();
        if (modelData.fetchData) {
            return QSize(option.rect.width(), 36);
        } else if (modelData.waitingAnimation) {
            return QSize(option.rect.width(), 36 + RectBlankHeight);
        }

        int height = 0;
        int contentWidth = option.rect.width() - (2 * Padding);
        if (modelData.checkBox) {
            contentWidth -= CheckBoxFullWidth;
        }
        QTextDocument textDoc;
        textDoc.setDefaultFont(getSmartFont(modelData.content));
        textDoc.setPlainText(modelData.content);
        textDoc.setDocumentMargin(0);
        textDoc.setTextWidth(contentWidth);
        height += textDoc.size().height();

        height += 2 * Padding;
        if (modelData.messageType == AI_TYPE_NAME) {
            height += ToolBarHeight;
        }
        if (modelData.messageType == AI_TYPE_NAME && !modelData.checkBox) {
            if (modelData.finishContent) {
                int pageItemH = calPageItemHeight(contentWidth, modelData);
                height += pageItemH;
                if (!modelData.checkBox && !modelData.instruction && !modelData.exception) {
                    height += CopyHeight;
                }
            }
        }
        height += RectBlankHeight; // spacing
        return QSize(option.rect.width(), height);
    }
    return QStyledItemDelegate::sizeHint(option,index);
}

bool ChatPdfDelegate::editorEvent(QEvent *event, QAbstractItemModel *model, const QStyleOptionViewItem &option, const QModelIndex &index)
{
    Q_UNUSED(model)
    QVariant userData = index.data(Qt::UserRole);
    if (!userData.canConvert<GptRspPDFStruct>()) {
        return false;
    }
    GptRspPDFStruct modelData = userData.value<GptRspPDFStruct>();
    if (modelData.waitingAnimation) {
        return false;
    }
    int fixWidth = calcRectWidth(option, modelData);
    QRect copyIconRect;
    QRect exportIconRect;
    QRect regenerateIconRect;
    QRect agreeIconRect;
    QRect disagreeIconRect;
    QRect contentRect;
    QRect checkBoxRect;
    QRect refreshRect;

    int leftMargin = 0;
    if (modelData.checkBox) {
        leftMargin = CheckBoxFullWidth;
    }
    if (modelData.messageType == AI_TYPE_NAME) {
        contentRect = QRect(option.rect.x() + leftMargin, option.rect.y() + ContentRectYOffset, fixWidth, option.rect.height() - RectBlankHeight - ContentRectYOffset - Spacing);
    } else {
        contentRect = QRect(option.rect.x() + option.rect.width() - fixWidth, option.rect.y(), fixWidth, option.rect.height() - RectBlankHeight - Spacing);
    }
    if (modelData.messageType == AI_TYPE_NAME && !modelData.checkBox) {
        QRect toolBarRect = QRect(option.rect.x(), option.rect.y(), option.rect.width(), ToolBarHeight);
        copyIconRect = QRect(toolBarRect.bottomRight() + QPoint(-24, -ToolBarHeight),QSize(IconWidth, IconWidth));

        if (!modelData.exception) {
            if (modelData.contentEmpty || modelData.needOcr) {
                QString tip = tr("Retry");
                if (modelData.needOcr) {
                    tip = tr("OCR");
                }
                QFontMetrics fm(getSmartFont(tip));
                int len = fm.horizontalAdvance(tip) + 4;
                refreshRect = QRect(contentRect.bottomRight() + QPoint(-36 - len, -32), QSize(IconWidth + len, 24));
            } else {
                if (!modelData.instruction) {
                    exportIconRect = QRect(contentRect.bottomRight() + QPoint(-32 * 4 - 24, -32),QSize(IconWidth, IconWidth));;
                    regenerateIconRect = QRect(contentRect.bottomRight() + QPoint(-32 * 3 - 24, -32),QSize(IconWidth, IconWidth));;
                    agreeIconRect = QRect(contentRect.bottomRight() + QPoint(-32 * 2, -32),QSize(IconWidth, IconWidth));
                    disagreeIconRect = QRect(contentRect.bottomRight() + QPoint(-32, -32),QSize(IconWidth, IconWidth));
                }
            }
        }
    }
    QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
    if (!modelData.checkBox) {
        if (modelData.finishContent) {
            calPagePos(option, &modelData);
        }
    } else {
        checkBoxRect = QRect(option.rect.x(), contentRect.y() + (contentRect.height() - CheckBoxWidth)/2, CheckBoxWidth + 1, CheckBoxWidth + 1);
    }
    if (event->type() == QEvent::MouseButtonPress) {
        if (mouseEvent->button() == Qt::LeftButton)
        {
            if (!modelData.checkBox) {
                if (copyIconRect.contains(mouseEvent->pos())) {
                    QPoint point(option.widget->mapToGlobal(QPoint(copyIconRect.center())));
                    point += QPoint(0, 12);
                    showTip(tr("Copied!"), point);
                    QApplication::clipboard()->setText(modelData.content);
                    mPressCopyRect = true;
                    emit refreshItem(index.row());
                }
            } else {
                if (checkBoxRect.contains(mouseEvent->pos())) {
                    mPressCheckBoxRect = true;
                }
            }

            if (modelData.finishContent) {
                if (!modelData.checkBox) {
                    if (modelData.contentEmpty || modelData.needOcr) {
                        if (refreshRect.contains(mouseEvent->pos())) {
                            mPressRefreshRect = true;
                        }
                    } else {
                        if (!modelData.instruction) {
                            // page
                            QList<int> pages = modelData.metaDataMap.keys();
                            for (int i = 0; i < pages.size(); ++i) {
                                RefMetaData meta = modelData.metaDataMap.value(pages.at(i));
                                if (meta.pos.contains(mouseEvent->pos() + QPoint(-2, -6))){
                                    emit clickPageItem(meta);
                                    break;
                                }
                            }
                            // end page
                            if (exportIconRect.contains(mouseEvent->pos())) {
                                mPressExportRect = true;
                            } else if (regenerateIconRect.contains(mouseEvent->pos())) {
                                if (modelData.summary != 1) {
                                    mPressRegenerateRect = true;
                                }
                            } else if (agreeIconRect.contains(mouseEvent->pos())) {
                                if (modelData.recordId != 0) {
                                    mPressAgreeRect = true;
                                }
                            } else if (disagreeIconRect.contains(mouseEvent->pos())) {
                                if (modelData.recordId != 0) {
                                    mPressDisagreeRect = true;
                                }
                            }
                        }
                    }
                }
            }
        }
    } else if (event->type() == QEvent::MouseButtonRelease) {
        if (!modelData.checkBox) {
            if (mPressCopyRect) {
                mPressCopyRect = false;
                emit refreshItem(index.row());
            }
        } else {
            if (mPressCheckBoxRect) {
               mPressCheckBoxRect = false;
               emit checkItem(index.row());
            }
        }

        if (modelData.finishContent) {
            if (!modelData.checkBox) {
                if (modelData.contentEmpty || modelData.needOcr) {
                    if (mPressRefreshRect) {
                        mPressRefreshRect = false;
                        if (modelData.contentEmpty) {
                            emit retryTrigger(index.row());
                        } else if (modelData.needOcr) {
                            emit ocrTrigger();
                        }
                    }
                } else {
                    if (!modelData.instruction) {
                        if (mPressExportRect) {
                            mPressExportRect = false;
                            emit exportTrigger(index.row());
                            hideTip();
                        } else if (mPressRegenerateRect) {
                            mPressRegenerateRect = false;
                            emit regenerateTrigger(index.row());
                            emit refreshItem(index.row());
                            hideTip();
                        } else if (mPressAgreeRect) {
                            mPressAgreeRect = false;
                            int oldLike = modelData.like;
                            modelData.like = modelData.like == LikeAgree ? LikeNull : LikeAgree;
                            emit likeItem(index.row(), modelData, oldLike);
                            emit updateItem(index.row(), modelData);
                            if (modelData.like != LikeNull) {
                                QRect innerRect = agreeIconRect.adjusted(-2, -4, 6, 4);
                                showAnimation(QRect(QPoint(option.widget->mapToGlobal(innerRect.topLeft())), QPoint(option.widget->mapToGlobal(innerRect.bottomRight()))),  "://updf-win-icon/ui2/2x/chatgpt/like.png", const_cast<QWidget*>(option.widget));
                            }
                        } else if (mPressDisagreeRect) {
                            mPressDisagreeRect = false;
                            int oldLike = modelData.like;
                            modelData.like = modelData.like == LikeDisagree ? LikeNull : LikeDisagree;
                            emit likeItem(index.row(), modelData, oldLike);
                            emit updateItem(index.row(), modelData);
                            if (modelData.like != LikeNull) {
                                QRect innerRect = disagreeIconRect.adjusted(0, -4, 8, 4);
                                showAnimation(QRect(QPoint(option.widget->mapToGlobal(innerRect.topLeft())), QPoint(option.widget->mapToGlobal(innerRect.bottomRight()))), "://updf-win-icon/ui2/2x/chatgpt/dislike.png", const_cast<QWidget*>(option.widget));
                            }
                        }
                    }
                }
            }
        }
    } else if (event->type() == QEvent::MouseMove) {
        auto listView = dynamic_cast<QListView*>(parent());
        bool inPage = false;
        if(listView && !modelData.checkBox && modelData.finishContent){
            // page
            QList<int> pages = modelData.metaDataMap.keys();
            for (int i = 0; i < pages.size(); ++i) {
                RefMetaData meta = modelData.metaDataMap.value(pages.at(i));
                if (meta.pos.contains(mouseEvent->pos() + QPoint(-2, -6))){
                    inPage = true;
                    break;
                }
            }
            // end page
            auto cursor = inPage?CursorUtil::getCursor(CS_PointingHand):Qt::ArrowCursor;
            listView->setCursor(cursor);
        }
        if (contentRect.contains(mouseEvent->pos()) && !modelData.checkBox && !inPage && !exportIconRect.contains(mouseEvent->pos())
                && !regenerateIconRect.contains(mouseEvent->pos()) && !agreeIconRect.contains(mouseEvent->pos())
                && !disagreeIconRect.contains(mouseEvent->pos()) && !copyIconRect.contains(mouseEvent->pos())) {
            // edit mode
            emit triggerEditor(mouseEvent->pos());
        }
        if(modelData.messageType == AI_TYPE_NAME) {
            if (!modelData.checkBox) {
                if (copyIconRect.contains(mouseEvent->pos())) {
                    mInCopyRect = true;
                    if (mTipsWidget && (mTipsWidget->tips() == tr("Copied!") || mTipsWidget->tips() == tr("Copy"))) {
                    } else {
                        QPoint point(option.widget->mapToGlobal(QPoint(copyIconRect.center())));
                        point += QPoint(0, 12);
                        showTip(tr("Copy"), point);
                    }
                } else {
                    mInCopyRect = false;
                    mPressCopyRect = false;
                }
            } else {
                if (checkBoxRect.contains(mouseEvent->pos())) {
                    mInCheckBoxRect = true;
                } else {
                    mInCheckBoxRect = false;
                    mPressCheckBoxRect = false;
                }
            }

            if (modelData.finishContent) {
                if (!modelData.checkBox) {
                    if (modelData.contentEmpty || modelData.needOcr) {
                        if (refreshRect.contains(mouseEvent->pos())) {
                            mInRefreshRect = true;
                        } else {
                            mInRefreshRect = false;
                            mPressRefreshRect = false;
                        }
                    } else {
                        if (!modelData.instruction) {
                            if (exportIconRect.contains(mouseEvent->pos())) {
                                mInExportRect = true;
                                if (mTipsWidget && mTipsWidget->tips() == tr("Export")) {
                                } else {
                                    QPoint point(option.widget->mapToGlobal(QPoint(exportIconRect.center())));
                                    point += QPoint(0, 12);
                                    showTip(tr("Export"), point);
                                }
                            } else {
                                mInExportRect = false;
                                mPressExportRect = false;
                            }
                            if (regenerateIconRect.contains(mouseEvent->pos())) {
                                mInRegenerateRect = true;
                                if (mTipsWidget && mTipsWidget->tips() == tr("Regenerate")) {
                                } else {
                                    QPoint point(option.widget->mapToGlobal(QPoint(regenerateIconRect.center())));
                                    point += QPoint(0, 12);
                                    showTip(tr("Regenerate"), point);
                                }
                            } else {
                                mInRegenerateRect = false;
                                mPressRegenerateRect = false;
                            }
                            if (agreeIconRect.contains(mouseEvent->pos())) {
                                mInAgreeRect = true;
                            } else {
                                mInAgreeRect = false;
                                mPressAgreeRect = false;
                            }
                            if (disagreeIconRect.contains(mouseEvent->pos())) {
                                mInDisagreeRect = true;
                            } else {
                                mInDisagreeRect = false;
                                mPressDisagreeRect = false;
                            }
                        }
                    }
                    if (!mInCopyRect && !mInExportRect && !mInRegenerateRect/* && !mInAgreeRect && !mInDisagreeRect*/) {
                        hideTip();
                    }
                }
            }
        }
        emit refreshItem(index.row());
    }
    return QStyledItemDelegate::editorEvent(event, model, option, index);
}

QWidget* ChatPdfDelegate::createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    QWidget* ret = nullptr;
    QVariant userData = index.data(Qt::UserRole);
    GptRspPDFStruct modelData = userData.value<GptRspPDFStruct>();
    if (!modelData.content.isEmpty() && !modelData.checkBox)
    {
        QTextEdit *textEdit = new CopyTextEdit(parent);
        connect(textEdit, &QTextEdit::selectionChanged, textEdit, [=] {
            emit setEditorCopyAction(textEdit->textCursor().selectedText().isEmpty());
        });
        textEdit->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        textEdit->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        textEdit->setContextMenuPolicy(Qt::CustomContextMenu);

        QPalette p = textEdit->palette();
        p.setColor(QPalette::Inactive, QPalette::Highlight, p.color(QPalette::Active, QPalette::Highlight));
        p.setColor(QPalette::Inactive, QPalette::HighlightedText, p.color(QPalette::Active, QPalette::HighlightedText));
        textEdit->setPalette(p);
        connect(textEdit, &QTextEdit::customContextMenuRequested, this, [=] {
            if (textEdit->textCursor().selectedText().isEmpty()) {
                return;
            }
            UMenu menu;
            GuiUtil::setMenuRoundStyle(&menu);
            QAction* actionCopy = new QAction(tr("Copy"));
            actionCopy->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_C));
            connect(actionCopy, &QAction::triggered, textEdit, [=] {
                QApplication::clipboard()->setText(textEdit->textCursor().selectedText());
            });
            menu.addAction(actionCopy);
            menu.setAnimationPoint(QCursor::pos());
        });
        textEdit->setReadOnly(true);
//        textEdit->setStyleSheet("border:1px solid #E9DAFF;");
        ret = textEdit;
    }
    return ret;
}

void ChatPdfDelegate::updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    QVariant userData = index.data(Qt::UserRole);
    GptRspPDFStruct modelData = userData.value<GptRspPDFStruct>();
    if (modelData.checkBox) {
        return;
    }
    int fixWidth = calcRectWidth(option, modelData);
    int contentWidth = option.rect.width() - (2 * Padding);
    QRect contentRect;
    if (modelData.messageType == AI_TYPE_NAME) {
        int pageItemH = 0;
        int addition = 0;
        if (modelData.finishContent) {
            pageItemH = calPageItemHeight(contentWidth, modelData);
            if (!modelData.instruction && !modelData.exception) {
                addition = CopyHeight;
            }
        }
        contentRect = QRect(option.rect.x(), option.rect.y() + ContentRectYOffset, fixWidth, option.rect.height() - ContentRectYOffset - RectBlankHeight - Spacing - pageItemH - addition);
    } else {
        contentRect = QRect(option.rect.x() + option.rect.width() - fixWidth, option.rect.y(), fixWidth, option.rect.height() - RectBlankHeight - Spacing);
    }
    int extra = 0;
    if (fixWidth < option.rect.width()) {
        extra = 1;
    }
    editor->setGeometry(contentRect.adjusted(Padding - 8, -2 + Padding - 8, -Padding + 8 + extra, -Padding + 5 + 8));
}

void ChatPdfDelegate::setEditorData(QWidget *editor, const QModelIndex &index) const
{
    QVariant userData = index.data(Qt::UserRole);
    GptRspPDFStruct modelData = userData.value<GptRspPDFStruct>();
    if(!modelData.content.isEmpty() && !modelData.checkBox)
    {
        QTextEdit *textEdit = dynamic_cast<QTextEdit*>(editor);
        QPalette palette = textEdit->palette();
        palette.setColor(QPalette::Text, Qt::transparent);
        textEdit->setPalette(palette);

        QTextDocument *textDoc = new QTextDocument;
        textDoc->setDefaultFont(getSmartFont(modelData.content));
        textDoc->setPlainText(modelData.content);
        textDoc->setDocumentMargin(8);
        textDoc->setTextWidth(textEdit->width());
        textEdit->setDocument(textDoc);
    }
    else
    {
        QStyledItemDelegate::setEditorData(editor, index);
    }
}
void ChatPdfDelegate::hideTip()
{
    if (mTipsWidget) {
        mTipsWidget->close();
        mTipsWidget = nullptr;
    }
}

void ChatPdfDelegate::showTip(const QString &tip, const QPoint &pos)
{
    hideTip();
    if (mTipsWidget == nullptr) {
        mTipsWidget = new ToolTipWin(QColor(53,53,53), tip);
        GuiUtil::setWidgetRoundStyle(mTipsWidget);
        mTipsWidget->initView();
        mTipsWidget->setArrowAZ(ToolTipWin::TOPAZ);
    }
    mTipsWidget->setEndPos(pos);
    mTipsWidget->popTips();
}

int ChatPdfDelegate::calPageItemHeight(int contentWidth, const GptRspPDFStruct &modelData) const
{
    int height = 0;
    QFontMetrics fm(getSmartFont(modelData.content));
    int pageStrLen = 0;
    int pageLine = 0;
    for (RefMetaData meta : modelData.metaDataMap.values()) {
        pageStrLen += qMax(24, fm.horizontalAdvance(QString::number(meta.page)));
        pageStrLen += PageGap;
    }
    if (pageStrLen > 0) {
        pageLine = pageStrLen / contentWidth;
        pageLine += pageStrLen % contentWidth == 0 ? 0 : 1;
        height += pageLine * PageLineHeight;
        height += pageLine * PageLineGap;
    }
    return height;
}
