#include "requestutil.h"
#include "langutil.h"
#include <QDir>
#include <QSettings>
#include <QMutexLocker>
#include <QCoreApplication>
#include <QJsonParseError>
#include <QJsonDocument>
#include <QHostInfo>
#include "settingutil.h"
#include "channel/channeldata.h"
#include "commonutils.h"

IRequest::IRequest(QObject *parent)
    : QObject(parent)
{

}

IRequest::~IRequest()
{

}
RequestTask::RequestTask(QObject *parent)
    : IRequest(parent)
    , m_requestUtil(nullptr)
    , mNetworkTip(true)
    , mForceTip(false)
{
}

RequestTask::~RequestTask()
{
}

void RequestTask::setRequest(const RequestStruct &request)
{
    m_request = request;
}

void RequestTask::process()
{
    m_requestUtil = new RequestUtil();
    m_requestUtil->setFrameRead(m_request.frameRead);
    connect(m_requestUtil, &RequestUtil::frameData, this, &RequestTask::frameData);
    connect(m_requestUtil, &RequestUtil::finished, this, &RequestTask::onFinished);
    connect(m_requestUtil, &RequestUtil::rawHeaderMap, this, &RequestTask::rawHeaderMap);
    connect(m_requestUtil, &RequestUtil::downloadProgress, this, &RequestTask::downloadProgress);
    connect(m_requestUtil, &RequestUtil::uploadProgress, this, &RequestTask::uploadProgress);
    if (RequestType::Get == m_request.type) {
        m_requestUtil->get(m_request.request, m_request.timeout);
    }
    else if (RequestType::Post == m_request.type) {
        m_requestUtil->post(m_request.request, m_request.postData, m_request.timeout);
    }
    else if (RequestType::Head == m_request.type) {
        m_requestUtil->head(m_request.request, m_request.timeout);
    }
}

void RequestTask::abort()
{
    QMutexLocker locker(&mMutex);
    if (m_requestUtil) {
        m_requestUtil->abort();
        RequestManager::instance()->abortTask(this);
    } else {
        RequestManager::instance()->removeTask(this);
    }
}

void RequestTask::setNetworkTip(bool tip)
{
    mNetworkTip = tip;
}

bool RequestTask::networkTip()
{
    return mNetworkTip;
}

void RequestTask::setForceTip(bool tip)
{
    mForceTip = tip;
}

bool RequestTask::forceTip()
{
    return mForceTip;
}

void RequestTask::onFinished(int state, int code, const QByteArray &data)
{
    QMutexLocker locker(&mMutex);
    m_requestUtil->deleteLater();
    m_requestUtil = nullptr;

    emit finished(state, code, data, m_request.opaque);
    emit requestFinished();
}

RequestManager::RequestManager(QObject *parent)
    : QObject(parent)
    , m_networkManager(nullptr)
    , m_runningRequestNum(0)
{
    setParent(nullptr);
    connect(this, &RequestManager::trigger, this, &RequestManager::runTask);
    moveToThread(&m_thread);
    connect(&m_thread, &QThread::started, this, &RequestManager::onRequestThreadStart);

    qRegisterMetaType<QMap<QByteArray, QByteArray>>("const QMap<QByteArray, QByteArray> &");
    start();
}

RequestManager *RequestManager::instance()
{
    static RequestManager instance;
    return &instance;
}

RequestManager::~RequestManager()
{
}

void RequestManager::start()
{
    m_thread.start();
}

void RequestManager::release()
{
    m_list.clear();
    m_networkManager->deleteLater(); // 跨线程delete
    QTimer timer;
    timer.setSingleShot(true);
    timer.setInterval(100);
    QEventLoop loop;
    connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    timer.start();
    loop.exec();
    timer.stop();
    if (m_thread.isRunning())
    {
        m_thread.terminate();
        m_thread.wait(5000);
    }
}

void RequestManager::addTask(IRequest *request, bool highPriority)
{
    connect(request, &IRequest::requestFinished, this, &RequestManager::requestFinished);
    QMutexLocker locker(&mMutex);
    if (highPriority) {
        m_list.push_front(request);
    } else {
        m_list.push_back(request);
    }
    QMetaObject::invokeMethod(this, "runTask");
}

void RequestManager::abortTask(IRequest *request)
{
    m_runningRequestNum--;
    if (m_runningRequestNum < 0) {
        m_runningRequestNum = 0;
    }
}

void RequestManager::removeTask(IRequest *request)
{
    QMutexLocker locker(&mMutex);
    m_list.removeAll(request);
}

QNetworkAccessManager *RequestManager::networkAccessManager()
{
    return m_networkManager;
}

void RequestManager::onRequestThreadStart()
{
    m_networkManager = new QNetworkAccessManager(this);
}

void RequestManager::runTask()
{
//    if (m_runningRequestNum >= MaxConcurrentRequestNum) {
//        return;
//    }
    IRequest *request = nullptr;
    QMutexLocker locker(&mMutex);
    if (m_list.empty()) {
        return;
    }
    if (request = m_list.first())
    {
        m_list.removeAll(request);
        m_runningRequestNum++;
        request->process();
    }
}

void RequestManager::requestFinished()
{
    m_runningRequestNum--;
    if (m_runningRequestNum < 0) {
        m_runningRequestNum = 0;
    }
    QMetaObject::invokeMethod(this, "runTask");
}

RequestUtil::RequestUtil(QObject *parent)
    : QObject(parent)
    , m_replyTimer(nullptr)
    , mRequestReply(nullptr)
    , m_replyTimeout(false)
    , m_replyAbort(false)
    , mReleased(false)
    , mFrameRead(false)
{
}

RequestUtil::~RequestUtil()
{
    mReleased = true;
}

void RequestUtil::post(const QNetworkRequest &request, const QByteArray &data, int msec)
{
    qWarning() << QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss:zzz") << " " << request.url();
    mRequestReply = RequestManager::instance()->networkAccessManager()->post(request, data);
    connectReply(mRequestReply);
    m_timeout = msec;
    mReqTime = QDateTime::currentDateTime();
    m_replyTimer = new HttpReplyTimer(mRequestReply, msec);
    connect(m_replyTimer, &HttpReplyTimer::timeout, this, &RequestUtil::onReplyTimeout);
}

void RequestUtil::get(const QNetworkRequest &request, int msec)
{
    qWarning() << QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss:zzz") << " " << request.url();
    mRequestReply = RequestManager::instance()->networkAccessManager()->get(request);
    connectReply(mRequestReply);
    m_timeout = msec;
    mReqTime = QDateTime::currentDateTime();
    m_replyTimer = new HttpReplyTimer(mRequestReply, msec);
    connect(m_replyTimer, &HttpReplyTimer::timeout, this, &RequestUtil::onReplyTimeout);
}

void RequestUtil::head(const QNetworkRequest &request, int msec)
{
    qWarning() << QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss:zzz") << " " << request.url();
    mRequestReply = RequestManager::instance()->networkAccessManager()->head(request);
    connectReply(mRequestReply);
    m_timeout = msec;
    mReqTime = QDateTime::currentDateTime();
    m_replyTimer = new HttpReplyTimer(mRequestReply, msec);
    connect(m_replyTimer, &HttpReplyTimer::timeout, this, &RequestUtil::onReplyTimeout);
}

void RequestUtil::abort()
{
    QMutexLocker locker(&mMutex);
    if (mRequestReply && !mReleased) {
        m_replyAbort = true;
        mRequestReply->abort();
    }
}

void RequestUtil::setFrameRead(bool frameRead)
{
    mFrameRead = frameRead;
}

void RequestUtil::connectReply(QNetworkReply *reply)
{
    if (mFrameRead) {
        connect(reply, &QNetworkReply::readyRead, this, &RequestUtil::onFrameRead);
    }
    connect(reply, &QNetworkReply::finished, this, &RequestUtil::onReplyReadReady);
    connect(reply, static_cast<void(QNetworkReply::*)(QNetworkReply::NetworkError)>(&QNetworkReply::error), this, &RequestUtil::onReplyError);
    connect(reply, &QNetworkReply::uploadProgress, this, &RequestUtil::uploadProgress);
    connect(reply, &QNetworkReply::downloadProgress, this, &RequestUtil::downloadProgress);
}

void RequestUtil::onFrameRead()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply || !m_replyTimer) {
        return;
    }
    QByteArray ba = reply->readAll();
    mFrameReadBuffer.append(ba);
    emit frameData(mFrameReadBuffer);
}

void RequestUtil::onReplyReadReady()
{
    QMutexLocker locker(&mMutex);
    auto releaseReply = [&](QNetworkReply *reply) {
        reply->close();
        reply->deleteLater();
        m_replyTimer->deleteLater();
        m_replyTimer = nullptr;
        mRequestReply = nullptr;
    };
    if (!m_replyTimeout && m_replyTimer) {
        m_replyTimer->stop();
    }
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply || !m_replyTimer) {
        return;
    }
    if (m_replyTimeout)
    {
        emit finished(IRequest::RequestState::Timeout, TimeoutErrorCode, QByteArray());
        qWarning() << QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss:zzz") << " " << reply->url() << " Request::Timeout mSeconds:" << mReqTime.msecsTo(QDateTime::currentDateTime());
    }
    else if (m_replyAbort)
    {
        emit finished(IRequest::RequestState::Abort, AbortErrorCode, QByteArray());
        qWarning() << QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss:zzz") << " " << reply->url() << " Request::Abort mSeconds:" << mReqTime.msecsTo(QDateTime::currentDateTime());
    }
    else
    {
        int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        //获取响应的信息，状态码为200表示正常
        if ((statusCode == 200 || statusCode == 206 || statusCode == 204) && reply->error() == QNetworkReply::NoError) {
            QMap<QByteArray, QByteArray> headerMap;
            for (auto pair : reply->rawHeaderPairs()) {
                headerMap.insert(pair.first, pair.second);
            }
            emit rawHeaderMap(headerMap);
            QByteArray ba = reply->readAll();
            if (mFrameRead && !mFrameReadBuffer.isEmpty()) {
                ba = mFrameReadBuffer;
            }
            emit finished(IRequest::RequestState::Success, statusCode, ba);
            // 判断不是图片或文件才打印
            if (statusCode != 206) {
                QJsonParseError jsonPE;
                QJsonDocument jsonDoc = QJsonDocument::fromJson(ba, &jsonPE);
                if (jsonPE.error == QJsonParseError::NoError) {
                    qWarning() << QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss:zzz") << " " << reply->url() << " response:" << ba << " mSeconds:" << mReqTime.msecsTo(QDateTime::currentDateTime());
                } else {
                    qWarning() << QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss:zzz") << " " << reply->url() << " mSeconds:" << mReqTime.msecsTo(QDateTime::currentDateTime());
                }
            }
        }
        else if (statusCode == 301 || statusCode == 302) {
            //redirect
            QVariant redirectionTargetUrl = reply->attribute(QNetworkRequest::RedirectionTargetAttribute);
            releaseReply(reply);
            get(QNetworkRequest(QUrl(redirectionTargetUrl.toString())), m_timeout);
            qInfo() << reply->url() << " redirectionTargetUrl" << redirectionTargetUrl;
            return;
        }
        else {
            emit finished(IRequest::RequestState::Failed, statusCode, QByteArray());
            qWarning() << QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss:zzz") << " " << reply->url() << " Request::Failed mSeconds:" << mReqTime.msecsTo(QDateTime::currentDateTime());
        }
    }
    releaseReply(reply);
}

void RequestUtil::onReplyError(QNetworkReply::NetworkError code)
{
    // log error code
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    qWarning() << QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss:zzz") << " " << reply->url() << " onReplyError:" << code;
}

void RequestUtil::onReplyTimeout()
{
    m_replyTimeout = true;
}

HttpReplyTimer::HttpReplyTimer(QNetworkReply *reply, int msec , QObject *parent)
    : QObject(parent)
    , m_reply(reply)
{
    Q_ASSERT(reply);
    if (reply && reply->isRunning()) {  // 启动单次定时器
        m_timer.setSingleShot(true);
        connect(&m_timer, &QTimer::timeout, this, &HttpReplyTimer::onTimeout);
        m_timer.start(msec);
    }
}

void HttpReplyTimer::stop()
{
    m_timer.stop();
}

void HttpReplyTimer::onTimeout()
{
    stop();
    if (m_reply && m_reply->isRunning()) {
        emit timeout();

        m_reply->abort();
    }
}

bool RequestResource::isPrivateDeploy()
{
    FrameParamConfig frameParam;
    return frameParam.isPrivateDeploy();
}

QString RequestResource::apiUrl()
{
    QString url = API_REQUEST_URL;
    if (!SettingUtil::isTestEnvironment()) {
        if (isPrivateDeploy()) {
            return SettingUtil::privateDeployServerIp();
        }
        // diff by location
        ChannelData channelData;
        if (channelData.isChineseChannel(channelData.localChannelId()))
        {
            url = "";
        }
        else
        {
            url = "";
        }
    }
    return url;
}

QString RequestResource::accountUrl()
{
    QString url = ACCOUNT_REQUEST_URL;
    if (!SettingUtil::isTestEnvironment()) {
        // diff by location
        ChannelData channelData;
        if (channelData.isChineseChannel(channelData.localChannelId()))
        {
            url = "";
        }
        else
        {
            url = "";
        }
    }
    return url;
}

QString RequestResource::payUrl()
{
    QString url = PAY_REQUEST_URL;
    if (!SettingUtil::isTestEnvironment()) {
        // diff by location
        ChannelData channelData;
        if (channelData.isChineseChannel(channelData.localChannelId()))
        {
            url = "";
        }
        else
        {
            url = "";
        }
    }
    return url;
}

QString RequestResource::getUrlLanguage()
{
    QString language = "zh-CN";
    QString lang= SettingUtil::language();
    if (lang.isEmpty()) {
        lang = LangUtil::localeCountry();
    }
    QLocale::Language localLang = LangUtil::fromlocalCode(lang);
    language = LangUtil::langLocaleCode(localLang);
    return language;
}

QMap<QString, QString> RequestResource::commonRawHeaders(bool downloder)
{
    QMap<QString, QString> map;
    map.insert("Authorization", "");
    map.insert("Device-Type", "WIN");
    map.insert("Device-Id", SettingUtil::uid().toLatin1());
    map.insert("Accept-Language", getUrlLanguage());
    map.insert("Product-Id", QString::number(25646).toLatin1());
    if (!isPrivateDeploy()) {
        ChannelData channelData;
        if (downloder) {
            map.insert("sub-pid", channelData.getProductID().toLatin1());
        } else {
            map.insert("sub-pid", channelData.localChannelId().toLatin1());
        }
    }

    map.insert("System-Ver", QSysInfo::productVersion().toLatin1());
    map.insert("Device-Name", QHostInfo::localHostName().toLatin1());
    return map;
}

void RequestResource::addCommonRawHeaders(QNetworkRequest *request, bool downloder, const QString &contentType)
{
    QMap<QString, QString> headerMap = commonRawHeaders(downloder);
    for (QString header : headerMap.keys()) {
        request->setRawHeader(header.toLatin1(), headerMap.value(header).toLatin1());
    }
    if (!contentType.isEmpty()) {
        request->setRawHeader("Content-Type", contentType.toLatin1());
    }
}

void RequestResource::setSslConfig(QNetworkRequest *request)
{
    QSslConfiguration config;
    config.setPeerVerifyMode(QSslSocket::VerifyNone);
    config.setProtocol(QSsl::TlsV1SslV3);
    request->setSslConfiguration(config);
}

QString RequestResource::formatUrl(const QString &url, const QMap<QString, QString> &map)
{
    QString strUrl = url;
    if (!map.isEmpty()) {
        strUrl += "?";
        for (QString key : map.keys()) {
            strUrl += QString("%1=%2&").arg(key).arg(map[key]);
        }
        strUrl.chop(1);
    }
    return strUrl;
}

RequestResource *RequestResource::instance()
{
    static RequestResource inst;
    return &inst;
}

RequestResource::RequestResource(QObject *parent)
    : QObject(parent)
{
    setDefaultApiUrl();
}

void RequestResource::requestConfig()
{
    RequestTask *task  = new RequestTask(this);
    connect(task, &RequestTask::requestFinished, task, &RequestTask::deleteLater);
    connect(task, &RequestTask::finished, this, &RequestResource::requestConfigFinished);

    RequestTask::RequestStruct reqStruct;

    RequestResource::setSslConfig(&reqStruct.request);
    RequestResource::addCommonRawHeaders(&reqStruct.request);
//    reqStruct.request.setRawHeader("Ver", APP_VERSION);

    // request param
    QMap<QString, QString> reqParmMap;
    QString funKey = "/v1/common/apiConfig";
    if (isPrivateDeploy()) {
        funKey = "/api/common/apiConfig";
    }
    QString urlStr = RequestResource::formatUrl(RequestResource::apiUrl() + funKey, reqParmMap);
    reqStruct.request.setUrl(QUrl(urlStr));
    task->setRequest(reqStruct);
    task->setNetworkTip(false);
    RequestManager::instance()->addTask(task, true);
}

void RequestResource::requestConfigFinished(int state, int code,const QByteArray &data, QVariant opaque)
{
    if (RequestTask::RequestState::Success == (RequestTask::RequestState)state) {
        mApiConfig.clear();
        auto varMap = QJsonDocument::fromJson(data).toVariant().toMap()["data"].toMap();
        for (QString key : varMap.keys()) {
            mApiConfig.insert(key, varMap[key].toString());
            if (key == "drive_api") {
                mCloudServerHost = varMap[key].toString();
            } else if (key == "enterprise_drive_api") {
                mEnterpriseCloudServerHost = varMap[key].toString();
            } else if (key == "ai_api") {
                mAIServerHost = varMap[key].toString();
            }
        }
    }
    emit getRequestConfig();
}

void RequestResource::setDefaultApiUrl()
{
    mDefaultApiConfig.insert("checkUpdateApi", "/v1/common/checkUpdate");
    mApiUrl.append("checkUpdateApi");
    mDefaultApiConfig.insert("checkUsernameExists", "/v1/user/checkUsernameExists");
    mApiUrl.append("checkUsernameExists");
    mDefaultApiConfig.insert("getAuthCodeApi", "/v1/user/getAuthCode");   // 获取跳转浏览器临时凭证接口
    mApiUrl.append("getAuthCodeApi");
    mDefaultApiConfig.insert("getUserInfoApi", "/v1/user/getUserInfo");
    mApiUrl.append("getUserInfoApi");
    mDefaultApiConfig.insert("googleLoginCallback", "/v1/user/googleLoginCallback");
    mApiUrl.append("googleLoginCallback");
    mDefaultApiConfig.insert("activeConfigApi", "/v1/common/activeConfig"); // 是否显示活动入口
    mApiUrl.append("activeConfigApi");
    mDefaultApiConfig.insert("planUseApi", "/v1/user/planUse");        // 激活赠送plan
    mApiUrl.append("planUseApi");
    mDefaultApiConfig.insert("activeSubscriptionApi", "/v1/user/activeSubscription"); // 续订plan
    mApiUrl.append("activeSubscriptionApi");
    mDefaultApiConfig.insert("getPlanApi", "/v1/user/plan");           // 获取用户plan信息
    mApiUrl.append("getPlanApi");
    mDefaultApiConfig.insert("jumpUrl", "/v1/common/go");           // 通用跳转链接
    mApiUrl.append("jumpUrl");
    mDefaultApiConfig.insert("logoutApi", "/v1/user/logout");
    mApiUrl.append("logoutApi");
    mDefaultApiConfig.insert("login", "/v1/user/login");
    mApiUrl.append("login");
    mDefaultApiConfig.insert("reportLogin", "/v1/user/reportLogin");
    mApiUrl.append("reportLogin");
    mDefaultApiConfig.insert("getDownloaderUrlApi", "/v1/common/getDownloaderUrl");   // 获取安装包url
    mApiUrl.append("getDownloaderUrlApi");
    mDefaultApiConfig.insert("getOcrUrlApi", "/v1/common/getOcrUrl");   // 获取安装包url
    mApiUrl.append("getOcrUrlApi");
    mDefaultApiConfig.insert("marketingActivitiesApi", "/v1/common/marketingActivities");   // 获取活动
    mApiUrl.append("marketingActivitiesApi");
    mDefaultApiConfig.insert("refreshTokenApi", "/v1/user/refreshToken");   // 主动刷新token
    mApiUrl.append("refreshTokenApi");
    mDefaultApiConfig.insert("getEmployeeAuthApi", "/v1/user/getEmployeeAuth");
    mApiUrl.append("getEmployeeAuthApi");
    mDefaultApiConfig.insert("getDeviceListApi", "/v1/user/getDeviceList");
    mApiUrl.append("getDeviceListApi");
    mDefaultApiConfig.insert("multiBindDeviceApi", "/v1/user/multiBindDevice");
    mApiUrl.append("multiBindDeviceApi");
    mDefaultApiConfig.insert("getPersonalDeviceListApi", "/v1/user/getPersonalDeviceList");
    mApiUrl.append("getPersonalDeviceListApi");
    mDefaultApiConfig.insert("multiBindPersonalDeviceApi", "/v1/user/multiBindPersonalDevice");
    mApiUrl.append("multiBindPersonalDeviceApi");

    mDefaultApiConfig.insert("loginUrl", "/index/login/");
    mAccountUrl.append("loginUrl");
    mDefaultApiConfig.insert("accountCenterUrl", "/index/accountCenter/");           // 获取用户中心
    mAccountUrl.append("accountCenterUrl");
    mDefaultApiConfig.insert("companyManagementUrl", "/index/companyManagement/");           // 获取用户中心
    mAccountUrl.append("companyManagementUrl");

    mDefaultApiConfig.insert("checkoutUrl", "/updf");
    mPayUrl.append("checkoutUrl");
}

QString RequestResource::getApiUrl(const QString &key)
{
    if (mApiConfig.contains(key)) {
        return mApiConfig.value(key);
    }

    if (isPrivateDeploy()) {
        return "";
    }
    QString host = "";
    if (isApiUrl(key)) {
        host = apiUrl();
    } else if (isAccountUrl(key)) {
        host = accountUrl();
    } else if (isPayUrl(key)){
        host = payUrl();
    }
    if (mDefaultApiConfig.contains(key)) {
        if (isAccountUrl(key)) {
            QString lang = LangUtil::localeCountry();
            return host + mDefaultApiConfig.value(key) + LangUtil::langLocaleCode(LangUtil::fromlocalCode(lang));
        } else {
            return host + mDefaultApiConfig.value(key);
        }
    }
    return "";
}

bool RequestResource::isApiUrl(const QString &key)
{
    return mApiUrl.contains(key);
}

bool RequestResource::isPayUrl(const QString &key)
{
    return mPayUrl.contains(key);
}

bool RequestResource::isAccountUrl(const QString &key)
{
    return mAccountUrl.contains(key);
}

QString RequestResource::cloudServerHost()
{
    if (mCloudServerHost.isEmpty()) {
        if (!SettingUtil::isTestEnvironment()) {
            // diff by location
            ChannelData channelData;
            if (channelData.isChineseChannel(channelData.localChannelId()))
            {
                mCloudServerHost = "https://apis.updf.cn/v1/drive";
            }
            else
            {
                mCloudServerHost = "https://apis.updf.com/v1/drive";
            }
        } else {
            mCloudServerHost = "https://test-api.updf.cn/v1/drive";
        }
    }
    return mCloudServerHost;
}

QString RequestResource::cloudEnterpriseServerHost()
{
    if (mEnterpriseCloudServerHost.isEmpty()) {
        if (!SettingUtil::isTestEnvironment()) {
            // diff by location
            ChannelData channelData;
            if (channelData.isChineseChannel(channelData.localChannelId()))
            {
                mEnterpriseCloudServerHost = "https://apis.updf.cn/v1/company/drive";
            }
            else
            {
                mEnterpriseCloudServerHost = "https://apis.updf.com/v1/company/drive";
            }
        } else {
            mEnterpriseCloudServerHost = "https://test-api.updf.cn/v1/company/drive";
        }
    }
    return mEnterpriseCloudServerHost;
}

QString RequestResource::aiServerHost()
{
    if (mAIServerHost.isEmpty()) {
        mAIServerHost = "https://apis.updf.com/v1/ai";
//        mAIServerHost = "https://zga-apis.updf.com/v1/ai";
    }
    return mAIServerHost;
}
