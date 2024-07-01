#include "configmanager.h"
#include <QDir>
#include <QSettings>
#include <QTextCodec>

#pragma execution_character_set("utf-8")
ConfigManager::ConfigManager(QObject *parent)
    : QObject{parent}
{
    mConfigFilePath = QDir::currentPath() + QDir::separator() + "config.ini";

}

ConfigManager *ConfigManager::instance()
{
    static ConfigManager instance;
    return &instance;
}

ConfigManager::ConfigParam ConfigManager::readConfig()
{
    QString filePath = mConfigFilePath;
    if (!QFile::exists(filePath)) {
        return mConfigParam;
    }
    QSettings settings(filePath, QSettings::IniFormat);
    QTextCodec *codec = QTextCodec::codecForName("utf-8");
    QTextCodec *gbkCodec = QTextCodec::codecForName("gbk");
    settings.setIniCodec(codec);
    mConfigParam.allSheet = settings.value("allSheet").toBool();
    mConfigParam.sheetName = settings.value("sheetName").toString();
    mConfigParam.lastExcelFile = settings.value("lastExcelFile").toString();
    mConfigParam.lastTsDir = settings.value("lastTsDir").toString();
    mConfigParam.lastUpdatePath = settings.value("lastUpdatePath").toString();
    mConfigParam.lastReleasePath = settings.value("lastReleasePath").toString();
    mConfigParam.key = settings.value("key").toString();
    QStringList fileNameList = settings.allKeys();
    for (QString fileName :  fileNameList) {
        if (fileName == "sheetName" || fileName == "lastExcelFile" || fileName == "lastTsDir"  || fileName == "key") {
            continue;
        }
        if (fileName.startsWith("transfer_")) {
            QString value = settings.value(fileName).toString();
            if (!value.contains("->")) {
                continue;
            }
            QString newKey = value.split("->").at(0);
            QString newValue = value.split("->").at(1);
            mConfigParam.charTransferList.append(qMakePair<QString, QString>(newKey, newValue));
        } else {
            mConfigParam.fileColumnMap.insert(settings.value(fileName).toString(), fileName);
        }
    }
    return mConfigParam;
}

void ConfigManager::writeConfig(const ConfigManager::ConfigParam &configParam)
{
    QString filePath = mConfigFilePath;
    if (!QFile::exists(filePath)) {
        QFile file(filePath);
        if (!file.open(QIODevice::WriteOnly)) {
            return;
        }
        file.close();
    }
    QSettings settings(filePath, QSettings::IniFormat);
    settings.setIniCodec(QTextCodec::codecForName("utf-8"));
//    settings.setValue("key", configParam.key);
//    settings.setValue("allSheet", configParam.allSheet);
//    settings.setValue("sheetName", configParam.sheetName);
    settings.setValue("lastExcelFile", configParam.lastExcelFile);
    settings.setValue("lastTsDir", configParam.lastTsDir);
    settings.setValue("lastUpdatePath", configParam.lastUpdatePath);
    settings.setValue("lastReleasePath", configParam.lastReleasePath);
//    for (QString lang :  mConfigParam.fileColumnMap.keys()) {
//        settings.setValue(mConfigParam.fileColumnMap.value(lang), lang);
//    }

    settings.sync();
}
