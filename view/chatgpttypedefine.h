#ifndef CHATGPTTYPEDEFINE_H
#define CHATGPTTYPEDEFINE_H

#include <QObject>
#include <QLabel>
#include <QDateTime>
#include <QMap>
#include <QTextEdit>
#include <QKeyEvent>
#include <QApplication>
#include <QClipboard>
#include <QPushButton>
#include "pdfcore/headers/spdf_edit.h"

#define MinGPTWidgetWidth 166
#define ChatItemSpacing 0
#define MaxParseTime 300

#define GPT_Work_Dir "gpt"
#define GPT_AI_ChatId_Config_File "aiChatIdFileConfig.ini"
#define GPT_PDF_ChatId_Config_File "pdfChatIdFileConfig.ini"
#define AI_TYPE_NAME "assistant"
#define CHAT_PDF_TYPE_NAME "file_talk"
#define SUMMARY_PDF_TYPE_NAME "file_page_summary"
#define TRANSLATE_PDF_TYPE_NAME "file_page_translate"

typedef std::function<void()> CommitRspCallback;

enum ExceedEnum {
    TooManyFileMission = 7003, // 正在解析文件的数量超限制
    TalkNumExceed      = 7004, // 对话超限制
    FilePageNumExceed  = 7005, // 文件页数超限制
    FileNumExceed      = 7006, // 文件数量超限制
    FileSizeExceed     = 8006, // 文件大小超限制
    TranslateLenExceed = 7010,
    TranslateFail      = 7011,
    TranslateNoPower = 7014,
    TranslatePointsExceed = 7015,
    TranslateDownloadFail = 7016,
    TranslateCharExceed1 = 7012,
    TranslateCharExceed = 7013,
    TranslatePointsExceed1 = 7019,
    TranslatePointsExceed2 = 7020,
};

enum AskGptType {
    GptRandomChat = 0,
    GptSummarize,
    GptTranslate,
    GptExplain,
    GptFileTalk,
    GptFilePageSummary,
    GptFilePageTranslate,
};
enum ExportMsgType {
    ExportMarkdown = 0,
    ExportText,
};

enum AnalysisStatus {
    UnAnalysis = 0,
    Analysising = 1,
    Finished,
};

enum GptFlowStage {
    GptBegin = 0,
    GptFlow,
    GptEnd,
};
enum LikeEnum {
    LikeNull = 0,
    LikeAgree,
    LikeDisagree,
};
struct RefMetaData {
    int page;
    QString text;
    QList<PDF_QUADPOINTSF> rectList;
    QRect pos;
    RefMetaData() {
        page = 0;
    }
};
using RefMetaDataMap = QMap<int, RefMetaData>;

struct AskGptStruct {
    qint64 fileId;
    qint64 chatId;
    qint64 singleChatId;
    qint64 lastSingleChatId;
    QString content;
    int summary; //  1--摘要 0--非摘要
    AskGptType type;
    QString targetLanguage; // type=translate时，需要此字段 eg:fr
    int retry;
    AskGptStruct() {
        fileId = 0;
        chatId = 0;
        singleChatId = 0;
        lastSingleChatId = 0;
        summary = 0;
        retry = 0;
        type = GptRandomChat;
    }
};
Q_DECLARE_METATYPE(AskGptStruct)

struct RspGptStruct {
    qint64 chatId;
    qint64 singleChatId;
    QString content;
    QDateTime createTime;
    QString messageType;
    GptFlowStage stage;
    bool waitingAnimation;
    bool fetchData;
    AskGptType type;
    qint64 recordId; // 翻页
    bool finishContent;
    LikeEnum like;
    QString targetLanguage;
    int contentEmpty; //1 can retry
    bool checkBox;
    bool isChecked;
    bool instruction;
    bool exception;
    RspGptStruct() {
        chatId = 0;
        singleChatId = 0;
        stage = GptBegin;
        waitingAnimation = false;
        fetchData = false;
        recordId = 0;
        type = GptRandomChat;
        finishContent = false;
        like = LikeNull;
        contentEmpty = 0;
        checkBox = false;
        isChecked = false;
        instruction = false;
        exception = false;
    }
};
Q_DECLARE_METATYPE(RspGptStruct)

struct LikeGptStruct {
    int index;
    RspGptStruct rspGptStruct;
    LikeEnum oldLike;
    LikeGptStruct() {
        index = 0;
        oldLike = LikeNull;
    }
};
Q_DECLARE_METATYPE(LikeGptStruct)

struct ReqPDFGptStruct {
    qint64 fileId;
    qint64 chatId;
    qint64 singleChatId;
    qint64 lastSingleChatId;
    qint64 parseId; // 多次解析对应
    QString content;
    int summary; //  1--摘要 0--非摘要
    int retry;
    AskGptType type;
    int page;
    QString targetLanguage;
    ReqPDFGptStruct() {
        fileId = 0;
        chatId = 0;
        singleChatId = 0;
        lastSingleChatId = 0;
        parseId = 0;
        summary = 0;
        retry = 0;
        page = 0;
        type = GptFileTalk;
    }
};
Q_DECLARE_METATYPE(ReqPDFGptStruct)

struct GptRspPDFStruct {
    qint64 fileId;
    qint64 chatId;
    qint64 singleChatId;
    QString content;
    QDateTime createTime;
    QString messageType;
    GptFlowStage stage;
    bool waitingAnimation;
    bool fetchData;
    RefMetaDataMap metaDataMap;
    qint64 recordId; // 翻页
    bool finishContent;
    LikeEnum like;
    QString targetLanguage;
    int contentEmpty; //1 can retry
    bool checkBox;
    bool isChecked;
    AskGptType type;
    int page;
    int summary; //  1--摘要 0--非摘要
    bool needOcr;
    bool instruction;
    bool exception;
    GptRspPDFStruct() {
        fileId = 0;
        chatId = 0;
        singleChatId = 0;
        stage = GptBegin;
        waitingAnimation = false;
        fetchData = false;
        recordId = 0;
        finishContent = false;
        like = LikeNull;
        contentEmpty = 0;
        checkBox = false;
        isChecked = false;
        page = 0;
        summary = 0;
        type = GptFileTalk;
        needOcr = false;
        instruction = false;
        exception = false;
    }
};
Q_DECLARE_METATYPE(GptRspPDFStruct)

struct LikePDFStruct {
    int index;
    GptRspPDFStruct rspPdfStruct;
    LikeEnum oldLike;
    LikePDFStruct() {
        index = 0;
        oldLike = LikeNull;
    }
};
Q_DECLARE_METATYPE(LikePDFStruct)

struct GptUsageStruct {
    int vip;
    qint64 fileFreeNum;
    qint64 fileFreePage;
    qint64 fileFreeSize;
    qint64 chatFreeNum;

    qint64 fileProNum;
    qint64 fileProPage;
    qint64 fileProSize;
    qint64 chatProNum;

    qint64 fileUsedNum;
    qint64 chatUsedNum;
    GptUsageStruct() {
        vip = 0;
        fileFreeNum = 0;
        fileFreePage = 0;
        fileFreeSize = 0;
        chatFreeNum = 0;

        fileProNum = 0;
        fileProPage = 0;
        fileProSize = 0;
        chatProNum = 0;

        fileUsedNum = 0;
        chatUsedNum = 0;
    }
};
Q_DECLARE_METATYPE(GptUsageStruct)

///////////////////////////
enum TranslatTypeEnum {
    Text = 1,
    File = 2,
};
enum TranslateStatus {
    TranslateNotStarted = 0,
    TranslateRunning,
    TranslateSucceeded,
    TranslateFailed,
};
enum TranslateDownloadStatus {
    TranslateDownloadNotStarted = 0,
    TranslateDownloading,
    TranslateDownloadFailed,
    TranslateDownloadSuccess,
};

struct ReqTranslateStruct {
    qint64 fileId;
    QString content;
    QString targetLanguage;
    TranslatTypeEnum translatType; // 1文本，2文件
    ReqTranslateStruct() {
        fileId = 0;
        translatType = Text;
    }
};
Q_DECLARE_METATYPE(ReqTranslateStruct)

struct RspTranslateStruct {
    qint64 fileId;
    qint64 recordId;
    QString originContent;
    QString content;
    QString targetLanguage;
    int pageCount;
    QString fileName;
    QString filePath;
    QString fileUrl;
    qint64 fileSize;
    TranslatTypeEnum translatType; // 1文本，2文件
    int progress;
    bool translateRsp;
    bool uploadFail;
    TranslateStatus translateStatus;
    TranslateDownloadStatus downloadStatus;
    bool waitingAnimation;
    bool fetchData;
    RspTranslateStruct() {
        fileId = 0;
        recordId = 0;
        translatType = Text;
        pageCount = 0;
        fileSize = 0;
        waitingAnimation = false;
        fetchData = false;
        translateRsp = false;
        uploadFail = false;
        translateStatus = TranslateNotStarted;
        progress = 0;
        downloadStatus = TranslateDownloadNotStarted;
    }
};
Q_DECLARE_METATYPE(RspTranslateStruct)
typedef QList<RspTranslateStruct> RspTranslateStructs;
Q_DECLARE_METATYPE(RspTranslateStructs)

struct TranslateUsageStruct {
    int vip;
    int showTranslate;
    qint64 fileFreePage;
    qint64 charFreeNum;

    qint64 fileProPage;
    qint64 charProNum;

    qint64 fileUsedPage;
    qint64 charUsedNum;

    qint64 filePurchasePage;
    qint64 charPurchaseNum;
    TranslateUsageStruct() {
        vip = 0;
        showTranslate = 0;
        fileFreePage = 0;
        charFreeNum = 0;
        fileProPage = 0;
        charProNum = 0;
        fileUsedPage = 0;
        charUsedNum = 0;
        filePurchasePage = 0;
        charPurchaseNum = 0;
    }
};
Q_DECLARE_METATYPE(TranslateUsageStruct)

class ChatgptTypeDefine : public QObject
{
    Q_OBJECT
public:
    explicit ChatgptTypeDefine(QObject *parent = nullptr);
    static QList<QPair<QString,QString>> getLangList(bool useTranslate);
    static QString showLangText(const QString &lang);
    static void getFileInfo(const QString &filePath, QString *fileName, QString *fileSize, int *pageCount);
    static void setTranslationAuth(bool auth);
    static bool hasTranslationAuth();
    static QString aiServerHost();
private:
    static bool mTranslationAuth;
};

class CopyTextEdit : public QTextEdit
{
public:
    explicit CopyTextEdit(QWidget *parent = nullptr)
        : QTextEdit(parent)
    {
    }
protected:
    void keyPressEvent(QKeyEvent *event)
    {
        if ((event->modifiers()&Qt::ControlModifier) != 0 && event->key() == Qt::Key_C) {
            QApplication::clipboard()->setText(textCursor().selectedText());
        }
        QTextEdit::keyPressEvent(event);
    }
};
class ColorGradientLabel : public QLabel
{
    Q_OBJECT
public:
    explicit ColorGradientLabel(QWidget *parent = nullptr, Qt::WindowFlags f=Qt::WindowFlags());
    void setPaddingLeft(int padding);
    void setBGColor(const QColor &color);
protected:
    void paintEvent(QPaintEvent *event);
signals:
private:
    int mPaddingLeft;
    QColor mBgColor;

};

class DiffColorTextButton : public QPushButton
{
    Q_OBJECT
public:
    explicit DiffColorTextButton(QWidget *parent = nullptr);
protected:
    void paintEvent(QPaintEvent *event);
};

QString loadLangSetting();
void saveLangSetting(const QString& langCode);
#endif // CHATGPTTYPEDEFINE_H
