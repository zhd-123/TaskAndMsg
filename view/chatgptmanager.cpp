#include "chatgptmanager.h"
#include "./src/cloud/netrequestmanager.h"
#include "./src/usercenter/usercentermanager.h"
#include "signalcenter.h"
#include "commonutils.h"
#include <QDir>
#include <QSettings>
#include "framework.h"
#include "./src/util/loginutil.h"

ChatgptManager::ChatgptManager(QObject *parent)
    : QObject{parent}
{

}
QString ChatgptManager::workSpaceDir()
{
    QString workPath = CommonUtils::appWritableDataDir() + QDir::separator() + GPT_Work_Dir;
    QDir dir(workPath);
    if (!dir.exists(workPath)) {
        dir.mkdir(workPath);
    }
    QString userWorkPath = workPath + QDir::separator() + UserCenterManager::instance()->userContext().mUserInfo.uid;
    QDir userDir(userWorkPath);
    if (!userDir.exists(userWorkPath)) {
        userDir.mkdir(userWorkPath);
    }
    return userWorkPath;
}

qint64 ChatgptManager::readAIChatId()
{
    QString configFile = workSpaceDir() + QDir::separator() + GPT_AI_ChatId_Config_File;
    if (!QFile::exists(configFile)) {
        return 0;
    }
    QSettings settings(configFile, QSettings::IniFormat);
    qint64 chatId =  settings.value("chatId").toLongLong();
    return chatId;
}

void ChatgptManager::writeAIChatId(qint64 chatId)
{
    QString configFile = workSpaceDir() + QDir::separator() + GPT_AI_ChatId_Config_File;
    QSettings settings(configFile, QSettings::IniFormat);
    settings.setValue("chatId", chatId);
    settings.sync();
}

qint64 ChatgptManager::readPDFChatId(qint64 fileId)
{
    QString configFile = workSpaceDir() + QDir::separator() + GPT_PDF_ChatId_Config_File;
    if (!QFile::exists(configFile)) {
        return 0;
    }
    QSettings settings(configFile, QSettings::IniFormat);
    return settings.value(QString::number(fileId) + "/" + "chatId").toLongLong();
}

void ChatgptManager::writePDFChatId(qint64 fileId, qint64 chatId)
{
    QString configFile = workSpaceDir() + QDir::separator() + GPT_PDF_ChatId_Config_File;
    QSettings settings(configFile, QSettings::IniFormat);
    settings.setValue(QString::number(fileId) + "/" + "chatId", QString::number(chatId));
    settings.sync();
}

void ChatgptManager::fileAnalyzed(qint64 fileId)
{
    if (!UserCenterManager::instance()->checkLoginStatus()) {
        return;
    }
    RequestTask *task  = new RequestTask(this);
    connect(task, &RequestTask::requestFinished, task, &RequestTask::deleteLater);
    connect(task, &RequestTask::finished, FrameWork::instance(), &FrameWork::onNetworkCheck);
    connect(task, &RequestTask::finished, this, &ChatgptManager::onFileAnalyzedFinished);
    RequestTask::RequestStruct reqStruct;
    RequestResource::setSslConfig(&reqStruct.request);
    setCommonRawHeader(&reqStruct.request);

    // request param
    QMap<QString, QString> reqParmMap;
    reqParmMap.insert("file_id", QString::number(fileId));
    QString urlStr = RequestResource::formatUrl(ChatgptTypeDefine::aiServerHost() + "/file/analyzed", reqParmMap);
    reqStruct.request.setUrl(urlStr);
    reqStruct.type = RequestTask::RequestType::Get;
    task->setForceTip(true);
    task->setRequest(reqStruct);
    RequestManager::instance()->addTask(task, true);
}
void ChatgptManager::onFileAnalyzedFinished(int state, int code, const QByteArray &data, QVariant opaque)
{
    int fileStatus = 0;
    qint64 parseId = 0;
    int stateCode = 0;
    if (RequestTask::RequestState::Success == (RequestTask::RequestState)state) {
        QJsonObject jsonObj, jsonObject;
        if (!data.isEmpty() && CommonUtils::bufferToJson(data, &jsonObj)) {
            if (jsonObj["data"].isObject()) {
                jsonObject = jsonObj["data"].toObject();
                fileStatus = jsonObject["file_status"].toInt();
                parseId = jsonObject["id"].toVariant().toLongLong();
                stateCode = jsonObject["code"].toInt();
                if (stateCode > 0) {
                    FrameWork::instance()->tipMessage(jsonObject["msg"].toString());
                }
             }
        }
    }
    emit fileAnalyzedFinished(state == RequestTask::RequestState::Success, parseId, fileStatus, stateCode);
}

void ChatgptManager::analysisFile(qint64 fileId, const QString &password)
{
    if (!UserCenterManager::instance()->checkLoginStatus()) {
        return;
    }
    RequestTask *task  = new RequestTask(this);
    connect(task, &RequestTask::requestFinished, task, &RequestTask::deleteLater);
    connect(task, &RequestTask::finished, FrameWork::instance(), &FrameWork::onNetworkCheck);
    connect(task, &RequestTask::finished, this, &ChatgptManager::onAnalysisFileFinished);
    RequestTask::RequestStruct reqStruct;
    RequestResource::setSslConfig(&reqStruct.request);
    setCommonRawHeader(&reqStruct.request);

    reqStruct.request.setUrl(QUrl(ChatgptTypeDefine::aiServerHost() + "/file/analysis-file"));
    reqStruct.type = RequestTask::RequestType::Post;
    QString replaceStr = QString("{\"file_id\":%1,\"password\":\"%2\"}").arg(fileId).arg(password);
    reqStruct.postData = replaceStr.toLatin1();
    task->setForceTip(true);
    task->setRequest(reqStruct);
    RequestManager::instance()->addTask(task, true);
}

void ChatgptManager::onAnalysisFileFinished(int state, int code, const QByteArray &data, QVariant opaque)
{
    bool nextStep = false;
    qint64 parseId = 0;
    int fileStatus = 0;
    if (RequestTask::RequestState::Success == (RequestTask::RequestState)state) {
        QJsonObject jsonObj, jsonObject;
        if (!data.isEmpty() && CommonUtils::bufferToJson(data, &jsonObj)) {
            int rspCode = jsonObj["code"].toInt();
            checkUsageExceed(rspCode);
            if (rspCode == 200) {
                if (jsonObj["data"].isObject()) {
                    jsonObject = jsonObj["data"].toObject();
                    parseId = jsonObject["id"].toVariant().toLongLong();
                    fileStatus = jsonObject["file_status"].toInt();
                    nextStep = true;
                }
            }
        }
    }
    emit analysisFileFinished(nextStep, parseId, fileStatus);
}

void ChatgptManager::queryAnalysisFileStatus(qint64 parseId, qint64 uid)
{
    if (!UserCenterManager::instance()->checkLoginStatus()) {
        return;
    }
    RequestTask *task  = new RequestTask(this);
    connect(task, &RequestTask::requestFinished, task, &RequestTask::deleteLater);
    connect(task, &RequestTask::finished, FrameWork::instance(), &FrameWork::onNetworkCheck);
    connect(task, &RequestTask::finished, this, &ChatgptManager::onQueryAnalysisFileStatusFinished);
    RequestTask::RequestStruct reqStruct;
    RequestResource::setSslConfig(&reqStruct.request);
    setCommonRawHeader(&reqStruct.request);

    // request param
    QMap<QString, QString> reqParmMap;
    reqParmMap.insert("id", QString::number(parseId));
    QString urlStr = RequestResource::formatUrl(ChatgptTypeDefine::aiServerHost() + "/file/check-status", reqParmMap);
    reqStruct.request.setUrl(urlStr);
    reqStruct.type = RequestTask::RequestType::Get;
    reqStruct.opaque.setValue(QString::number(parseId) +":"+QString::number(uid));
    task->setRequest(reqStruct);
    RequestManager::instance()->addTask(task, true);
}

RequestTask* ChatgptManager::getSingleChatId()
{
    RequestTask *task  = new RequestTask(this);
    connect(task, &RequestTask::requestFinished, task, &RequestTask::deleteLater);
    connect(task, &RequestTask::finished, FrameWork::instance(), &FrameWork::onNetworkCheck);
    RequestTask::RequestStruct reqStruct;
    RequestResource::setSslConfig(&reqStruct.request);
    setCommonRawHeader(&reqStruct.request);

    // request param
    QMap<QString, QString> reqParmMap;
    QString urlStr = RequestResource::formatUrl(ChatgptTypeDefine::aiServerHost() + "/chat/single-chat-id", reqParmMap);
    reqStruct.request.setUrl(urlStr);
    reqStruct.type = RequestTask::RequestType::Get;
    task->setRequest(reqStruct);
    RequestManager::instance()->addTask(task, true);
    return task;
}

void ChatgptManager::onQueryAnalysisFileStatusFinished(int state, int code, const QByteArray &data, QVariant opaque)
{
    QList<QString> list = opaque.value<QString>().split(":");
    qint64 parseId = list.at(0).toLongLong();
    qint64 uid = list.at(1).toLongLong();
    int fileStatus = 0;
    int stateCode = 0;
    if (RequestTask::RequestState::Success == (RequestTask::RequestState)state) {
        QJsonObject jsonObj, jsonObject;
        if (!data.isEmpty() && CommonUtils::bufferToJson(data, &jsonObj)) {
            if (jsonObj["data"].isObject()) {
                jsonObject = jsonObj["data"].toObject();
                parseId = jsonObject["id"].toVariant().toLongLong();
                fileStatus = jsonObject["file_status"].toInt();
                stateCode = jsonObject["code"].toInt();
                if (stateCode > 0) {
                    FrameWork::instance()->tipMessage(jsonObject["msg"].toString());
                }
            }
        }
    }
    emit getAnalysisFileStatus(state == RequestTask::RequestState::Success, parseId, uid, fileStatus, stateCode);
}

RequestTask* ChatgptManager::chatPdfGpt(const ReqPDFGptStruct &reqPdfGpt)
{
    if (!UserCenterManager::instance()->checkLoginStatus()) {
        return nullptr;
    }
    mGptRspPDFContext = GptRspPDFStruct();
    mAddPDFRspItem = false;

    QString key = "";
    bool addPage = false;
    switch (reqPdfGpt.type) {
    case AskGptType::GptFileTalk:
        key = CHAT_PDF_TYPE_NAME;
        break;
    case AskGptType::GptFilePageSummary:
    {
        key = SUMMARY_PDF_TYPE_NAME;
        addPage = true;
    }
        break;
    case AskGptType::GptFilePageTranslate:
    {
        key = TRANSLATE_PDF_TYPE_NAME;
        addPage = true;
    }
        break;
    default:
        break;
    }
    QJsonObject jsonObject;
    jsonObject.insert("auto_summary", reqPdfGpt.summary);
    jsonObject.insert("chat_type", key);
    jsonObject.insert("content", reqPdfGpt.content);
    jsonObject.insert("retry", reqPdfGpt.retry);
    if (addPage) {
        jsonObject.insert("page", reqPdfGpt.page);
    }
    if (!reqPdfGpt.targetLanguage.isEmpty()) {
        jsonObject.insert("target_lang", reqPdfGpt.targetLanguage);
    }

    RequestTask *task  = new RequestTask(this);
    connect(task, &RequestTask::requestFinished, task, &RequestTask::deleteLater);
    connect(task, &RequestTask::finished, FrameWork::instance(), &FrameWork::onNetworkCheck);
    connect(task, &RequestTask::frameData, this, &ChatgptManager::onReqPdfGptFrameData);
    connect(task, &RequestTask::finished, this, &ChatgptManager::onReqPdfGptFinished);
    RequestTask::RequestStruct reqStruct;
    RequestResource::setSslConfig(&reqStruct.request);
    setCommonRawHeader(&reqStruct.request, "application/octet-stream");

    reqStruct.request.setUrl(QUrl(ChatgptTypeDefine::aiServerHost() + "/chat/talk-stream"));
    reqStruct.type = RequestTask::RequestType::Post;
    QByteArray byteArray = QJsonDocument(jsonObject).toJson(QJsonDocument::Compact);
    QString replaceStr = "";
    if (reqPdfGpt.lastSingleChatId > 0) {
        replaceStr = QString("{\"chat_id\":%1,\"file_id\":%2,\"id\":%3,\"single_chat_id\":%4,\"last_single_chat_id\":%5,").arg(reqPdfGpt.chatId).arg(reqPdfGpt.fileId)
                .arg(reqPdfGpt.parseId).arg(reqPdfGpt.singleChatId).arg(reqPdfGpt.lastSingleChatId);
    } else {
        replaceStr = QString("{\"chat_id\":%1,\"file_id\":%2,\"id\":%3,\"single_chat_id\":%4,").arg(reqPdfGpt.chatId).arg(reqPdfGpt.fileId).arg(reqPdfGpt.parseId)
                .arg(reqPdfGpt.singleChatId);
    }
    byteArray = replaceStr.toLatin1() + byteArray.mid(1);
    reqStruct.postData = byteArray;
    reqStruct.frameRead = true;
    reqStruct.timeout = 30 * 60 * 1000;
    task->setRequest(reqStruct);
    RequestManager::instance()->addTask(task, true);
    return task;
}

void ChatgptManager::onReqPdfGptFrameData(const QByteArray &data)
{
    GptRspPDFStruct gptRspPDFStruct;

    bool done = false;
    parseRspPDFData(data, &gptRspPDFStruct, &done);
    if (!gptRspPDFStruct.content.isEmpty()) {
        emit chatPdfGptRsp(done, gptRspPDFStruct, false);
    } else {
        if (data.contains("<html>")) {
            gptRspPDFStruct.exception = true;
            emit chatPdfGptRsp(done, gptRspPDFStruct, true);
        }
    }
}

void ChatgptManager::onReqPdfGptFinished(int state, int code, const QByteArray &data, QVariant opaque)
{
    GptRspPDFStruct gptRspPDFStruct;
    bool done = false;
    parseRspPDFData(data, &gptRspPDFStruct, &done);
    gptRspPDFStruct.stage = GptEnd;
    gptRspPDFStruct.finishContent = true;
    emit chatPdfGptRsp(done, gptRspPDFStruct, true);

    if (RequestTask::RequestState::Success == (RequestTask::RequestState)state) {
        QJsonObject jsonObj;
        if (!data.isEmpty() && CommonUtils::bufferToJson(data, &jsonObj)) {
            if (jsonObj.contains("code")) {
                int code = jsonObj["code"].toInt();
                checkUsageExceed(code);
            }
        }
    }
}

void ChatgptManager::parseRspPDFData(const QByteArray &data, GptRspPDFStruct *gptRspPDFStruct, bool *done)
{
    *done = false;
    QList<QString> dataList = QString::fromUtf8(data).split("}\n");
    //qDebug() << "data:" << data << "\n";
    mGptRspPDFContext.content = "";
    for (QString jsonStr : dataList) {
        if (!jsonStr.endsWith("}")) {
            jsonStr += "}";
        }
        QByteArray jsonData = jsonStr.toUtf8();
        QJsonObject jsonObj, jsonObject, choice, fileContent, metaData;
        QJsonArray jsonArray, posData;
        if (!jsonData.isEmpty() && CommonUtils::bufferToJson(jsonData, &jsonObj)) {
            if (jsonObj.contains("choices")) {
                jsonArray = jsonObj["choices"].toArray();
                for (int i = 0; i < jsonArray.size(); ++i) {
                    choice = jsonArray.at(i).toObject();
                    jsonObject = choice["delta"].toObject();
                    if (jsonObject.contains("role")) {
                        mGptRspPDFContext.messageType = jsonObject["role"].toString();
                        mGptRspPDFContext.createTime = QDateTime::fromSecsSinceEpoch(jsonObj["created"].toDouble());
                    }
                    mGptRspPDFContext.content += jsonObject["content"].toString();
                }
                *done = true;
                *gptRspPDFStruct = mGptRspPDFContext;
            } else if (jsonObj.contains("chat_id")) {
                qint64 chatId = jsonObj["chat_id"].toVariant().toLongLong();
                qint64 singleChatId = jsonObj["single_chat_id"].toVariant().toLongLong();
                qint64 recordId = jsonObj["id"].toVariant().toLongLong();
                qint64 fileId = jsonObj["file_id"].toVariant().toLongLong();
                QString messageType = jsonObj["message_type"].toString();
                QString content = jsonObj["empty_content"].toString();
                QString targetLang = jsonObj["target_lang"].toString();
                QString chatType = jsonObj["chat_type"].toString();
                int contentEmpty = jsonObj["content_empty"].toInt();
                int page = jsonObj["page"].toInt();
                int summary = jsonObj["auto_summary"].toInt();
                QDateTime createTime = QDateTime::fromSecsSinceEpoch(jsonObj["created_at"].toDouble());
                jsonArray = jsonObj["org_file_content"].toArray();
                RefMetaDataMap refMetaDataMap;
                for (int i = 0; i < jsonArray.size(); ++i) {
                    fileContent = jsonArray.at(i).toObject();
                    metaData = fileContent["metadata"].toObject();
                    RefMetaData meta;
                    int page = metaData["page"].toInt();
                    if (page == 0) {
                        continue;
                    }
                    if (refMetaDataMap.contains(page)) {
                        meta = refMetaDataMap.value(page);
                    } else {
                        meta.page = page;
                        meta.text = metaData["text"].toString();
                    }
                    posData = metaData["pos"].toArray();
                    if (posData.size() >= 8) {
                        PDF_QUADPOINTSF rect;
                        rect.x1 = posData.at(0).toDouble();
                        rect.y1 = posData.at(1).toDouble();
                        rect.x2 = posData.at(2).toDouble();
                        rect.y2 = posData.at(3).toDouble();
                        rect.x3 = posData.at(4).toDouble();
                        rect.y3 = posData.at(5).toDouble();
                        rect.x4 = posData.at(6).toDouble();
                        rect.y4 = posData.at(7).toDouble();
                        meta.rectList.append(rect);
                    }
                    refMetaDataMap.insert(page, meta);
                }
                *done = true;
                *gptRspPDFStruct = mGptRspPDFContext;
                (*gptRspPDFStruct).chatId = chatId;
                (*gptRspPDFStruct).singleChatId = singleChatId;
                (*gptRspPDFStruct).fileId = fileId;
                (*gptRspPDFStruct).recordId = recordId;

                AskGptType type = GptFileTalk;
                if (chatType == CHAT_PDF_TYPE_NAME) {
                    type = GptFileTalk;
                } else if (chatType == SUMMARY_PDF_TYPE_NAME) {
                    type = GptFilePageSummary;
                } else if (chatType == TRANSLATE_PDF_TYPE_NAME) {
                    type = GptFilePageTranslate;
                }
                (*gptRspPDFStruct).type = type;
                (*gptRspPDFStruct).contentEmpty = contentEmpty;
                if (contentEmpty) {
                    content = tr("I apologize, but we had trouble processing your most recent request. Could you please try again?");
                }
                (*gptRspPDFStruct).targetLanguage = targetLang;
                if (!content.isEmpty()) {
                    (*gptRspPDFStruct).content = content;
                }
                (*gptRspPDFStruct).createTime = createTime;
                (*gptRspPDFStruct).page = page;
                (*gptRspPDFStruct).summary = summary;
                (*gptRspPDFStruct).messageType = messageType;
                (*gptRspPDFStruct).metaDataMap = refMetaDataMap;
            } else if (jsonObj.contains("msg")) {
                QString content = jsonObj["msg"].toString();
                *done = true;
                *gptRspPDFStruct = mGptRspPDFContext;
                (*gptRspPDFStruct).exception = true;
                int code = jsonObj["code"].toInt();
                if (code == 7021) {
//                    (*gptRspPDFStruct).needOcr = true;
                    content = tr("Your file does not contain any text content. Please perform OCR to recognize and upload the file in order to continue using the \"%1\" function.").arg(tr("Ask PDF"));
                } else if (code == 7024) {
                    (*gptRspPDFStruct).contentEmpty = 1;
                    (*gptRspPDFStruct).exception = false;
                    content = tr("I apologize, but we had trouble processing your most recent request. Could you please try again?");
                }
                (*gptRspPDFStruct).content = content;
            }
        }
    }
    if (!mAddPDFRspItem) {
        mAddPDFRspItem = true;
        (*gptRspPDFStruct).stage = GptBegin;
    } else {
        (*gptRspPDFStruct).stage = GptFlow;
    }
    (*gptRspPDFStruct).messageType = AI_TYPE_NAME;
}

void ChatgptManager::checkUsageExceed(int code)
{
    if (code == 7003 || code == 7004 || code == 7005
            || code == 7006) {
        emit usageExceed(code);
    }
}

RequestTask *ChatgptManager::innerAskGpt(const AskGptStruct &askGptStruct)
{
    QString key = "";
    switch (askGptStruct.type) {
    case AskGptType::GptRandomChat:
        key = "random_talk";
        break;
    case AskGptType::GptSummarize:
        key = "summary";
        break;
    case AskGptType::GptTranslate:
        key = "translate";
        break;
    case AskGptType::GptExplain:
        key = "explain";
        break;
    default:
        break;
    }

    auto langCode = askGptStruct.targetLanguage;
    if(langCode == "zh-CN"){
        langCode = "zh-hans";
    }
    else if(langCode == "zh-HK"){
        langCode = "zh-hant";
    }

    QJsonObject jsonObject;
    jsonObject.insert("auto_summary", askGptStruct.summary);
    jsonObject.insert("chat_type", key);
    jsonObject.insert("retry", askGptStruct.retry);
    jsonObject.insert("content", askGptStruct.content);
    if (!langCode.isEmpty()) {
        jsonObject.insert("target_lang", langCode);
    }


    RequestTask *task  = new RequestTask(this);
    connect(task, &RequestTask::requestFinished, task, &RequestTask::deleteLater);
    connect(task, &RequestTask::finished, FrameWork::instance(), &FrameWork::onNetworkCheck);
    RequestTask::RequestStruct reqStruct;
    RequestResource::setSslConfig(&reqStruct.request);
    setCommonRawHeader(&reqStruct.request, "application/octet-stream");

    reqStruct.request.setUrl(QUrl(ChatgptTypeDefine::aiServerHost() + "/chat/talk-stream"));
    reqStruct.type = RequestTask::RequestType::Post;
    QByteArray byteArray = QJsonDocument(jsonObject).toJson(QJsonDocument::Compact);
    QString replaceStr = "";
    if (askGptStruct.lastSingleChatId > 0) {
        replaceStr = QString("{\"chat_id\":%1,\"file_id\":%2,\"single_chat_id\":%3,\"last_single_chat_id\":%4,").arg(askGptStruct.chatId).arg(askGptStruct.fileId)
                 .arg(askGptStruct.singleChatId).arg(askGptStruct.lastSingleChatId);
    } else {
        replaceStr = QString("{\"chat_id\":%1,\"file_id\":%2,\"single_chat_id\":%3,").arg(askGptStruct.chatId).arg(askGptStruct.fileId)
                 .arg(askGptStruct.singleChatId);
    }
    byteArray = replaceStr.toLatin1() + byteArray.mid(1);
    reqStruct.postData = byteArray;
    reqStruct.opaque.setValue(askGptStruct);
    reqStruct.frameRead = true;
    reqStruct.timeout = 30 * 60 * 1000;
    task->setRequest(reqStruct);
    RequestManager::instance()->addTask(task, true);
    return task;
}

RequestTask * ChatgptManager::askGpt(const AskGptStruct &askGptStruct)
{
    if (!UserCenterManager::instance()->checkLoginStatus()) {
        return nullptr;
    }

    mRspGptContext = RspGptStruct();
    mAddAIRspItem = false;
    RequestTask *task = innerAskGpt(askGptStruct);
    connect(task, &RequestTask::frameData, this, &ChatgptManager::onAskGptFrameData);
    connect(task, &RequestTask::finished, this, &ChatgptManager::onAskGptFinished);
    return task;
}

void ChatgptManager::onAskGptFrameData(const QByteArray &data)
{
    RspGptStruct rspGptStruct;
    bool done = false;
    parseRspGptData(data, &rspGptStruct, &done);
    if (!rspGptStruct.content.isEmpty()) {
        emit askGptRsp(done, rspGptStruct, false);
    } else {
        if (data.contains("<html>")) {
            rspGptStruct.exception = true;
            emit askGptRsp(done, rspGptStruct, true);
        }
    }
}

void ChatgptManager::onAskGptFinished(int state, int code, const QByteArray &data, QVariant opaque)
{
    bool done = false;
    AskGptStruct askGptStruct = opaque.value<AskGptStruct>();
    RspGptStruct rspGptStruct;
    parseRspGptData(data, &rspGptStruct, &done);
    rspGptStruct.type = askGptStruct.type;
    rspGptStruct.finishContent = true;
    rspGptStruct.stage = GptEnd;
    emit askGptRsp(done, rspGptStruct, true);

    if (RequestTask::RequestState::Success == (RequestTask::RequestState)state) {
        QJsonObject jsonObj;
        if (!data.isEmpty() && CommonUtils::bufferToJson(data, &jsonObj)) {
            if (jsonObj.contains("code")) {
                int code = jsonObj["code"].toInt();
                checkUsageExceed(code);
            }
        }
    }
}

RequestTask *ChatgptManager::askContextGpt(const AskGptStruct &askGptStruct)
{
    if (!UserCenterManager::instance()->checkLoginStatus()) {
        return nullptr;
    }
    mRspGptContext = RspGptStruct();
    mAddAIRspItem = false;

    RequestTask *task = innerAskGpt(askGptStruct);
    connect(task, &RequestTask::frameData, this, &ChatgptManager::onAskContextGptFrameData);
    connect(task, &RequestTask::finished, this, &ChatgptManager::onAskContextGptFinished);
    return task;
}

void ChatgptManager::onAskContextGptFrameData(const QByteArray &data)
{
    RspGptStruct rspGptStruct;
    bool done = false;
    parseRspGptData(data, &rspGptStruct, &done);
    if (!rspGptStruct.content.isEmpty()) {
        emit askContextGptRsp(done, rspGptStruct, false);
    } else {
        if (data.contains("<html>")) {
            emit askContextGptRsp(done, rspGptStruct, true);
        }
    }
}

void ChatgptManager::onAskContextGptFinished(int state, int code, const QByteArray &data, QVariant opaque)
{
    bool done = false;
    AskGptStruct askGptStruct = opaque.value<AskGptStruct>();
    RspGptStruct rspGptStruct;
    parseRspGptData(data, &rspGptStruct, &done);
    rspGptStruct.type = askGptStruct.type;
    emit askContextGptRsp(done, rspGptStruct, true);

    if (RequestTask::RequestState::Success == (RequestTask::RequestState)state) {
        QJsonObject jsonObj;
        if (!data.isEmpty() && CommonUtils::bufferToJson(data, &jsonObj)) {
            if (jsonObj.contains("code")) {
                int code = jsonObj["code"].toInt();
                checkUsageExceed(code);
            }
        }
    }
}

void ChatgptManager::parseRspGptData(const QByteArray &data, RspGptStruct *rspGptStruct, bool *done)
{
    *done = false;
    QList<QString> dataList = QString::fromUtf8(data).split("}\n");
//    qDebug() << "data:" << data << "\n";
    mRspGptContext.content = "";
    for (QString jsonStr : dataList) {
        if (!jsonStr.endsWith("}")) {
            jsonStr += "}";
        }
        QByteArray jsonData = jsonStr.toUtf8();
        QJsonObject jsonObj, jsonObject, choice, fileContent;
        QJsonArray jsonArray;
        if (!jsonData.isEmpty() && CommonUtils::bufferToJson(jsonData, &jsonObj)) {
            if (jsonObj.contains("choices")) {
                jsonArray = jsonObj["choices"].toArray();
                for (int i = 0; i < jsonArray.size(); ++i) {
                    choice = jsonArray.at(i).toObject();
                    jsonObject = choice["delta"].toObject();
                    if (jsonObject.contains("role")) {
                        mRspGptContext.messageType = jsonObject["role"].toString();
                        mRspGptContext.createTime = QDateTime::fromSecsSinceEpoch(jsonObj["created"].toDouble());
                    }
                    mRspGptContext.content += jsonObject["content"].toString();
                }
                *done = true;
                *rspGptStruct = mRspGptContext;
            } else if (jsonObj.contains("chat_id")) {
                qint64 chatId = jsonObj["chat_id"].toVariant().toLongLong();
                qint64 singleChatId = jsonObj["single_chat_id"].toVariant().toLongLong();
                qint64 recordId = jsonObj["id"].toVariant().toLongLong();
                QString messageType = jsonObj["message_type"].toString();
                QString content = jsonObj["empty_content"].toString();
                QString targetLang = jsonObj["target_lang"].toString();
                QString chatType = jsonObj["chat_type"].toString();
                int contentEmpty = jsonObj["content_empty"].toInt();
                QDateTime createTime = QDateTime::fromSecsSinceEpoch(jsonObj["created_at"].toDouble());

                *done = true;
                *rspGptStruct = mRspGptContext;
                (*rspGptStruct).chatId = chatId;
                if (contentEmpty) {
                    content = tr("I apologize, but we had trouble processing your most recent request. Could you please try again?");
                }
                if (!content.isEmpty()) {
                    (*rspGptStruct).content = content;
                }
                (*rspGptStruct).createTime = createTime;
                (*rspGptStruct).recordId = recordId;
                (*rspGptStruct).singleChatId = singleChatId;
                (*rspGptStruct).contentEmpty = contentEmpty;
                (*rspGptStruct).targetLanguage = targetLang;
                (*rspGptStruct).messageType = messageType;
                AskGptType type = GptRandomChat;
                if (chatType == "random_talk") {
                    type = GptRandomChat;
                } else if (chatType == "translate") {
                    type = GptTranslate;
                } else if (chatType == "summary") {
                    type = GptSummarize;
                } else if (chatType == "explain") {
                    type = GptExplain;
                }
                (*rspGptStruct).type = type;
            } else if (jsonObj.contains("msg")) {
                QString content = jsonObj["msg"].toString();
                *done = true;
                *rspGptStruct = mRspGptContext;
                int code = jsonObj["code"].toInt();
                if (code == 7024) {
                    (*rspGptStruct).contentEmpty = 1;
                    content = tr("I apologize, but we had trouble processing your most recent request. Could you please try again?");
                } else {
                    (*rspGptStruct).exception = true;
                }
                (*rspGptStruct).content = content;
            }
        }
    }
    if (!mAddAIRspItem) {
        mAddAIRspItem = true;
        (*rspGptStruct).stage = GptBegin;
    } else {
        (*rspGptStruct).stage = GptFlow;
    }
    (*rspGptStruct).messageType = AI_TYPE_NAME;
}

void ChatgptManager::instruction(const QString &chatType, const QString &lang)
{
    if (!UserCenterManager::instance()->checkLoginStatus()) {
        return;
    }
    RequestTask *task  = new RequestTask(this);
    connect(task, &RequestTask::requestFinished, task, &RequestTask::deleteLater);
    connect(task, &RequestTask::finished, FrameWork::instance(), &FrameWork::onNetworkCheck);
    connect(task, &RequestTask::finished, this, &ChatgptManager::onInstructionFinished);
    RequestTask::RequestStruct reqStruct;
    RequestResource::setSslConfig(&reqStruct.request);
    setCommonRawHeader(&reqStruct.request);

    // request param
    QMap<QString, QString> reqParmMap;
    reqParmMap.insert("lang_type", lang);
    reqParmMap.insert("chat_type", chatType);
    QString urlStr = RequestResource::formatUrl(ChatgptTypeDefine::aiServerHost() + "/chat/instruction", reqParmMap);
    reqStruct.request.setUrl(urlStr);
    reqStruct.type = RequestTask::RequestType::Get;
    reqStruct.opaque.setValue(chatType);
    task->setForceTip(true);
    task->setRequest(reqStruct);
    RequestManager::instance()->addTask(task, true);
}
void ChatgptManager::onInstructionFinished(int state, int code, const QByteArray &data, QVariant opaque)
{
    QString chatType = opaque.value<QString>();
    if (RequestTask::RequestState::Success == (RequestTask::RequestState)state) {
        QJsonObject jsonObj, jsonObject;
        if (!data.isEmpty() && CommonUtils::bufferToJson(data, &jsonObj)) {
            if (jsonObj["data"].isObject()) {
                jsonObject = jsonObj["data"].toObject();
                QString content = jsonObject["change_chat_type"].toString();
                QString langType = jsonObject["change_lang_type"].toString();
                if (!content.isEmpty()) {
                    if (chatType == CHAT_PDF_TYPE_NAME) {
                        GptRspPDFStruct gptRspPDFStruct;
                        gptRspPDFStruct.messageType = AI_TYPE_NAME;
                        gptRspPDFStruct.stage = GptBegin;
                        gptRspPDFStruct.content = content;
                        gptRspPDFStruct.finishContent = true;
                        gptRspPDFStruct.instruction = true;
                        emit chatPdfGptRsp(true, gptRspPDFStruct, true);
                    } else {
                        RspGptStruct rspGptStruct;
                        rspGptStruct.messageType = AI_TYPE_NAME;
                        rspGptStruct.stage = GptBegin;
                        rspGptStruct.content = content;
                        rspGptStruct.finishContent = true;
                        rspGptStruct.instruction = true;
                        emit askGptRsp(true, rspGptStruct, true);
                    }
                }
            }
        }
    }
}

void ChatgptManager::chatHistory(qint64 chatId, qint64 fileId, const QString &chatType, qint64 page)
{
    if (!UserCenterManager::instance()->checkLoginStatus()) {
        return;
    }
    RequestTask *task  = new RequestTask(this);
    connect(task, &RequestTask::requestFinished, task, &RequestTask::deleteLater);
    connect(task, &RequestTask::finished, FrameWork::instance(), &FrameWork::onNetworkCheck);
    connect(task, &RequestTask::finished, this, &ChatgptManager::onChatHistoryFinished);
    RequestTask::RequestStruct reqStruct;
    RequestResource::setSslConfig(&reqStruct.request);
    setCommonRawHeader(&reqStruct.request);

    // request param
    QMap<QString, QString> reqParmMap;
    reqParmMap.insert("chat_id", QString::number(chatId));
    if (chatType == CHAT_PDF_TYPE_NAME) {
        reqParmMap.insert("file_id", QString::number(fileId));
        reqParmMap.insert("chat_type", chatType);
    }
    if (page > 0) {
        reqParmMap.insert("token", QString::number(page));
    }
    QString urlStr = RequestResource::formatUrl(ChatgptTypeDefine::aiServerHost() + "/chat/history", reqParmMap);
    reqStruct.request.setUrl(urlStr);
    reqStruct.type = RequestTask::RequestType::Get;
    reqStruct.opaque.setValue(chatType + ":" + QString::number(page));
    task->setRequest(reqStruct);
    RequestManager::instance()->addTask(task, true);
}
void ChatgptManager::onChatHistoryFinished(int state, int code, const QByteArray &data, QVariant opaque)
{
    QList<GptRspPDFStruct> gptRspPDFStructs;
    QList<RspGptStruct> rspGptStructs;
    // chatType + ":" + QString::number(page)
    QList<QString> list = opaque.value<QString>().split(":");
    QString chatType = list.at(0);
    qint64 page = list.at(1).toLongLong();
    if (RequestTask::RequestState::Success == (RequestTask::RequestState)state) {
        QJsonObject jsonObj, jsonObject, chatMsg, fileContent, metaData;
        QJsonArray jsonArray, contentArray, posData;
        if (!data.isEmpty() && CommonUtils::bufferToJson(data, &jsonObj)) {
            if (jsonObj["data"].isObject()) {
                jsonObject = jsonObj["data"].toObject();
                jsonArray = jsonObject["list"].toArray();
                for (int i = 0; i < jsonArray.size(); ++i) {
                    chatMsg = jsonArray.at(i).toObject();
                    qint64 chatId = chatMsg["chat_id"].toVariant().toLongLong();
                    qint64 singleChatId = chatMsg["single_chat_id"].toVariant().toLongLong();
                    qint64 fileId = chatMsg["file_id"].toVariant().toLongLong();
                    qint64 recordId = chatMsg["id"].toVariant().toLongLong();
                    int like = chatMsg["liked"].toInt();
                    int contentEmpty = chatMsg["content_empty"].toInt();
                    QString roleType = chatMsg["role_type"].toString();
                    QString chatType = chatMsg["chat_type"].toString();
                    QString messageType = chatMsg["message_type"].toString();
                    QString content = chatMsg["content"].toString();
                    if (contentEmpty) {
                        content = tr("I apologize, but we had trouble processing your most recent request. Could you please try again?");
                    }
                    QString targetLang = chatMsg["target_lang"].toString();
                    int page = chatMsg["page"].toInt();
                    int summary = chatMsg["auto_summary"].toInt();
                    QDateTime createTime = QDateTime::fromSecsSinceEpoch(chatMsg["created_at"].toDouble());

                    contentArray = chatMsg["org_file_content"].toArray();
                    RefMetaDataMap refMetaDataMap;
                    for (int i = 0; i < contentArray.size(); ++i) {
                        fileContent = contentArray.at(i).toObject();
                        metaData = fileContent["metadata"].toObject();
                        RefMetaData meta;
                        int page = metaData["page"].toInt();
                        if (page == 0) {
                            continue;
                        }
                        if (refMetaDataMap.contains(page)) {
                            meta = refMetaDataMap.value(page);
                        } else {
                            meta.page = page;
                            meta.text = metaData["text"].toString();
                        }
                        posData = metaData["pos"].toArray();
                        if (posData.size() >= 8) {
                            PDF_QUADPOINTSF rect;
                            rect.x1 = posData.at(0).toDouble();
                            rect.y1 = posData.at(1).toDouble();
                            rect.x2 = posData.at(2).toDouble();
                            rect.y2 = posData.at(3).toDouble();
                            rect.x3 = posData.at(4).toDouble();
                            rect.y3 = posData.at(5).toDouble();
                            rect.x4 = posData.at(6).toDouble();
                            rect.y4 = posData.at(7).toDouble();
                            meta.rectList.append(rect);
                        }
                        refMetaDataMap.insert(page, meta);
                    }
                    if (chatType == CHAT_PDF_TYPE_NAME || chatType == SUMMARY_PDF_TYPE_NAME || chatType == TRANSLATE_PDF_TYPE_NAME) {
                        GptRspPDFStruct gptRspPDFStruct;
                        gptRspPDFStruct.stage = GptEnd;
                        gptRspPDFStruct.chatId = chatId;
                        gptRspPDFStruct.singleChatId = singleChatId;
                        gptRspPDFStruct.fileId = fileId;
                        gptRspPDFStruct.content = content;
                        gptRspPDFStruct.createTime = createTime;
                        gptRspPDFStruct.messageType = messageType;
                        gptRspPDFStruct.metaDataMap = refMetaDataMap;
                        gptRspPDFStruct.recordId = recordId;
                        gptRspPDFStruct.like = (LikeEnum)like;
                        gptRspPDFStruct.contentEmpty = contentEmpty;
                        gptRspPDFStruct.targetLanguage = targetLang;
                        gptRspPDFStruct.page = page;
                        gptRspPDFStruct.summary = summary;
                        gptRspPDFStruct.finishContent = true;

                        AskGptType type = GptFileTalk;
                        if (chatType == CHAT_PDF_TYPE_NAME) {
                            type = GptFileTalk;
                        } else if (chatType == SUMMARY_PDF_TYPE_NAME) {
                            type = GptFilePageSummary;
                        } else if (chatType == TRANSLATE_PDF_TYPE_NAME) {
                            type = GptFilePageTranslate;
                        }
                        gptRspPDFStruct.type = type;
                        gptRspPDFStructs.append(gptRspPDFStruct);
                    } else {
                        RspGptStruct rspGptStruct;
                        rspGptStruct.stage = GptEnd;
                        rspGptStruct.chatId = chatId;
                        rspGptStruct.singleChatId = singleChatId;
                        rspGptStruct.content = content;
                        rspGptStruct.createTime = createTime;
                        rspGptStruct.messageType = messageType;
                        rspGptStruct.recordId = recordId;
                        rspGptStruct.like = (LikeEnum)like;
                        rspGptStruct.contentEmpty = contentEmpty;
                        rspGptStruct.targetLanguage = targetLang;
                        rspGptStruct.finishContent = true;

                        AskGptType type = GptRandomChat;
                        if (chatType == "random_talk") {
                            type = GptRandomChat;
                        } else if (chatType == "translate") {
                            type = GptTranslate;
                        } else if (chatType == "summary") {
                            type = GptSummarize;
                        } else if (chatType == "explain") {
                            type = GptExplain;
                        }
                        rspGptStruct.type = type;
                        rspGptStructs.append(rspGptStruct);
                    }
                }
            }
        }
    }
    if (chatType == CHAT_PDF_TYPE_NAME) {
        emit fileTalkHistory(gptRspPDFStructs, page);
    } else {
        emit aiChatHistory(rspGptStructs, page);
    }
}

void ChatgptManager::clearChatHistory(qint64 chatId, const QString &chatType)
{
    if (!UserCenterManager::instance()->checkLoginStatus()) {
        return;
    }
    RequestTask *task  = new RequestTask(this);
    connect(task, &RequestTask::requestFinished, task, &RequestTask::deleteLater);
    connect(task, &RequestTask::finished, FrameWork::instance(), &FrameWork::onNetworkCheck);
    connect(task, &RequestTask::finished, this, &ChatgptManager::onClearChatHistoryFinished);
    RequestTask::RequestStruct reqStruct;
    RequestResource::setSslConfig(&reqStruct.request);
    setCommonRawHeader(&reqStruct.request);

    reqStruct.request.setUrl(QUrl(ChatgptTypeDefine::aiServerHost() + "/chat/clear"));
    reqStruct.type = RequestTask::RequestType::Post;
    QString replaceStr = QString("{\"chat_id\":%1}").arg(chatId);
    reqStruct.postData = replaceStr.toLatin1();
    reqStruct.opaque.setValue(chatType);
    task->setForceTip(true);
    task->setRequest(reqStruct);
    RequestManager::instance()->addTask(task, true);
}

void ChatgptManager::onClearChatHistoryFinished(int state, int code, const QByteArray &data, QVariant opaque)
{
    QString chatType = opaque.value<QString>();
    if (RequestTask::RequestState::Success == (RequestTask::RequestState)state) {
        QJsonObject jsonObj, jsonObject;
        if (!data.isEmpty() && CommonUtils::bufferToJson(data, &jsonObj)) {      
            if (jsonObj["code"] == 200) {
                emit clearChatHistoryFinished(chatType);
            }
        }
    }
}

void ChatgptManager::gptUsage()
{
    if (!UserCenterManager::instance()->checkLoginStatus()) {
        return;
    }
    RequestTask *task  = new RequestTask(this);
    connect(task, &RequestTask::requestFinished, task, &RequestTask::deleteLater);
    connect(task, &RequestTask::finished, FrameWork::instance(), &FrameWork::onNetworkCheck);
    connect(task, &RequestTask::finished, this, &ChatgptManager::onGptUsageFinished);
    RequestTask::RequestStruct reqStruct;
    RequestResource::setSslConfig(&reqStruct.request);
    setCommonRawHeader(&reqStruct.request);

    reqStruct.request.setUrl(QUrl(ChatgptTypeDefine::aiServerHost() + "/chat/usage"));
    reqStruct.type = RequestTask::RequestType::Get;
    task->setRequest(reqStruct);
    RequestManager::instance()->addTask(task, true);
}

void ChatgptManager::onGptUsageFinished(int state, int code, const QByteArray &data, QVariant opaque)
{
    if (RequestTask::RequestState::Success == (RequestTask::RequestState)state) {
        QJsonObject jsonObj, jsonObject, freeObject, proObject, usageObject;
        if (!data.isEmpty() && CommonUtils::bufferToJson(data, &jsonObj)) {
            if (jsonObj["code"] == 200) {
                jsonObject = jsonObj["data"].toObject();
                GptUsageStruct usageStruct;
                usageStruct.vip = jsonObject["ai_vip"].toInt();
                freeObject = jsonObject["free"].toObject();
                usageStruct.fileFreeNum =  freeObject["file_nums"].toVariant().toLongLong();
                usageStruct.fileFreePage = freeObject["file_pages"].toVariant().toLongLong();
                usageStruct.fileFreeSize = freeObject["file_size"].toVariant().toLongLong();
                usageStruct.chatFreeNum =  freeObject["chat_nums"].toVariant().toLongLong();
                proObject = jsonObject["pro"].toObject();
                usageStruct.fileProNum =  proObject["file_nums"].toVariant().toLongLong();
                usageStruct.fileProPage = proObject["file_pages"].toVariant().toLongLong();
                usageStruct.fileProSize = proObject["file_size"].toVariant().toLongLong();
                usageStruct.chatProNum =  proObject["chat_nums"].toVariant().toLongLong();
                usageObject = jsonObject["usage"].toObject();
                usageStruct.fileUsedNum =  usageObject["file_nums"].toVariant().toLongLong();
                usageStruct.chatUsedNum =  usageObject["chat_nums"].toVariant().toLongLong();
                emit gptUsageFinished(usageStruct);
            }
        }
    }
}

void ChatgptManager::like(int index, const RspGptStruct &rspGptStruct, LikeEnum oldLike)
{
    if (!UserCenterManager::instance()->checkLoginStatus()) {
        return;
    }
    RequestTask *task  = new RequestTask(this);
    connect(task, &RequestTask::requestFinished, task, &RequestTask::deleteLater);
    connect(task, &RequestTask::finished, FrameWork::instance(), &FrameWork::onNetworkCheck);
    connect(task, &RequestTask::finished, this, &ChatgptManager::onLikeGptFinished);
    RequestTask::RequestStruct reqStruct;
    RequestResource::setSslConfig(&reqStruct.request);
    setCommonRawHeader(&reqStruct.request);

    reqStruct.request.setUrl(QUrl(ChatgptTypeDefine::aiServerHost() + "/chat/like"));
    reqStruct.type = RequestTask::RequestType::Post;
    QString replaceStr = QString("{\"id\":%1, \"like\":%2}").arg(rspGptStruct.recordId).arg(int(rspGptStruct.like));
    reqStruct.postData = replaceStr.toLatin1();
    task->setRequest(reqStruct);
    LikeGptStruct likeGptStruct;
    likeGptStruct.index = index;
    likeGptStruct.rspGptStruct = rspGptStruct;
    likeGptStruct.oldLike = oldLike;
    reqStruct.opaque.setValue(likeGptStruct);
    RequestManager::instance()->addTask(task, true);
}

void ChatgptManager::onLikeGptFinished(int state, int code, const QByteArray &data, QVariant opaque)
{
    LikeGptStruct likeGptStruct = opaque.value<LikeGptStruct>();
    bool success = false;
    if (RequestTask::RequestState::Success == (RequestTask::RequestState)state) {
        QJsonObject jsonObj, jsonObject;
        if (!data.isEmpty() && CommonUtils::bufferToJson(data, &jsonObj)) {
            if (jsonObj["code"] == 200) {
                success = true;
                jsonObject = jsonObj["data"].toObject();
            }
        }
    }
    if (!success) {
        likeGptStruct.rspGptStruct.like = likeGptStruct.oldLike;
        emit updateGptItem(likeGptStruct.index, likeGptStruct.rspGptStruct);
    }
}

void ChatgptManager::like(int index, const GptRspPDFStruct &gptRspPDFStruct, LikeEnum oldLike)
{
    if (!UserCenterManager::instance()->checkLoginStatus()) {
        return;
    }
    RequestTask *task  = new RequestTask(this);
    connect(task, &RequestTask::requestFinished, task, &RequestTask::deleteLater);
    connect(task, &RequestTask::finished, FrameWork::instance(), &FrameWork::onNetworkCheck);
    connect(task, &RequestTask::finished, this, &ChatgptManager::onLikePdfFinished);
    RequestTask::RequestStruct reqStruct;
    RequestResource::setSslConfig(&reqStruct.request);
    setCommonRawHeader(&reqStruct.request);

    reqStruct.request.setUrl(QUrl(ChatgptTypeDefine::aiServerHost() + "/chat/like"));
    reqStruct.type = RequestTask::RequestType::Post;
    QString replaceStr = QString("{\"id\":%1, \"like\":%2}").arg(gptRspPDFStruct.recordId).arg(int(gptRspPDFStruct.like));
    reqStruct.postData = replaceStr.toLatin1();
    task->setRequest(reqStruct);
    LikePDFStruct likePDFStruct;
    likePDFStruct.index = index;
    likePDFStruct.rspPdfStruct = gptRspPDFStruct;
    likePDFStruct.oldLike = oldLike;
    reqStruct.opaque.setValue(likePDFStruct);
    RequestManager::instance()->addTask(task, true);
}

void ChatgptManager::onLikePdfFinished(int state, int code, const QByteArray &data, QVariant opaque)
{
    LikePDFStruct likePDFStruct = opaque.value<LikePDFStruct>();
    bool success = false;
    if (RequestTask::RequestState::Success == (RequestTask::RequestState)state) {
        QJsonObject jsonObj, jsonObject;
        if (!data.isEmpty() && CommonUtils::bufferToJson(data, &jsonObj)) {
            if (jsonObj["code"] == 200) {
                success = true;
                jsonObject = jsonObj["data"].toObject();
            }
        }
    }
    if (!success) {
        likePDFStruct.rspPdfStruct.like = likePDFStruct.oldLike;
        emit updatePdfItem(likePDFStruct.index, likePDFStruct.rspPdfStruct);
    }
}
