#include "installmanager.h"
#include <QDebug>
#include <QProcess>
#include <QSettings>
#include <QEventLoop>
#include <QApplication>
#include <QDebug>
#include <QMessageBox>
#include "commonutils.h"

#include <sddl.h>      // for ConvertSidToStringSidW
#include <wincrypt.h>  // for CryptoAPI base64
#include <bcrypt.h>    // for CNG MD5
#include <winternl.h>  // for NT_SUCCESS()
#include <shellapi.h>
#include <ShlObj.h>

#pragma comment(lib, "bcrypt.lib")
#pragma comment(lib, "Crypt32.lib")

enum CheckUserChoiceHashResult{
    ERR_MISMATCH,
    ERR_OTHER,
    OK_V1
};

using namespace std;
#define MAX_NAME 256


static unique_ptr<wchar_t[]> FormatUserChoiceString(const wchar_t* aExt,
                                                   const wchar_t* aUserSid,
                                                   const wchar_t* aProgId,
                                                   SYSTEMTIME aTimestamp) {


  aTimestamp.wSecond = 0;
  aTimestamp.wMilliseconds = 0;

  FILETIME fileTime = {0};
  if (!::SystemTimeToFileTime(&aTimestamp, &fileTime)) {
    return nullptr;
  }

  // This string is built into Windows as part of the UserChoice hash algorithm.
  // It might vary across Windows SKUs (e.g. Windows 10 vs. Windows Server), or
  // across builds of the same SKU, but this is the only currently known
  // version. There isn't any known way of deriving it, so we assume this
  // constant value. If we are wrong, we will not be able to generate correct
  // UserChoice hashes.
  const wchar_t* userExperience =
      L"User Choice set via Windows User Experience "
      L"{D18B6DD5-6124-4341-9318-804003BAFA0B}";

  const wchar_t* userChoiceFmt =
      L"%s%s%s"
      L"%08lx"
      L"%08lx"
      L"%s";
  int userChoiceLen = _scwprintf(userChoiceFmt, aExt, aUserSid, aProgId,
                                 fileTime.dwHighDateTime,
                                 fileTime.dwLowDateTime, userExperience);
  userChoiceLen += 1;  // _scwprintf does not include the terminator

  auto userChoice = make_unique<wchar_t[]>(userChoiceLen);
  _snwprintf_s(userChoice.get(), userChoiceLen, _TRUNCATE, userChoiceFmt, aExt,
               aUserSid, aProgId, fileTime.dwHighDateTime,
               fileTime.dwLowDateTime, userExperience);

  ::CharLowerW(userChoice.get());

  return userChoice;

}


static unique_ptr<DWORD[]> CNG_MD5(const unsigned char* bytes, ULONG bytesLen) {
  constexpr ULONG MD5_BYTES = 16;
  constexpr ULONG MD5_DWORDS = MD5_BYTES / sizeof(DWORD);
  unique_ptr<DWORD[]> hash;

  BCRYPT_ALG_HANDLE hAlg = nullptr;
  if (NT_SUCCESS(::BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_MD5_ALGORITHM,
                                               nullptr, 0))) {
    BCRYPT_HASH_HANDLE hHash = nullptr;
    // As of Windows 7 the hash handle will manage its own object buffer when
    // pbHashObject is nullptr and cbHashObject is 0.
    if (NT_SUCCESS(
            ::BCryptCreateHash(hAlg, &hHash, nullptr, 0, nullptr, 0, 0))) {
      // BCryptHashData promises not to modify pbInput.
      if (NT_SUCCESS(::BCryptHashData(hHash, const_cast<unsigned char*>(bytes),
                                      bytesLen, 0))) {
        hash = make_unique<DWORD[]>(MD5_DWORDS);
        if (!NT_SUCCESS(::BCryptFinishHash(
                hHash, reinterpret_cast<unsigned char*>(hash.get()),
                MD5_DWORDS * sizeof(DWORD), 0))) {
          hash.reset();
        }
      }
      ::BCryptDestroyHash(hHash);
    }
    ::BCryptCloseAlgorithmProvider(hAlg, 0);
  }

  return hash;
}

// @return The input bytes encoded as base64, nullptr on failure.
static unique_ptr<wchar_t[]> CryptoAPI_Base64Encode(const unsigned char* bytes,
                                                   DWORD bytesLen) {
  DWORD base64Len = 0;
  if (!::CryptBinaryToStringW(bytes, bytesLen,
                              CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF,
                              nullptr, &base64Len)) {
    return nullptr;
  }
  auto base64 = make_unique<wchar_t[]>(base64Len);
  if (!::CryptBinaryToStringW(bytes, bytesLen,
                              CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF,
                              base64.get(), &base64Len)) {
    return nullptr;
  }

  return base64;
}

static inline DWORD WordSwap(DWORD v) { return (v >> 16) | (v << 16); }

/*
 * Generate the UserChoice Hash.
 *
 * This implementation is based on the references listed above.
 * It is organized to show the logic as clearly as possible, but at some
 * point the reasoning is just "this is how it works".
 *
 * @param inputString   A null-terminated string to hash.
 *
 * @return The base64-encoded hash, or nullptr on failure.
 */
static unique_ptr<wchar_t[]> HashString(const wchar_t* inputString) {
  auto inputBytes = reinterpret_cast<const unsigned char*>(inputString);
  int inputByteCount = (::lstrlenW(inputString) + 1) * sizeof(wchar_t);

  constexpr size_t DWORDS_PER_BLOCK = 2;
  constexpr size_t BLOCK_SIZE = sizeof(DWORD) * DWORDS_PER_BLOCK;
  // Incomplete blocks are ignored.
  int blockCount = inputByteCount / BLOCK_SIZE;

  if (blockCount == 0) {
    return nullptr;
  }

  // Compute an MD5 hash. md5[0] and md5[1] will be used as constant multipliers
  // in the scramble below.
  auto md5 = CNG_MD5(inputBytes, inputByteCount);
  if (!md5) {
    return nullptr;
  }

  // The following loop effectively computes two checksums, scrambled like a
  // hash after every DWORD is added.

  // Constant multipliers for the scramble, one set for each DWORD in a block.
  const DWORD C0s[DWORDS_PER_BLOCK][5] = {
      {md5[0] | 1, 0xCF98B111uL, 0x87085B9FuL, 0x12CEB96DuL, 0x257E1D83uL},
      {md5[1] | 1, 0xA27416F5uL, 0xD38396FFuL, 0x7C932B89uL, 0xBFA49F69uL}};
  const DWORD C1s[DWORDS_PER_BLOCK][5] = {
      {md5[0] | 1, 0xEF0569FBuL, 0x689B6B9FuL, 0x79F8A395uL, 0xC3EFEA97uL},
      {md5[1] | 1, 0xC31713DBuL, 0xDDCD1F0FuL, 0x59C3AF2DuL, 0x35BD1EC9uL}};

  // The checksums.
  DWORD h0 = 0;
  DWORD h1 = 0;
  // Accumulated total of the checksum after each DWORD.
  DWORD h0Acc = 0;
  DWORD h1Acc = 0;

  for (int i = 0; i < blockCount; ++i) {
    for (size_t j = 0; j < DWORDS_PER_BLOCK; ++j) {
      const DWORD* C0 = C0s[j];
      const DWORD* C1 = C1s[j];

      DWORD input;
      memcpy(&input, &inputBytes[(i * DWORDS_PER_BLOCK + j) * sizeof(DWORD)],
             sizeof(DWORD));

      h0 += input;
      // Scramble 0
      h0 *= C0[0];
      h0 = WordSwap(h0) * C0[1];
      h0 = WordSwap(h0) * C0[2];
      h0 = WordSwap(h0) * C0[3];
      h0 = WordSwap(h0) * C0[4];
      h0Acc += h0;

      h1 += input;
      // Scramble 1
      h1 = WordSwap(h1) * C1[1] + h1 * C1[0];
      h1 = (h1 >> 16) * C1[2] + h1 * C1[3];
      h1 = WordSwap(h1) * C1[4] + h1;
      h1Acc += h1;
    }
  }

  DWORD hash[2] = {h0 ^ h1, h0Acc ^ h1Acc};

  return CryptoAPI_Base64Encode(reinterpret_cast<const unsigned char*>(hash),
                                sizeof(hash));
}

unique_ptr<wchar_t[]> GenerateUserChoiceHash(const wchar_t* aExt,
                                            const wchar_t* aUserSid,
                                            const wchar_t* aProgId,
                                            SYSTEMTIME aTimestamp) {
  auto userChoice = FormatUserChoiceString(aExt, aUserSid, aProgId, aTimestamp);
  if (!userChoice) {
    return nullptr;
  }
  return HashString(userChoice.get());
}

QString InstallManager::getWinSid()
{
    char userName[MAX_NAME] = "";
    char sid[MAX_NAME] = "";
    DWORD nameSize = sizeof(userName);
    ::GetUserName((LPWSTR)userName, &nameSize);
    char userSID[MAX_NAME] = "";
    char userDomain[MAX_NAME] = "";
    DWORD sidSize = sizeof(userSID);
    DWORD domainSize = sizeof(userDomain);

    SID_NAME_USE snu;
    ::LookupAccountName(NULL,
        (LPWSTR)userName,
        (PSID)userSID,
        &sidSize,
        (LPWSTR)userDomain,
        &domainSize,
        &snu);

    PSID_IDENTIFIER_AUTHORITY psia = GetSidIdentifierAuthority(userSID);
    sidSize = sprintf(sid, "S-%lu-", SID_REVISION);
    sidSize += sprintf(sid + strlen(sid), "%-lu", psia->Value[5]);

    int i = 0;
    int subAuthorities = *GetSidSubAuthorityCount(userSID);

    for (i = 0; i < subAuthorities; i++)
    {
        sidSize += sprintf(sid + sidSize, "-%lu", *GetSidSubAuthority(userSID, i));
    }
    return sid;
}


InstallManager::InstallManager(QObject *parent)
    : QObject(parent)
{

}

InstallManager *InstallManager::instance()
{
    static InstallManager instance;
    return &instance;
}
void InstallManager::startService(const QString &name)
{
    SC_HANDLE hSC = ::OpenSCManager(NULL, NULL, GENERIC_EXECUTE);
    if(hSC == NULL) {
        return;
    }
    SC_HANDLE hSvc = ::OpenService(hSC, (LPCWSTR)name.toStdWString().c_str(),
        SERVICE_START | SERVICE_QUERY_STATUS | SERVICE_QUERY_CONFIG | SERVICE_STOP | SERVICE_CHANGE_CONFIG);
    if(hSvc == NULL) {
        ::CloseServiceHandle(hSC);
        return;
    }
    SERVICE_STATUS status;
    if(::QueryServiceStatus(hSvc, &status) == FALSE)
    {
        ::CloseServiceHandle(hSvc);
        ::CloseServiceHandle(hSC);
        return;
    }
    if(status.dwCurrentState == SERVICE_STOPPED)
    {
        int bufSize = 8*1024;
        LPQUERY_SERVICE_CONFIGW config = (LPQUERY_SERVICE_CONFIGW)malloc(bufSize);
        DWORD outSize;
        if (::QueryServiceConfig(hSvc, config, bufSize, &outSize)) {
            if (config->dwStartType == SERVICE_DISABLED) {
                SC_LOCK sclLockA;
                sclLockA = LockServiceDatabase(hSC);
                if (sclLockA != NULL){
                    if (!ChangeServiceConfig(
                        hSvc, // handle of service
                        SERVICE_NO_CHANGE, // service type: no change
                        SERVICE_DEMAND_START, // 这里做了更改
                        SERVICE_NO_CHANGE, // error control: no change
                        NULL, // binary path: no change
                        NULL, // load order group: no change
                        NULL, // tag ID: no change
                        NULL, // dependencies: no change
                        NULL, // account name: no change
                        NULL, // password: no change
                        NULL)) //displayname
                    {
                    }
                }
                UnlockServiceDatabase(sclLockA);
            }
        }
        free(config);
        if(::StartService(hSvc, NULL, NULL) == FALSE)
        {
            ::CloseServiceHandle(hSvc);
            ::CloseServiceHandle(hSC);
            return;
        }
        while(::QueryServiceStatus(hSvc, &status) == TRUE)
        {
            ::Sleep(status.dwWaitHint);
            if( status.dwCurrentState == SERVICE_RUNNING)
            {
                ::CloseServiceHandle(hSvc);
                ::CloseServiceHandle(hSC);
                return;
            }
      }
    }
    ::CloseServiceHandle(hSvc);
    ::CloseServiceHandle(hSC);
}

BOOL InstallManager::createProcessAsUser(const QString &filePath)
{
    QString path = "\"" + filePath + "\"";
    QString dir = filePath.left(filePath.lastIndexOf("/"));
    QString app = filePath.mid(filePath.lastIndexOf("/") + 1);
    BOOL ret = FALSE;
    DWORD u_ExplorerPID;
    HANDLE hTokenUser = 0;
    HANDLE h_Token = 0;
    HANDLE h_Process = 0;
    STARTUPINFO si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(STARTUPINFO));
    ZeroMemory(&pi, sizeof(PROCESS_INFORMATION));
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    HWND h_Progman = GetShellWindow();
    GetWindowThreadProcessId(h_Progman, &u_ExplorerPID);
    h_Process = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, u_ExplorerPID);
    // 获取进程的令牌句柄
    OpenProcessToken(h_Process, TOKEN_DUPLICATE, &h_Token);
    DuplicateTokenEx(h_Token, TOKEN_ALL_ACCESS, 0, SecurityImpersonation, TokenPrimary, &hTokenUser);
    ret = CreateProcessWithTokenW(hTokenUser, NULL, (LPCWSTR)app.toStdWString().c_str(),
        (LPWSTR)path.toStdWString().c_str()
        , NORMAL_PRIORITY_CLASS, NULL, /*L"."*/(LPWSTR)dir.toStdWString().c_str(), &si, &pi);
    if (h_Token) {
        CloseHandle(h_Token);
    }
    if (hTokenUser) {
        CloseHandle(hTokenUser);
    }
    if (h_Process) {
        CloseHandle(h_Process);
    }
    if (!ret) {
        DWORD dwStatus = GetLastError();
        QMap<QString,QVariant> gaMap1;
        gaMap1["action"] = "start_fail_num";
        gaMap1["result"] = "code:" + QString::number(dwStatus) + " path:" + filePath;
        gaEventUpload("start_process", gaMap1);
    } else {
        QMap<QString,QVariant> gaMap1;
        gaMap1["action"] = "start_success_num";
        gaMap1["result"] = "path:" + filePath;
        gaEventUpload("start_process", gaMap1);
    }
    return ret;
}

bool InstallManager::start(const QString &installPath)
{
    mInstallPath = installPath;
    QString setupFile = setupFilePath();
    CommonUtils::killAllProcess("UPDF.exe");
    CommonUtils::killAllProcess("WebView.exe");
    if (!uncompress(setupFile, installPath)) {
        return false;
    }

    QString filePath = QDir::fromNativeSeparators(installPath) + "UPDF.exe";
    mFilePath = filePath;
    createShortcut(filePath);
    writeReg(installPath, filePath);
    runExeAsUser(filePath);
    return true;
}

void InstallManager::runExeInAdmin()
{
    QString path = "\"" + mFilePath + "\"";
    QProcess::startDetached(path, {mInstallPath});
}

void InstallManager::runExeWithShortcut(const QString &path)
{
    ::ShellExecute(NULL, L"open", (LPCWSTR)path.toStdWString().c_str(), NULL, NULL, SW_SHOWNORMAL);
}

bool InstallManager::uncompress(const QString &file, const QString &dir)
{
    QProcess process;
    QString output = "-o" + QDir::toNativeSeparators(dir);
    process.start("7z.exe", {"x", QDir::toNativeSeparators(file), "-aoa", "-y", output});
    QEventLoop loop;
    connect(&process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), &loop, &QEventLoop::quit);
    loop.exec();
    process.waitForFinished(-1);
    QString ret = process.readAllStandardOutput();
    if (ret.contains("Everything is Ok", Qt::CaseInsensitive)) {
        return true;
    }
    return false;
}

void InstallManager::createShortcut(const QString &filePath)
{
    QFile::link(filePath, QStandardPaths::writableLocation(QStandardPaths::DesktopLocation).append("/").append("UPDF.lnk"));

    QString startMenuPath = QStandardPaths::writableLocation(QStandardPaths::ApplicationsLocation).append("/UPDF");
    QDir dir(startMenuPath);
    if (!dir.exists()) {
        dir.mkdir(startMenuPath);
    }
    QFile::link(filePath, startMenuPath.append("/").append("UPDF.lnk"));
}

void InstallManager::writeReg(const QString &fileDir, const QString &filePath)
{
    writeRegedit("WinUpdf", fileDir, filePath);
    QString key = "UPDF.AssocFile.PDF";
    writePDFAssociateRegedit(key, fileDir, filePath);
    // cp unstall
    QString newUnistPath = fileDir + "uninst.exe";
    // check uninst.exe is in fileDir

    if (QFile::exists(fileDir + "uninst.exe.nsis")) {
        QFile::remove(fileDir + "uninst.exe.nsis");
    }
    if (!QFile::exists(newUnistPath)) {
        QFile::copy("uninst.exe", newUnistPath);
    }
    writeRegeditUninstall("UPDF", filePath, newUnistPath);
}

void InstallManager::writeRegedit(const QString &scheme, const QString &installPath, const QString &filePath)
{
    QString path = filePath;
    QString baseUrl("HKEY_CLASSES_ROOT\\");
    QSettings setting(baseUrl + scheme, QSettings::NativeFormat);
    setting.setValue("URL Protocol", path.replace("/","\\"));
    setting.sync();

    auto iconPath = installPath + "defaulticon.ico";
    QSettings setting1(baseUrl + scheme + QString("\\DefaultIcon"), QSettings::NativeFormat);
    setting1.setValue(".",iconPath.replace("/","\\"));
    setting1.sync();

    QSettings setting2(baseUrl + scheme + QString("\\Shell\\open\\command"), QSettings::NativeFormat);
    setting2.setValue(".", path.replace("/","\\")+QString(" \"%1\"") );
    setting2.sync();
}

void InstallManager::writePDFAssociateRegedit(const QString &scheme, const QString &installPath, const QString &filePath)
{
    QString path = filePath;
    QString baseUrl("HKEY_CLASSES_ROOT\\");
    auto iconPath = installPath + "defaulticon.ico";
    QSettings setting1(baseUrl + scheme + QString("\\DefaultIcon"), QSettings::NativeFormat);
    setting1.setValue(".",iconPath.replace("/","\\"));
    setting1.sync();

    QSettings setting2(baseUrl + scheme + QString("\\Shell\\open\\command"), QSettings::NativeFormat);
    setting2.setValue(".", path.replace("/","\\")+QString(" \"%1\""));
    setting2.sync();

    QSettings setting3(baseUrl + QString("\\.pdf\\OpenWithProgids"), QSettings::NativeFormat);
    setting3.setValue(scheme, "");
    setting3.sync();

    QString baseWsUrl("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion");
    QSettings settingWs1(baseWsUrl + QString("\\ApplicationAssociationToasts"), QSettings::NativeFormat);
    settingWs1.setValue(scheme + "_.pdf", 0);
    settingWs1.sync();

    const wchar_t* lpszSubKey = L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\FileExts\\.pdf\\UserChoice";
    RegDeleteKey(HKEY_CURRENT_USER, lpszSubKey);

    FILETIME lastWriteFileTime;
    SYSTEMTIME lastWriteSystemTime;
    ::FileTimeToSystemTime(&lastWriteFileTime, &lastWriteSystemTime);

    SYSTEMTIME stUTC;
    ::GetSystemTime(&stUTC);
    auto sid = getWinSid();
    auto computedHash = GenerateUserChoiceHash(L".pdf", sid.toStdWString().c_str(), scheme.toStdWString().c_str(), stUTC);
    QSettings settingWs2(baseWsUrl + QString("\\Explorer\\FileExts\\.pdf\\UserChoice"), QSettings::NativeFormat);
    settingWs2.setValue(".", "");
    settingWs2.setValue("ProgId", scheme);
    settingWs2.setValue("Hash", QString::fromWCharArray(computedHash.get()));
    settingWs2.sync();

    QString baselmUrl("HKEY_LOCAL_MACHINE\\Software\\Classes");
    QSettings settingLm1(baselmUrl + QString("\\.pdf\\OpenWithProgids"), QSettings::NativeFormat);
    settingLm1.setValue(scheme, "");
    settingLm1.sync();

    QSettings settingLm2(baselmUrl + scheme + QString("\\DefaultIcon"), QSettings::NativeFormat);
    settingLm2.setValue(".",iconPath.replace("/","\\"));
    settingLm2.sync();

    QSettings settingLm3(baselmUrl + scheme + QString("\\Shell\\open\\command"), QSettings::NativeFormat);
    settingLm3.setValue(".", path.replace("/","\\")+QString(" \"%1\""));
    settingLm3.sync();

    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, NULL, NULL);
}

void InstallManager::writeRegeditUninstall(const QString &scheme, const QString &filePath, const QString &uninstPath)
{
    QString path = filePath;
    QString unpath = uninstPath;
    QString baseUrl("HKEY_LOCAL_MACHINE\\Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\");
    QSettings setting(baseUrl + scheme, QSettings::NativeFormat);
    setting.setValue("DisplayName", scheme);
    setting.setValue("URLInfoAbout", "https://www.superace.com/");
    setting.setValue("Publisher", "Superace Software Technology Co., Ltd.");
    setting.setValue("UninstallString", unpath.replace("/","\\"));
    setting.setValue("DisplayIcon", path.replace("/","\\"));
    setting.sync();
}

void InstallManager::runExeAsUser(const QString &filePath)
{
    QString batFile = startByBat(filePath);
    createProcessAsUser(batFile);
}

QString InstallManager::startByBat(const QString &filePath)
{
    QString path = "\"" + filePath + "\"";
    QString batFile = QDir::fromNativeSeparators(QApplication::applicationDirPath() + QDir::separator() + "start.bat");
    QString command = "start \"\" "+ path;
    QFile file(batFile);
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        file.write(command.toLocal8Bit().data());
    }
    file.close();
    return batFile;
}
