#include "downloadmanager.h"
#include "../requestutil.h"
#include "../commonutils.h"
#include <QFile>

DownloadManager::DownloadManager(QObject *parent)
    : QObject(parent)
    , mStartDownload(false)
    , mInDownloading(false)
    , mIsDownloader(false)
{
}

void DownloadManager::setConfig(const QString &configFile, const QString &filePath, bool downloader)
{
    mIsDownloader = downloader;
    mDownloadConfigFile = configFile;
    mFilePath = filePath;
    DownloadConfig::instance()->setConfigFile(configFile);
    mConfigParam = DownloadConfig::instance()->readDownloadConfig();
}

void DownloadManager::startDownload(const QString &url, const QMap<QString, QString> &reqParmMap)
{
    if (mInDownloading) {
        qDebug() << "DownloadManager InDownloading...";
        return;
    }
    reset();
    mInDownloading = true;
    mFileListUrl = url;
    mReqParmMap = reqParmMap;

    requestFileList(mFileListUrl, mReqParmMap);
    mProgress = 5;
    emit progress(Downloading, mProgress);
}


void DownloadManager::downloadFile(const QStringList& urls)
{
    if(urls.isEmpty()){
        return ;
    }
    mIsGetFileList = true;
    mFileUrlList.clear();
    mFileUrlList = urls;
    mFileIndex = 0;
    requestFileInfo(mFileUrlList.at(mFileIndex));
}

bool DownloadManager::isStartDownload()
{
    return mStartDownload;
}

bool DownloadManager::isInDownloading()
{
    return mInDownloading;
}

void DownloadManager::stopDownload()
{
    abortSegmentDownload();
}

void DownloadManager::reset()
{
    mFileFailTimes = 0;
    mFileIndex = 0;
    mGetFileListTimes = 0;
    mGetFileInfoTimes = 0;
}

void DownloadManager::requestFileList(const QString &url, const QMap<QString, QString> &reqParmMap)
{
    mIsGetFileList = false;
    RequestTask *task  = new RequestTask(this);
    connect(task, &RequestTask::requestFinished, task, &RequestTask::deleteLater);
    connect(task, &RequestTask::finished, this, &DownloadManager::onRequestFileListFinished);

    RequestTask::RequestStruct reqStruct;
    RequestResource::setSslConfig(&reqStruct.request);
    RequestResource::addCommonRawHeaders(&reqStruct.request, mIsDownloader);

    // request param
    QString urlStr = RequestResource::formatUrl(url, reqParmMap);
    reqStruct.request.setUrl(QUrl(urlStr));
    reqStruct.type = RequestTask::RequestType::Get;
    task->setRequest(reqStruct);
    RequestManager::instance()->addTask(task, true);
}

void DownloadManager::onRequestFileListFinished(int state, int code,const QByteArray &data, QVariant opaque)
{
    if (RequestTask::RequestState::Success == (RequestTask::RequestState)state) {
        QJsonObject jsonObj;
        QJsonArray jsonArray;
        if (data.isEmpty() || !CommonUtils::bufferToJson(data, &jsonObj)) {
            // failed
            qDebug() << "onRequestFileListFinished is null";
            return;
        }
        if (jsonObj["data"].isArray()) {
            mIsGetFileList = true;
            mFileUrlList.clear();
            jsonArray = jsonObj["data"].toArray();
            for (int i = 0; i < jsonArray.size(); ++i) {
                mFileUrlList.append(jsonArray.at(i).toString());
            }
            requestFileInfo(mFileUrlList.at(mFileIndex));
        }
    } else {
        // failed
        qDebug() << "onRequestFileListFinished fail";
    }
    if (!mIsGetFileList) {
        // 重复获取
        if (++mGetFileListTimes <= MAX_RETRY_TIMES) {
            QTimer::singleShot(100, this, [=] {
                requestFileList(mFileListUrl, mReqParmMap);
            });
        } else {
            // tips error
            emit progress(DownloadFailed, mProgress);
            mInDownloading = false;
        }
    }
}

void DownloadManager::requestFileInfo(const QString &url)
{
    mIsGetFileInfo = false;
    RequestTask *task  = new RequestTask(this);
    connect(task, &RequestTask::requestFinished, task, &RequestTask::deleteLater);
    connect(task, &RequestTask::finished, this, &DownloadManager::onRequestFileInfoFinished);
    connect(task, &RequestTask::rawHeaderMap, this, &DownloadManager::onGetRawHeaderMap);

    RequestTask::RequestStruct reqStruct;
    RequestResource::setSslConfig(&reqStruct.request);
    RequestResource::addCommonRawHeaders(&reqStruct.request, mIsDownloader, "");
    // request param
    QMap<QString, QString> reqParmMap;
    QString urlStr = RequestResource::formatUrl(url, reqParmMap);
    reqStruct.request.setUrl(QUrl(urlStr));
    reqStruct.type = RequestTask::RequestType::Head;
    task->setRequest(reqStruct);
    task->setNetworkTip(false);
    RequestManager::instance()->addTask(task, true);
}

void DownloadManager::onGetRawHeaderMap(const QMap<QByteArray, QByteArray> &headerMap)
{
    if (!headerMap.contains("Content-Length") || !headerMap.contains("x-oss-hash-crc64ecma")) {
        return;
    }
    mCurPackageParam.totalSize =  headerMap.value("Content-Length").toLongLong();
    mCurPackageParam.crc = headerMap.value("x-oss-hash-crc64ecma").toULongLong();
    mCurPackageParam.url = mFileUrlList.at(mFileIndex);
}

void DownloadManager::onRequestFileInfoFinished(int state, int code,const QByteArray &data, QVariant opaque)
{
    if (RequestTask::RequestState::Success == (RequestTask::RequestState)state) {
        // enable install button
        mIsGetFileInfo = true;
        fileDownload();
    } else {
        // failed
        qDebug() << "onRequestFileInfoFinished fail";
    }
    if (!mIsGetFileInfo) {
        // 重复获取
        if (++mGetFileInfoTimes <= MAX_RETRY_TIMES) {
            mFileIndex = mGetFileInfoTimes%(mFileUrlList.size());
            QTimer::singleShot(100, this, [=] { requestFileInfo(mFileUrlList.at(mFileIndex)); });
        } else {
            // tips error
            emit progress(DownloadFailed, mProgress);
            mInDownloading = false;
        }
    }
}

void DownloadManager::fileDownload()
{
    qsrand(QTime(0,0,0).secsTo(QTime::currentTime()));
    int progressValue = 0;
    int count = 0;
    while (progressValue < 5) {
        progressValue = qrand() % 11;
        if (++count > 3) {
            progressValue = 6;
        }
    }
    mProgress = progressValue;
    emit progress(Downloading, mProgress);

    prepareForDownload();
    qDebug() << "start segment download" << mConfigParam.totalSize << mConfigParam.crc;
    startSegmentDownload();
}

void DownloadManager::prepareForDownload()
{
    QString setupFile = mFilePath;
    if (mConfigParam.totalSize != mCurPackageParam.totalSize || mConfigParam.crc != mCurPackageParam.crc) {
        if (QFile::exists(setupFile)) {
            QFile::remove(setupFile);
        }
        mConfigParam = mCurPackageParam;
        // cal segment
        mConfigParam.segmentCount = mConfigParam.totalSize / EACH_SEGMENT_SIZE;
        if (mConfigParam.totalSize % EACH_SEGMENT_SIZE != 0) {
            mConfigParam.segmentCount += 1;
        }
        mConfigParam.segmentArray.clear();
        for (int i = 0; i < mConfigParam.segmentCount; ++i) {
            SegmentParam param;
            param.index = i;
            param.startPos = i * EACH_SEGMENT_SIZE;
            if (i != mConfigParam.segmentCount - 1) {
                param.endPos = i * EACH_SEGMENT_SIZE + EACH_SEGMENT_SIZE - 1;
            } else {
                param.endPos = mConfigParam.totalSize - 1;
            }
            qDebug() << "segment info index:" << i << " startPos:" << param.startPos << " endPos:" << param.endPos;
            mConfigParam.segmentArray.append(param);
        }
        DownloadConfig::instance()->writeDownloadConfig(mConfigParam);
    }
    // create empty file
    if (!QFile::exists(setupFile)) {
        QFile file(setupFile);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            return;
        }
        file.close();
    }
}

void DownloadManager::startSegmentDownload()
{
    mRequestList.clear();
    mStartDownload = false;
    mAlreadyDownload = 0;
    // check if segment need download
    for (int i = 0; i < mConfigParam.segmentCount; ++i) {
        if (mConfigParam.segmentArray[i].startPos >= 0  && mConfigParam.segmentArray[i].startPos < mConfigParam.segmentArray[i].endPos) {
            mSegmentFail = false;
            // start segment
            segmentDownload(mConfigParam.url, i);
        }
    }
    if (mRequestList.size() <= 0) {
        // 包已经下载好
        downloadFinished();
    } else {
        mStartDownload = true;
    }
}

void DownloadManager::abortSegmentDownload()
{
    qDebug() << "abortSegmentDownload";
    for (RequestTask *task : mRequestList) {
        task->abort();
    }
    mRequestList.clear();
}

void DownloadManager::segmentDownload(const QString &url, int index)
{
    RequestTask *task = new RequestTask(this);
    connect(task, &RequestTask::requestFinished, task, &RequestTask::deleteLater);
    connect(task, &RequestTask::finished, this, [&](int state, int code,const QByteArray &data, QVariant opaque) {
        RequestTask *reqTask = qobject_cast<RequestTask *>(sender());
        if (reqTask) {
            qDebug() << "removeOne:" <<  reqTask;
            mRequestList.removeOne(reqTask);
        }
        onSegmentDownloadFinished(state, code, data, opaque);
    });
    connect(task, &RequestTask::downloadProgress, this, [&](qint64 bytesReceived, qint64 bytesTotal) {
        quint64 downloadSize = mConfigParam.downloadSize;
        downloadSize += bytesReceived;
        downloadSize = qMax(mAlreadyDownload, downloadSize);
        mAlreadyDownload = downloadSize;
        mProgress = qMax(mProgress, (int)(downloadSize * 1.0 / mConfigParam.totalSize * 90));
        emit progress(Downloading, mProgress);
    });
    RequestTask::RequestStruct reqStruct;
    RequestResource::setSslConfig(&reqStruct.request);
    RequestResource::addCommonRawHeaders(&reqStruct.request, mIsDownloader, "application/octet-stream");
    reqStruct.request.setRawHeader("Range", "bytes="+ QString::number(mConfigParam.segmentArray[index].startPos).toLatin1() + "-"
                                   + QString::number(mConfigParam.segmentArray[index].endPos).toLatin1());
    // request param
    QMap<QString, QString> reqParmMap;
    QString urlStr = RequestResource::formatUrl(url, reqParmMap);
    reqStruct.request.setUrl(QUrl(urlStr));
    reqStruct.type = RequestTask::RequestType::Get;
    reqStruct.timeout = 60 * 60 * 1000 * 12;
    reqStruct.opaque.setValue(index);
    task->setRequest(reqStruct);
    task->setNetworkTip(false);
    RequestManager::instance()->addTask(task, false);
    qDebug() << "append:" <<  task;
    mRequestList.append(task);
}

void DownloadManager::retryDownload()
{
    // 判断下载次数，选择index
    int downloadTimes = mFileFailTimes + 1;
    mFileIndex = downloadTimes%(mFileUrlList.size());
    requestFileInfo(mFileUrlList.at(mFileIndex));
}

void DownloadManager::segmentFinish(int index, bool success, int state)
{
    QString text = success == true ? "success" : "failed";
    qDebug() << "segmentFinish index:" << QString::number(index) << " status:" << text;
    if (!success) {
        if (AbortErrorCode != state && !mSegmentFail) {// 每次分段下载，只进入一次
            mSegmentFail = true;
            mFileFailTimes++;
            abortSegmentDownload();
            if (mFileFailTimes <= MAX_RETRY_TIMES) {
                QTimer::singleShot(1000 * 2, this, [=] { retryDownload(); });
            } else {
                // tips failed
                emit progress(DownloadFailed, mProgress);
                mInDownloading = false;
                return;
            }
        }
    } else {
        downloadFinished();
    }
}

void DownloadManager::onSegmentDownloadFinished(int state, int code,const QByteArray &data, QVariant opaque)
{
    int index = opaque.toInt();
    if (RequestTask::RequestState::Success == (RequestTask::RequestState)state) {
        bool openfileSuccess = false;
        {
            QMutexLocker locker(&mMutex);
            QFile file(mFilePath);
            if (file.open(QIODevice::WriteOnly | QIODevice::Append)) {
                openfileSuccess = true;
                file.seek(mConfigParam.segmentArray[index].startPos);
                file.write(data);
                file.close();

                int dataLen = data.length();
                int startPos = mConfigParam.segmentArray[index].startPos;
                mConfigParam.segmentArray[index].startPos = startPos + dataLen - 1;
                mConfigParam.downloadSize += dataLen;
                qDebug() << index << " datalen:" << dataLen;
                DownloadConfig::instance()->writeDownloadConfig(mConfigParam);

//                mProgress = mConfigParam.downloadSize * 1.0 / mConfigParam.totalSize * 90;
//                emit progress(Downloading, mProgress);
                segmentFinish(index, true, state);
            } else {
                qDebug() << index << " openfile fail";
            }
        }
        if (!openfileSuccess) {
            QTimer::singleShot(20, this, [=] { onSegmentDownloadFinished(state, code, data, opaque); });
        }
    } else {
        qDebug() << index << " failed";
        segmentFinish(index, false, state);
    }
}

void DownloadManager::downloadFinished()
{
    mInDownloading = false;
    if (verifyFile()) {
        mProgress = 100;
        emit progress(DownloadSuccess, mProgress);
    }
}

bool DownloadManager::verifyFile()
{
    if (mConfigParam.downloadSize >= mConfigParam.totalSize) {
        mProgress = 95;
        emit progress(Downloading, mProgress);
        // check crc
        QString setupFile = mFilePath;
        QFile file(setupFile);
        quint64 crc = 0;
        if (file.open(QIODevice::ReadOnly)) {
            QByteArray ba = file.readAll();
            crc = CalcCRC64(0, ba.data(), ba.size());
            file.close();
        }

        if (mConfigParam.crc != crc) {
            // tips failed
            if (QFile::exists(setupFile)) {
                QFile::remove(setupFile);
            }
            QFile::remove(mDownloadConfigFile);
            qDebug() << "verifyFile crc no eq " << mConfigParam.crc << " " << crc;

            emit progress(DownloadFailed, mProgress);
            return false;
        }
        return true;
    }
    return false;
}
