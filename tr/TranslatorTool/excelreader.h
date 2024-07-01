#ifndef EXCELREADER_H
#define EXCELREADER_H

#include <QObject>
#include <QMap>
#include <QAxObject>

class ExcelReader : public QObject
{
    Q_OBJECT
public:
    explicit ExcelReader(QObject *parent = nullptr);
    ~ExcelReader();
    void openFile(const QString &path);
    bool openSheet(int index);
    bool openSheet(const QString &sheetName);
    void closeFile();
    QMap<QString, int> readFirstRow();
    void readKeyValue(int keyColumn, const QMap<QString, int> langColumnMap, QMap<QString, QMap<QString, QString>> *langKVMap);
    int sheetRowCount();
    int sheetColumnCount();
    int sheetCount();

signals:
private:
    QAxObject *mExcel;
    QAxObject *mWorkbook;
    QAxObject *mWorkbooks;
    QAxObject *mWorksheet;
    int mRowCount;
    int mColumnCount;
};

#endif // EXCELREADER_H
