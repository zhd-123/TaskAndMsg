#include "chatgpttypedefine.h"
#include <QFontMetrics>
#include <QPainter>
#include <QLinearGradient>
#include <QFileInfo>
#include <QSettings>
#include "./src/util/configutil.h"
#include "./src/util/guiutil.h"
#include "../pageengine/pdfcore/PDFDocument.h"
#include "./src/toolsview/basemodelview/basefiledefine.h"
#include "./src/usercenter/limituse/limituse_enterprise/enterpriselimituse.h"

ChatgptTypeDefine::ChatgptTypeDefine(QObject *parent)
    : QObject{parent}
{

}

QList<QPair<QString,QString>> ChatgptTypeDefine::getLangList(bool useTranslate)
{
    if (useTranslate) {
        return {
                {tr("cn_English"),"en"},
                {tr("cn_German"),"de"},
                {tr("cn_French"),"fr"},
                {tr("cn_Italian"),"it"},
                {tr("cn_Japanese"),"ja"},
                {tr("cn_Portuguese"),"pt"},
                {tr("cn_Spanish"),"es"},
                {tr("cn_Russian"),"ru"},
                {tr("cn_Dutch"),"nl"},
                {tr("cn_Korean"),"ko"},
                {tr("cn_Chinese Simplified"),"zh-CN"},
                {tr("cn_Chinese Traditional"),"zh-HK"},
               };
    } else {
        bool chineseChannel = ChannelUtil::isChineseChannel(ChannelUtil::localChannelId());
        if (chineseChannel) {
            return {
                {tr("lang_Chinese Simplified"),"zh-CN"},
                {tr("lang_English"),"en"},
            };
        } else {
            return {
                {tr("lang_Arabic (Saudi Arabia)"),"ar-sa"},
                {tr("lang_Belarusian"),"be"},
                {tr("lang_Chinese Simplified"),"zh-CN"},
                {tr("lang_Chinese Traditional"),"zh-HK"},
                {tr("lang_Croatian"),"hr"},
                {tr("lang_Czech"),"cs"},
                {tr("lang_Danish"),"da"},
                {tr("lang_Dutch"),"nl"},
                {tr("lang_English"),"en"},
                {tr("lang_Finnish"),"fi"},
                {tr("lang_French"),"fr"},
                {tr("lang_German"),"de"},
                {tr("lang_Hebrew"),"he"},
    //            {tr("lang_Irish"),"ga"},
                {tr("lang_Italian"),"it"},
                {tr("lang_Japanese"),"ja"},
                {tr("lang_Korean"),"ko"},
                {tr("lang_Norwegian"),"no"},
                {tr("lang_Polish"),"pl"},
                {tr("lang_Portuguese"),"pt"},
                {tr("lang_Portuguese (Brazilian)"),"pt-br"},
                {tr("lang_Russian"),"ru"},
                {tr("lang_Spanish"),"es"},
                {tr("lang_Swedish"),"sv"},
                {tr("lang_Thai"),"th"},
                {tr("lang_Turkish"),"tr"},
                {tr("lang_Ukrainian"),"uk"},
                {tr("lang_Vietnamese"),"vi"},
            };
        }
    }
}

QString ChatgptTypeDefine::showLangText(const QString &lang)
{
    QString langString = "";
    if (lang == "en") {
        langString = tr("cn_English");
    } else if (lang == "de") {
        langString = tr("cn_German");
    } else if (lang == "fr") {
        langString = tr("cn_French");
    } else if (lang == "it") {
        langString = tr("cn_Italian");
    } else if (lang == "nl") {
        langString = tr("cn_Dutch");
    } else if (lang == "pt") {
        langString = tr("cn_Portuguese");
    } else if (lang == "es") {
        langString = tr("cn_Spanish");
    } else if (lang == "ja") {
        langString = tr("cn_Japanese");
    } else if (lang == "ru") {
        langString = tr("cn_Russian");
    } else if (lang == "ko") {
        langString = tr("cn_Korean");
    }else if (lang == "zh-CN") {
        langString = tr("cn_Chinese Simplified");
    } else if (lang == "zh-HK") {
        langString = tr("cn_Chinese Traditional");
    }
    return langString;
}

void ChatgptTypeDefine::getFileInfo(const QString &filePath, QString *fileName, QString *fileSize, int *pageCount)
{
    *fileName = GuiUtil::getFileName(filePath, false);
    *fileSize = BaseFileDefine::formatShowStorage(QFileInfo(filePath).size(), 1);

    PDF_DOCUMENT pdfDoc = nullptr;
    int openRetCode = PDFDocument::openDocument(filePath.toStdWString(), "", &pdfDoc);
    if (pdfDoc) {
        *pageCount = PDFDocument::pageCount(pdfDoc);
        PDFDocument::closeDocument(pdfDoc);
    }
}

bool ChatgptTypeDefine::mTranslationAuth = false;
void ChatgptTypeDefine::setTranslationAuth(bool auth)
{
    mTranslationAuth = auth;
}

bool ChatgptTypeDefine::hasTranslationAuth()
{
    return mTranslationAuth;
}

QString ChatgptTypeDefine::aiServerHost()
{
    if (UserCenterManager::instance()->userContext().mUserInfo.companyUser) {
        return RequestResource::instance()->enterpriseAiServerHost();
    } else {
        return RequestResource::instance()->aiServerHost();
    }
}

ColorGradientLabel::ColorGradientLabel(QWidget *parent, Qt::WindowFlags f)
    : QLabel(parent, f)
    , mPaddingLeft(0)
    , mBgColor("#FFFFFF")
{

}

void ColorGradientLabel::setPaddingLeft(int padding)
{
    mPaddingLeft = padding;
}

void ColorGradientLabel::setBGColor(const QColor &color)
{
    mBgColor = color;
}

void ColorGradientLabel::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    painter.fillRect(rect(), QBrush(mBgColor));
    QString content = text();

    QFont font;
    font.setPixelSize(12);
    font.setWeight(QFont::DemiBold);
    QFontMetrics fm(font);
    QRect rt = fm.boundingRect(rect().adjusted(mPaddingLeft, 0, 0, 0), Qt::AlignLeft | Qt::TextSingleLine, content);
    QLinearGradient linear(mPaddingLeft, 0, rt.width(), 0);
    linear.setColorAt(0, QColor("#9E53FF"));
    linear.setColorAt(1, QColor("#428EFF"));

    painter.setPen(QPen(linear, 0));
    painter.setFont(font);
    painter.drawText(rect().adjusted(mPaddingLeft, 0, 0, 0), Qt::AlignVCenter, content);
}

QString loadLangSetting()
{
    QString filePath = ConfigUtil::UPDFFilePath();
    QSettings settings(filePath, QSettings::IniFormat);
    return settings.value("Aimenu/langCode").toString();
}

void saveLangSetting(const QString &langCode)
{
    QString filePath = ConfigUtil::UPDFFilePath();
    QSettings settings(filePath, QSettings::IniFormat);
    settings.setValue("Aimenu/langCode", langCode);
    settings.sync();
}

DiffColorTextButton::DiffColorTextButton(QWidget *parent)
    : QPushButton(parent)
{

}

void DiffColorTextButton::paintEvent(QPaintEvent *event)
{
    QString content = text();
    setText("");
    QPushButton::paintEvent(event);

    QPainter painter(this);
    QString leftContent, rightContent;
    if (content.contains("(")) {
        int index = content.lastIndexOf("(");
        leftContent = content.left(index - 1);
        rightContent = content.mid(index);
        painter.setPen(QPen(QColor(255,255,255),1,Qt::SolidLine));
        QFontMetrics fm(font());
        QRect rect = fm.boundingRect(this->rect(), Qt::AlignCenter, content);
        painter.setPen(QPen(QColor(255,255,255),1,Qt::SolidLine));
        painter.drawText(rect, Qt::AlignLeft | Qt::AlignVCenter, leftContent);

        painter.setPen(QPen(QColor(255,255,0, 150),1,Qt::SolidLine));
        painter.drawText(rect, Qt::AlignRight | Qt::AlignVCenter, rightContent);
    } else {
        painter.setPen(QPen(QColor(255,255,255),1,Qt::SolidLine));
        painter.drawText(rect(), Qt::AlignCenter, content);
    }
    setText(content);
}
