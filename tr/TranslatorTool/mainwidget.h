#ifndef MAINWIDGET_H
#define MAINWIDGET_H

#include <QWidget>
#include <QLineEdit>
#include <QTextEdit>
#include <QPushButton>
#include "excelreader.h"
#include "configmanager.h"

class MainWidget : public QWidget
{
    Q_OBJECT

public:
    MainWidget(QWidget *parent = nullptr);
    ~MainWidget();
    void initView();
    void connectSignals();

    QString selectFile();
    void translatorText(const QString &fileName, const QMap<QString, QString> &maps);
    void autoExec();
private:
    void releaseExcelReader();
    void execCmd(const QString &path);
private slots:
    void onSelectExcelPath();
    void onExcelPathChange(const QString &path);
    void onSelectTsDir();
    void onTsDirChange(const QString &path);
    void onStartTranslator();

    void onSelectUpdateDir();
    void onUpdateDirChange(const QString &path);
    void onSelectReleaseDir();
    void onReleaseDirChange(const QString &path);
    void onExecUpdate();
    void onExecRelease();
private:
    ConfigManager::ConfigParam mConfigParam;
    int mKeyColumn;

    QLineEdit *mExcelPathLineEdit;
    QPushButton *mSelectExcelBtn;
    QLineEdit *mTsDirLineEdit;
    QPushButton *mSelectTsDirBtn;
    QTextEdit *mTextEdit;

    QLineEdit *mLupdateDirLineEdit;
    QPushButton *mSelectLupdateDirBtn;
    QPushButton *mLupdateBtn;
    QPushButton *mStartBtn;
    QLineEdit *mLreleaseDirLineEdit;
    QPushButton *mSelectLreleaseDirBtn;
    QPushButton *mLreleaseBtn;
    ExcelReader *mExcelReader;

    QMap<QString, int> mHeadMap;
    QMap<QString, QMap<QString, QString>> mLangKVMap;
};
#endif // MAINWIDGET_H
