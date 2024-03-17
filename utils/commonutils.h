#ifndef COMMONUTILS_H
#define COMMONUTILS_H

#include "util_global.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>
#include <Windows.h>

class UTIL_EXPORT CommonUtils
{
public:
    CommonUtils();
    static bool bufferToJson(const QByteArray &buffer, QJsonObject *jsonObject);
    static void mkDirRecursive(const QString &path);
    static void rmDirRecursive(const QString &dirPath);
    static QString appWritableDataDir();

    static bool checkIsRunning(const QString &program);
    static QList<DWORD> getPid(const QString &program);
    static BOOL killProcess(DWORD pid);
    static void killProcessCmd(DWORD pid);
    static void killAllProcess(const QString &program);
    static bool checkProcessByName(const QString &strExe);
private:
    static void Wchar_tToString(std::string& szDst, wchar_t *wchar);
};
#endif // COMMONUTILS_H
