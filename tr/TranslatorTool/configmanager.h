#ifndef CONFIGMANAGER_H
#define CONFIGMANAGER_H

#include <QObject>
#include <QMap>
class ConfigManager : public QObject
{
    Q_OBJECT
public:
    struct ConfigParam {
        bool allSheet; // 读取所有sheet
        QString sheetName;
        QString key;
        QString lastExcelFile;
        QString lastTsDir;
        QString lastUpdatePath;
        QString lastReleasePath;
        QMap<QString, QString> fileColumnMap;
        QList<QPair<QString, QString>> charTransferList; // 转义字符
        ConfigParam() {
            allSheet = false;
        }
    };
    static ConfigManager *instance();
    ConfigManager::ConfigParam readConfig();
    void writeConfig(const ConfigManager::ConfigParam &configParam);
private:
    explicit ConfigManager(QObject *parent = nullptr);

signals:
private:
    QString mConfigFilePath;
    ConfigParam mConfigParam;
};

#endif // CONFIGMANAGER_H
