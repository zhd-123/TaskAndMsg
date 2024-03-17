#ifndef REQUESTUTIL_H
#define REQUESTUTIL_H

#include <QObject>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QTimer>
#include <QVariant>
#include <QThread>
#include <QMutex>
#include "util_global.h"

#define MaxConcurrentRequestNum 20


//#define PRIVATE_DEPLOY
#define TEST_ENVIRONMENT
#ifdef TEST_ENVIRONMENT

#ifdef PRIVATE_DEPLOY
#define API_REQUEST_URL "http://124.71.18.102:9000"
#define ACCOUNT_REQUEST_URL "https://test-accounts.cn"
#define PAY_REQUEST_URL "https://test-pay.cn"
#else
#define API_REQUEST_URL "https://test-api.cn"
#define ACCOUNT_REQUEST_URL "https://test-accounts.cn"
#define PAY_REQUEST_URL "https://test-pay.cn"
#endif

#endif

class RequestUtil;
class HttpReplyTimer;

#define TimeoutErrorCode -11
#define AbortErrorCode -12
class UTIL_EXPORT IRequest : public QObject
{
    Q_OBJECT
public:
    enum RequestType
    {
        Get = 0,
        Post = 1,
        Head = 2,
    };
    enum RequestState
    {
        Success = 1,
        Failed = -1,
        Timeout = -11,
        Abort = -12,
    };
    struct RequestStruct {
        QNetworkRequest request;
        RequestType type;
        int timeout; //ms
        QByteArray postData;
        QVariant opaque;
        bool frameRead;
        RequestStruct() {
            timeout = 30000;
            type = RequestType::Get;
            frameRead = false;
        }
    };
    explicit IRequest(QObject *parent);
    virtual ~IRequest();
    virtual void setRequest(const RequestStruct &request) = 0;
    virtual void process() = 0;
signals:
    void requestFinished();
};

class UTIL_EXPORT RequestTask : public IRequest
{
    Q_OBJECT
public:
    explicit RequestTask(QObject *parent = nullptr);
    virtual ~RequestTask();
    virtual void setRequest(const RequestStruct &request);
    virtual void process();
    void abort();
    void setNetworkTip(bool tip);
    bool networkTip();
    void setForceTip(bool tip);
    bool forceTip();
signals:
    void rawHeaderMap(const QMap<QByteArray, QByteArray> &headerMap);
    void finished(int state, int code,const QByteArray &data, QVariant opaque);
    void uploadProgress(qint64 bytesSent, qint64 bytesTotal);
    void downloadProgress(qint64 bytesReceived, qint64 bytesTotal);
    void frameData(const QByteArray &data);
public slots:
    void onFinished(int state, int code, const QByteArray &data);
private:
    RequestStruct m_request;
    RequestUtil* m_requestUtil;
    QMutex mMutex;
    bool mNetworkTip;
    bool mForceTip;
};

class UTIL_EXPORT RequestManager : public QObject
{
    Q_OBJECT
public:
    static RequestManager* instance();
    ~RequestManager();
    void start();
    void release();
    void addTask(IRequest* request, bool highPriority = false);
    void abortTask(IRequest* request);
    void removeTask(IRequest* request);
    QNetworkAccessManager* networkAccessManager();

private:
    RequestManager(QObject *parent = nullptr);
    Q_INVOKABLE void runTask();
signals:
    void trigger();
public slots:
    void requestFinished();
    void onRequestThreadStart();
private:
    QThread     m_thread;
    int         m_runningRequestNum;
    QNetworkAccessManager *m_networkManager;
    QList<IRequest *> m_list;
    QMutex mMutex;
};

class UTIL_EXPORT RequestUtil : public QObject
{
    Q_OBJECT
public:
    explicit RequestUtil(QObject *parent = nullptr);
    ~RequestUtil();
    void post(const QNetworkRequest &request, const QByteArray &data, int msec);
    void get(const QNetworkRequest &request, int msec);
    void head(const QNetworkRequest &request, int msec);
    void abort();
    void setFrameRead(bool frameRead);
private:
    void connectReply(QNetworkReply *reply);
signals:
    void rawHeaderMap(const QMap<QByteArray, QByteArray> &headerMap);
    void finished(int state, int code, const QByteArray &data);
    void uploadProgress(qint64 bytesSent, qint64 bytesTotal);
    void downloadProgress(qint64 bytesReceived, qint64 bytesTotal);
    void frameData(const QByteArray &data);
public slots:
    void onFrameRead();
    void onReplyReadReady();
    void onReplyError(QNetworkReply::NetworkError code);
    void onReplyTimeout();
private:
    QNetworkReply *mRequestReply;
    HttpReplyTimer *m_replyTimer;
    bool m_replyTimeout;
    bool m_replyAbort;
    int m_timeout;
    bool mReleased;
    QMutex mMutex;
    QDateTime mReqTime;
    bool mFrameRead;
    QByteArray mFrameReadBuffer;
};

class UTIL_EXPORT HttpReplyTimer : public QObject
{
    Q_OBJECT
public:
    HttpReplyTimer(QNetworkReply *reply, int msec, QObject *parent = nullptr);
    void stop();
signals:
    void timeout();

private slots:
    void onTimeout();
private:
    QTimer m_timer;
    QNetworkReply *m_reply;
};

using ApiConfig = QMap<QString, QString>;
class UTIL_EXPORT RequestResource : public QObject
{
    Q_OBJECT
public:
    static QString apiUrl();
    static QString accountUrl();
    static QString payUrl();
    static QString getUrlLanguage();
    static QMap<QString, QString> commonRawHeaders(bool downloder);
    static void addCommonRawHeaders(QNetworkRequest *request, bool downloder=false, const QString &contentType = "application/json");
    static void setSslConfig(QNetworkRequest *request);
    static QString formatUrl(const QString &url, const QMap<QString, QString> &map);

    static RequestResource* instance();
    void requestConfig();
    QString getApiUrl(const QString &key);

    bool isApiUrl(const QString &key);
    bool isPayUrl(const QString &key);
    bool isAccountUrl(const QString &key);
    QString cloudServerHost();
    QString cloudEnterpriseServerHost();
    QString aiServerHost();
private:
    explicit RequestResource(QObject *parent = Q_NULLPTR);
    void setDefaultApiUrl();
    static bool isPrivateDeploy();
signals:
    void getRequestConfig();
public slots:
    void requestConfigFinished(int state, int code,const QByteArray &data, QVariant opaque);
private:
    ApiConfig mDefaultApiConfig;
    ApiConfig mApiConfig;
    QString mCloudServerHost;
    QString mAIServerHost;
    QString mEnterpriseCloudServerHost;
    QList<QString> mApiUrl;
    QList<QString> mPayUrl;
    QList<QString> mAccountUrl;
};

#endif // REQUESTUTIL_H
