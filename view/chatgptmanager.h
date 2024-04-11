#ifndef CHATGPTMANAGER_H
#define CHATGPTMANAGER_H

#include <QObject>
#include <QVariant>
#include "./src/chatgpt/chatgpttypedefine.h"

class RequestTask;
class ChatgptManager : public QObject
{
    Q_OBJECT
public:
    explicit ChatgptManager(QObject *parent = nullptr);
    qint64 readAIChatId();
    void writeAIChatId(qint64 chatId);
    qint64 readPDFChatId(qint64 fileId);
    void writePDFChatId(qint64 fileId, qint64 chatId);

    void analysisFile(qint64 fileId, const QString &password);
    void fileAnalyzed(qint64 fileId);
    void queryAnalysisFileStatus(qint64 parseId, qint64 uid);
    RequestTask* getSingleChatId();
    RequestTask* chatPdfGpt(const ReqPDFGptStruct &reqPdfGpt);
    RequestTask* askGpt(const AskGptStruct &askGptStruct);
    RequestTask* askContextGpt(const AskGptStruct &askGptStruct);


    void instruction(const QString &chatType, const QString &lang);
    void chatHistory(qint64 chatId, qint64 fileId, const QString &chatType, qint64 page);
    void clearChatHistory(qint64 chatId, const QString &chatType);
    void gptUsage();
    void like(int index, const RspGptStruct &rspGptStruct, LikeEnum oldLike);
    void like(int index, const GptRspPDFStruct &gptRspPDFStruct, LikeEnum oldLike);
private:
    QString workSpaceDir();
    void parseRspGptData(const QByteArray &data, RspGptStruct *rspGptStruct, bool *done);
    void parseRspPDFData(const QByteArray &data, GptRspPDFStruct *gptRspPDFStruct, bool *done);
    void checkUsageExceed(int code);
    RequestTask *innerAskGpt(const AskGptStruct &askGptStruct);
signals:
    void fileAnalyzedFinished(bool success, qint64 parseId, int status, int code);
    void analysisFileFinished(bool success, qint64 parseId, int status);
    void getAnalysisFileStatus(bool success, qint64 fileId, qint64 uid, int status, int code);

    void chatPdfGptRsp(bool success, const GptRspPDFStruct &gptRspPDFStruct, bool finished);
    void askGptRsp(bool success, const RspGptStruct &rspGptStruct, bool finished);
    void askContextGptRsp(bool success, const RspGptStruct &rspGptStruct, bool finished);
    void fileTalkHistory(const QList<GptRspPDFStruct> &gptRspPDFStructs, qint64 recordId);
    void aiChatHistory(const QList<RspGptStruct> rspGptStructs, qint64 recordId);
    void clearChatHistoryFinished(const QString &chatType);
    void usageExceed(int code);
    void gptUsageFinished(const GptUsageStruct &usageStruct);
    void updateAiMenuData(bool success, const RspGptStruct &rspGptStruct, bool finished);
    void updateGptItem(int index, const RspGptStruct &rspGptStruct);
    void updatePdfItem(int index, const GptRspPDFStruct &gptRspPDFStruct);
private slots:
    void onAnalysisFileFinished(int state, int code,const QByteArray &data, QVariant opaque);
    void onFileAnalyzedFinished(int state, int code,const QByteArray &data, QVariant opaque);
    void onQueryAnalysisFileStatusFinished(int state, int code,const QByteArray &data, QVariant opaque);
    void onReqPdfGptFrameData(const QByteArray &data);
    void onReqPdfGptFinished(int state, int code,const QByteArray &data, QVariant opaque);
    void onAskGptFrameData(const QByteArray &data);
    void onAskGptFinished(int state, int code,const QByteArray &data, QVariant opaque);
    void onAskContextGptFrameData(const QByteArray &data);
    void onAskContextGptFinished(int state, int code,const QByteArray &data, QVariant opaque);

    void onInstructionFinished(int state, int code,const QByteArray &data, QVariant opaque);
    void onChatHistoryFinished(int state, int code,const QByteArray &data, QVariant opaque);
    void onClearChatHistoryFinished(int state, int code,const QByteArray &data, QVariant opaque);
    void onGptUsageFinished(int state, int code,const QByteArray &data, QVariant opaque);
    void onLikeGptFinished(int state, int code,const QByteArray &data, QVariant opaque);
    void onLikePdfFinished(int state, int code,const QByteArray &data, QVariant opaque);
private:
    RspGptStruct mRspGptContext;
    bool mAddAIRspItem;
    GptRspPDFStruct mGptRspPDFContext;
    bool mAddPDFRspItem;

};

#endif // CHATGPTMANAGER_H
