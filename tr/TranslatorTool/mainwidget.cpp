#include "mainwidget.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QFileDialog>
#include <QTextStream>
#include <QTextCodec>
#include <QProcess>
#include <QDateTime>
#include <QDebug>
#include <QTimer>
#include <QApplication>

MainWidget::MainWidget(QWidget *parent)
    : QWidget(parent)
    , mExcelReader(nullptr)
{
    setFixedSize(720, 480);
    mConfigParam = ConfigManager::instance()->readConfig();

    initView();
    connectSignals();
}

MainWidget::~MainWidget()
{
}

void MainWidget::releaseExcelReader()
{
    if (mExcelReader) {
        mExcelReader->closeFile();
        delete mExcelReader;
        mExcelReader = nullptr;
    }
}

void MainWidget::initView()
{
    mExcelPathLineEdit = new QLineEdit(this);
    mExcelPathLineEdit->setFixedHeight(32);
    mExcelPathLineEdit->setText(mConfigParam.lastExcelFile);
    mSelectExcelBtn = new QPushButton(this);
    mSelectExcelBtn->setFixedSize(104, 32);
    mSelectExcelBtn->setText("select excel");
    QHBoxLayout *excelLayout = new QHBoxLayout;
    excelLayout->addWidget(mExcelPathLineEdit);
    excelLayout->addSpacing(16);
    excelLayout->addWidget(mSelectExcelBtn);

    mTsDirLineEdit = new QLineEdit(this);
    mTsDirLineEdit->setFixedHeight(32);
    mTsDirLineEdit->setText(mConfigParam.lastTsDir);
    mSelectTsDirBtn = new QPushButton(this);
    mSelectTsDirBtn->setFixedSize(104, 32);
    mSelectTsDirBtn->setText("select tsPath");
    QHBoxLayout *tsLayout = new QHBoxLayout;
    tsLayout->addWidget(mTsDirLineEdit);
    tsLayout->addSpacing(16);
    tsLayout->addWidget(mSelectTsDirBtn);


    mLupdateDirLineEdit = new QLineEdit(this);
    mLupdateDirLineEdit->setFixedHeight(32);
    mLupdateDirLineEdit->setText(mConfigParam.lastUpdatePath);
    mSelectLupdateDirBtn = new QPushButton(this);
    mSelectLupdateDirBtn->setFixedSize(104, 32);
    mSelectLupdateDirBtn->setText("sel update.bat");
    QHBoxLayout *updateLayout = new QHBoxLayout;
    updateLayout->addWidget(mLupdateDirLineEdit);
    updateLayout->addSpacing(16);
    updateLayout->addWidget(mSelectLupdateDirBtn);

    mLreleaseDirLineEdit = new QLineEdit(this);
    mLreleaseDirLineEdit->setFixedHeight(32);
    mLreleaseDirLineEdit->setText(mConfigParam.lastReleasePath);
    mSelectLreleaseDirBtn = new QPushButton(this);
    mSelectLreleaseDirBtn->setFixedSize(104, 32);
    mSelectLreleaseDirBtn->setText("sel release.bat");
    QHBoxLayout *releaseLayout = new QHBoxLayout;
    releaseLayout->addWidget(mLreleaseDirLineEdit);
    releaseLayout->addSpacing(16);
    releaseLayout->addWidget(mSelectLreleaseDirBtn);

    mLupdateBtn = new QPushButton(this);
    mLupdateBtn->setFixedSize(104, 32);
    mLupdateBtn->setText("update.bat");

    mLreleaseBtn = new QPushButton(this);
    mLreleaseBtn->setFixedSize(104, 32);
    mLreleaseBtn->setText("release.bat");

    mStartBtn = new QPushButton(this);
    mStartBtn->setFixedSize(104, 32);
    mStartBtn->setText("start");
    QHBoxLayout *startLayout = new QHBoxLayout;
    startLayout->addWidget(mLupdateBtn);
    startLayout->addWidget(mStartBtn);
    startLayout->addWidget(mLreleaseBtn);
    startLayout->setSpacing(12);
    startLayout->addStretch();

    mTextEdit = new QTextEdit(this);
    QVBoxLayout *layout = new QVBoxLayout;
    layout->addLayout(excelLayout);
    layout->addLayout(tsLayout);
    layout->addLayout(updateLayout);
    layout->addLayout(releaseLayout);
    layout->addLayout(startLayout);
    layout->addWidget(mTextEdit);
    layout->setSpacing(8);
    layout->setContentsMargins(32, 16, 32, 16);
    setLayout(layout);
}

void MainWidget::connectSignals()
{
    connect(mSelectExcelBtn, &QPushButton::clicked, this, &MainWidget::onSelectExcelPath);
    connect(mExcelPathLineEdit, &QLineEdit::textChanged, this, &MainWidget::onExcelPathChange);
    connect(mSelectTsDirBtn, &QPushButton::clicked, this, &MainWidget::onSelectTsDir);
    connect(mTsDirLineEdit, &QLineEdit::textChanged, this, &MainWidget::onTsDirChange);

    connect(mSelectLupdateDirBtn, &QPushButton::clicked, this, &MainWidget::onSelectUpdateDir);
    connect(mLupdateDirLineEdit, &QLineEdit::textChanged, this, &MainWidget::onUpdateDirChange);
    connect(mSelectLreleaseDirBtn, &QPushButton::clicked, this, &MainWidget::onSelectReleaseDir);
    connect(mLreleaseDirLineEdit, &QLineEdit::textChanged, this, &MainWidget::onReleaseDirChange);

    connect(mLupdateBtn, &QPushButton::clicked, this, &MainWidget::onExecUpdate);
    connect(mStartBtn, &QPushButton::clicked, this, &MainWidget::onStartTranslator);
    connect(mLreleaseBtn, &QPushButton::clicked, this, &MainWidget::onExecRelease);
}

void MainWidget::onSelectExcelPath()
{
    QString filePath = QFileDialog::getOpenFileName(this, "Select Excel", ".", tr("Files (*.xls *.xlsx)"));
    mConfigParam.lastExcelFile = filePath;
    mExcelPathLineEdit->setText(mConfigParam.lastExcelFile);
}

void MainWidget::onExcelPathChange(const QString &path)
{
    mConfigParam.lastExcelFile = path;
}

void MainWidget::onSelectTsDir()
{
    QString path = QFileDialog::getExistingDirectory(this, "Select Path",
                    QDir::fromNativeSeparators(mTsDirLineEdit->text()), QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
    if (path.isEmpty()) {
        return;
    }
    mConfigParam.lastTsDir = path;
    mTsDirLineEdit->setText(mConfigParam.lastTsDir);
}

void MainWidget::onTsDirChange(const QString &path)
{
    mConfigParam.lastTsDir = path;
    if (!mConfigParam.lastTsDir.endsWith("/")) {
        mConfigParam.lastTsDir += "/";
    }
}

void MainWidget::onSelectUpdateDir()
{
    QString path = QFileDialog::getOpenFileName(this, "Select update.bat", ".", tr("Files (*.bat)"));
    if (path.isEmpty()) {
        return;
    }
    mConfigParam.lastUpdatePath = path;
    mLupdateDirLineEdit->setText(mConfigParam.lastUpdatePath);
}

void MainWidget::onUpdateDirChange(const QString &path)
{
    mConfigParam.lastUpdatePath = path;
}

void MainWidget::onSelectReleaseDir()
{
    QString path = QFileDialog::getOpenFileName(this, "Select release.bat", ".", tr("Files (*.bat)"));
    if (path.isEmpty()) {
        return;
    }
    mConfigParam.lastReleasePath = path;
    mLreleaseDirLineEdit->setText(mConfigParam.lastReleasePath);
}

void MainWidget::onReleaseDirChange(const QString &path)
{
    mConfigParam.lastReleasePath = path;
}

void MainWidget::onExecUpdate()
{
    QString batFile = mConfigParam.lastUpdatePath;
    if (batFile.isEmpty()) {
        return;
    }
    ConfigManager::instance()->writeConfig(mConfigParam);
    execCmd(batFile);
    mTextEdit->append("path:" + batFile + " exec finish");
}

void MainWidget::onExecRelease()
{
    QString batFile = mConfigParam.lastReleasePath;
    if (batFile.isEmpty()) {
        return;
    }
    ConfigManager::instance()->writeConfig(mConfigParam);
    execCmd(batFile);
    mTextEdit->append("path:" + batFile + " exec finish");
}

void MainWidget::execCmd(const QString &path)
{
    QString dir = "";
    if (path.contains("/")) {
        dir = path.left(path.lastIndexOf("/") + 1);
    }
    QProcess process;
    process.setWorkingDirectory(dir);
    process.start(path);
    process.waitForFinished(-1);
    QString ret = process.readAllStandardOutput();
    mTextEdit->append("exec:" + ret);
    if (ret.isEmpty()) {
        QProcess::startDetached(path, {dir});
    }
}

void MainWidget::onStartTranslator()
{
    if (mConfigParam.lastExcelFile.isEmpty() || mConfigParam.lastTsDir.isEmpty()) {
        return;
    }
    qDebug() << QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss:zzz") + ">> start**";
    ConfigManager::instance()->writeConfig(mConfigParam);
    mTextEdit->clear();
    mTextEdit->setTextColor(Qt::black);
    mExcelReader = new ExcelReader();
    bool allSheet = mConfigParam.allSheet;
    QString sheetName = mConfigParam.sheetName;
    if (sheetName.isEmpty()) {
        sheetName = "Sheet1";
    }
    mExcelReader->openFile(mConfigParam.lastExcelFile);
    if (allSheet) {
        int sheetCount = mExcelReader->sheetCount();
        for (int pageIndex = 1; pageIndex <= sheetCount; ++pageIndex) {
            if (!mExcelReader->openSheet(pageIndex)) {
                break;
            }
            qDebug() << QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss:zzz") + ">> openSheet index:" + QString::number(pageIndex);
            if (pageIndex == 1) {
                mHeadMap = mExcelReader->readFirstRow();
                if (mHeadMap.contains(mConfigParam.key)) {
                    mKeyColumn = mHeadMap.value(mConfigParam.key);
                }
            }
            // 按语言
            QMap<QString, int> langColumnMap;
            for (QString head : mHeadMap.keys()) {
                if (head == mConfigParam.key) {
                    continue;
                }

                if (!mConfigParam.fileColumnMap.contains(head)) {
                    continue;
                }
                QString fileName = mConfigParam.fileColumnMap.value(head);
                int valueColumn = mHeadMap.value(head);
                if (pageIndex == 1) {
                    mTextEdit->append("language:" + head + " file:" + fileName);
                }
                langColumnMap.insert(fileName, valueColumn);
            }
            mExcelReader->readKeyValue(mKeyColumn, langColumnMap, &mLangKVMap);
        }
    } else {
        if (mExcelReader->openSheet(sheetName)) {
            mHeadMap = mExcelReader->readFirstRow();
            if (mHeadMap.contains(mConfigParam.key)) {
                mKeyColumn = mHeadMap.value(mConfigParam.key);
            }
            // 按语言
            QMap<QString, int> langColumnMap;
            for (QString head : mHeadMap.keys()) {
                if (head == mConfigParam.key) {
                    continue;
                }

                if (!mConfigParam.fileColumnMap.contains(head)) {
                    continue;
                }
                QString fileName = mConfigParam.fileColumnMap.value(head);
                int valueColumn = mHeadMap.value(head);
                mTextEdit->append("language:" + head + " file:" + fileName);
                langColumnMap.insert(fileName, valueColumn);
            }
            mExcelReader->readKeyValue(mKeyColumn, langColumnMap, &mLangKVMap);
        }
    }
    qDebug() << "--------------------------------------------------------------------------------------------------\n\n";
    qDebug() << QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss:zzz") + ">> translate start**";

    for (QString head : mLangKVMap.keys()) {
        QString fileName = head;
        QMap<QString, QString> kvMap = mLangKVMap.value(head);
        translatorText(fileName, kvMap);
        qDebug() << QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss:zzz") + ">> translate <" + fileName + "> Finished";
    }
    qDebug() << QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss:zzz") + ">> end**";
    releaseExcelReader();
}

void MainWidget::translatorText(const QString &fileName, const QMap<QString, QString> &maps)
{
    // 打开文件进行读写
    QString tempFile = mConfigParam.lastTsDir + "temp.ts";
    QString tsFile = mConfigParam.lastTsDir + fileName + ".ts";
    QFile readFile(tsFile);
    if (!readFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return;
    }

    QFile writeFile(tempFile);
    if (!writeFile.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        return;
    }
    QTextStream outStream(&writeFile);
    outStream.setCodec("UTF-8"); // 设置文件的编码格式为UTF-8
    // <source>Warning</source>
    // <translation></translation>
    QString lastSource = "";
    QTextCodec *codec = QTextCodec::codecForName("UTF-8");
    while (!readFile.atEnd()) {
        QByteArray lineByteArray = readFile.readLine();
        QString lineBytes = codec->toUnicode(lineByteArray);
        QString trimLineBytes = lineBytes.trimmed();
        if (!trimLineBytes.startsWith("<translation")) {
            outStream << lineBytes.toUtf8();
        } else {
            // tips update
            trimLineBytes.replace("<translation>", "");
            trimLineBytes.replace("<translation type=\"unfinished\">", "");
            trimLineBytes.replace("</translation>", "");
            QString line = trimLineBytes;
            if (lastSource != "" && line != maps.value(lastSource)) {
                if (fileName == "zh") {
                    mTextEdit->append("update key:<" + lastSource + ">-->" + maps.value(lastSource));
                }
            }
            continue;
        }

        if (trimLineBytes.startsWith("<source>")) {
            trimLineBytes.replace("<source>", "");
            trimLineBytes.replace("</source>", "");
            // 从source到product_key
            if (trimLineBytes.contains("&quot;")) {
                trimLineBytes.replace("&quot;", "\\\"");
            }

            // &lt; --> <
            for (QPair<QString, QString> pair : mConfigParam.charTransferList) {
                QString transferChar = pair.first;
                if (trimLineBytes.contains(pair.second)) {
                    trimLineBytes.replace(pair.second, transferChar);
                }
            }

            QString line = trimLineBytes;
            lastSource = line;
            QString translation = "";
            if (maps.contains(line)) {
                translation = maps.value(line);
                // 去除开始、结束的换行
                if (translation.startsWith("\n")) {
                    translation = translation.mid(1);
                }

                if (translation.endsWith("\n")) {
                    translation = translation.left(translation.length() - 1);
                }
                // < --> &lt;
                for (auto pair : mConfigParam.charTransferList) {
                    QString transferChar = pair.first;
                    if (translation.contains(transferChar)) {
                        translation.replace(transferChar, pair.second);
                    }
                }
            } else {
                if (fileName == "zh" && !line.isEmpty()) {
                    mTextEdit->setTextColor(Qt::red);
                    mTextEdit->append("key:<" + line + QString("> no translation"));
                    mTextEdit->setTextColor(Qt::black);
                }
            }
            QString data = "";
            if (translation.isEmpty())  {
                data = QString("        <translation type=\"unfinished\">") + translation + QString("</translation>");
            } else {
                data = QString("        <translation>") + translation + QString("</translation>");
            }
            outStream << data.toUtf8() << "\n";
        }
    }

    readFile.close();
    writeFile.close();
    QFile::remove(tsFile);
    QFile::rename(tempFile, tsFile);
}

void MainWidget::autoExec()
{
    onExecUpdate();
    onStartTranslator();
    onExecRelease();

    qApp->quit();
}
