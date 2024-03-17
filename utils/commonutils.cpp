#include "commonutils.h"
#include <QDir>
#include <QStandardPaths>
#include <fcntl.h>
#include <QProcess>
#include <QEventLoop>
#include <QApplication>
#include <windows.h>
#include <tlhelp32.h>

CommonUtils::CommonUtils()
{

}

bool CommonUtils::bufferToJson(const QByteArray &buffer, QJsonObject *jsonObject)
{
    QJsonParseError jsonPE;
    QJsonDocument jsonDoc = QJsonDocument::fromJson(buffer, &jsonPE);
    if (jsonPE.error == QJsonParseError::NoError) {
        if (jsonDoc.isObject()) {
            *jsonObject = jsonDoc.object();
            return true;
        }
    }
    return false;
}

void CommonUtils::mkDirRecursive(const QString &path)
{
    QString newPath = path;
    if (newPath.contains("\\")) {
        newPath.replace("\\", "/");
    }
    if (!newPath.endsWith("/")) {
        newPath.append("/");
    }
    int from = newPath.indexOf("/");
    while (from >= 0) {
        from = newPath.indexOf("/", from + 1);
        QString dirPath = newPath.left(newPath.indexOf("/", from) + 1);
        QDir dir(dirPath);
        if (!dir.exists()) {
            dir.mkdir(dirPath);
        }
    }
}

void CommonUtils::rmDirRecursive(const QString &dirPath)
{
    if (!QDir(dirPath).exists()) {
        return;
    }
    QDir dir(dirPath);
    dir.setFilter(QDir::AllEntries | QDir::NoDotAndDotDot);
    QFileInfoList fileList = dir.entryInfoList();
    foreach (QFileInfo file, fileList) {
        if (file.isFile()) {
            QFile::remove(file.absoluteFilePath());
        } else if (file.isDir()) {
            rmDirRecursive(file.absoluteFilePath());
        }
    }
    dir.rmdir(dir.absolutePath());
}

QString CommonUtils::appWritableDataDir()
{
    QString dataDir = QStandardPaths::writableLocation(QStandardPaths::DataLocation);
    QDir dir(dataDir);
    dir.cdUp();
    QString configDir = dir.path()  + QDir::separator() + "UPDF";
    if (!dir.exists(configDir)) {
        dir.mkdir(configDir);
    }
    return QDir::toNativeSeparators(configDir);
}
void CommonUtils::Wchar_tToString(std::string& szDst, wchar_t *wchar)
{
    wchar_t * wText = wchar;
    DWORD dwNum = WideCharToMultiByte(CP_OEMCP,NULL,wText,-1,NULL,0,NULL,FALSE);// WideCharToMultiByte的运用
    char *psText; // psText为char*的临时数组，作为赋值给std::string的中间变量
    psText = new char[dwNum];
    WideCharToMultiByte (CP_OEMCP,NULL,wText,-1,psText,dwNum,NULL,FALSE);// WideCharToMultiByte的再次运用
    szDst = psText;// std::string赋值
    delete []psText;// psText的清除
}

bool CommonUtils::checkProcessByName(const QString &strExe)
{
    bool bResult = false;
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (INVALID_HANDLE_VALUE == hSnapshot) {
        return false;
    }
    PROCESSENTRY32 pe = { sizeof(pe) };
    for (BOOL ret = Process32First(hSnapshot, &pe); ret; ret = Process32Next(hSnapshot, &pe)) {
        wchar_t* process_str = pe.szExeFile;
        std::string current_process_name;
        Wchar_tToString(current_process_name, process_str);
        if (current_process_name == strExe.toStdString()) {
            bResult = true;
            break;
        }
    }
    CloseHandle(hSnapshot);
    return bResult;
}

bool CommonUtils::checkIsRunning(const QString &program)// "UPDF.exe"
{
    QProcess process;
    QString param = "imagename eq %1";
    process.start("tasklist", QStringList() << "-fi" << param.arg(program));
    QEventLoop loop;
    QObject::connect(&process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), &loop, &QEventLoop::quit);
    loop.exec();
    process.waitForFinished(5000);
    QString info = QString::fromLocal8Bit(process.readAllStandardOutput());
    if (info.contains("PID") && info.contains(program)) {
        return true;
    } else {
        return false;
    }
}

QList<DWORD> CommonUtils::getPid(const QString &program)
{
    QList<DWORD> pidList;
    QProcess process;
    QString param = "imagename eq %1";
    process.start("tasklist", QStringList() << "-fi" << param.arg(program));
    QEventLoop loop;
    QObject::connect(&process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), &loop, &QEventLoop::quit);
    loop.exec();
    process.waitForFinished(5000);
    char line[1024] = {0};
    while (process.canReadLine()) {
        process.readLine(line, sizeof(line));
        QString lineString = line;
        QList<QString> listString;
        if (lineString.startsWith(program)) {
            listString = lineString.split(" ", QString::SkipEmptyParts);
            if (listString.size() >= 2) {
                pidList.append(listString.at(1).toInt());
            }
        }
    }
    return pidList;
}

BOOL CommonUtils::killProcess(DWORD pid)
{
    HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
    if (hProcess == nullptr)
        return FALSE;
    if (!TerminateProcess(hProcess, 0)) {
        killProcessCmd(pid);
    }
    return TRUE;
}

void CommonUtils::killProcessCmd(DWORD pid)
{
    QProcess process;
    QString param = "taskkill /pid %1 /t /f";
    process.start("cmd", QStringList() << "/c" << param.arg(pid));
    QEventLoop loop;
    QObject::connect(&process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), &loop, &QEventLoop::quit);
    loop.exec();
    process.waitForFinished(5000);
    QString info = QString::fromLocal8Bit(process.readAllStandardOutput());
}

void CommonUtils::killAllProcess(const QString &program)
{
    QList<DWORD> pidList = getPid(program);
    for (int pid : pidList) {
        if (pid != 0) {
            killProcess(pid);
        }
    }
}
