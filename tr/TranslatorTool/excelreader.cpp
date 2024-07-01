#include "excelreader.h"
#include <Windows.h>
ExcelReader::ExcelReader(QObject *parent)
    : QObject(parent)
    , mExcel(nullptr)
{
    mRowCount = 0;
    mColumnCount = 0;

    OleInitialize(0);
}

ExcelReader::~ExcelReader()
{
    OleUninitialize();
}

void ExcelReader::openFile(const QString &path)
{
    mExcel = new QAxObject("Excel.Application");
    mExcel->dynamicCall("SetVisible(bool)", false); //true 表示操作文件时可见，false表示为不可见
    mWorkbooks = mExcel->querySubObject("WorkBooks");//所有excel文件
    mWorkbook = mWorkbooks->querySubObject("Open(QString&)", path);//按照路径获取文件
}

bool ExcelReader::openSheet(int index)
{
    QAxObject *worksheets = mWorkbook->querySubObject("WorkSheets");//获取文件的所有sheet页
    mWorksheet = worksheets->querySubObject("Item(int)", index);//获取文件sheet页
    if (mWorksheet) {
        QAxObject *usedrange = mWorksheet->querySubObject("UsedRange");//有数据的矩形区域
        //获取行数
        QAxObject *Rows = usedrange->querySubObject("Rows");
        mRowCount = Rows->property("Count").toInt();
        //获取列数
        QAxObject *Columns = usedrange->querySubObject("Columns");
        mColumnCount = Columns->property("Count").toInt();
    }
    return mWorksheet != nullptr;
}

bool ExcelReader::openSheet(const QString &sheetName)
{
    QAxObject *worksheets = mWorkbook->querySubObject("WorkSheets");//获取文件的所有sheet页
    mWorksheet = worksheets->querySubObject("Item(QString)", sheetName);//获取文件sheet页
    if (mWorksheet) {
        QAxObject *usedRange = mWorksheet->querySubObject("UsedRange");//有数据的矩形区域
        //获取行数
        QAxObject *Rows = usedRange->querySubObject("Rows");
        mRowCount = Rows->property("Count").toInt();
        //获取列数
        QAxObject *Columns = usedRange->querySubObject("Columns");
        mColumnCount = Columns->property("Count").toInt();
    }
    return mWorksheet != nullptr;
}

void ExcelReader::closeFile()
{
    if (mExcel)
    {
        mWorkbook->dynamicCall("Close()");
        mWorkbooks->dynamicCall("Close()");
        mExcel->dynamicCall("Quit()");
        delete mExcel;
        mExcel = nullptr;
    }
}

QMap<QString, int> ExcelReader::readFirstRow()
{
    QMap<QString, int> headMap;
    int i = 1;//第一行默认为表头，从第二行读起
    for (int j = 1; j <= mColumnCount; j++) {
        QAxObject *cell = mWorksheet->querySubObject("Cells(int,int)", i, j);
        QString strValue = cell->property("Value2").toString();
        headMap.insert(strValue, j);
    }
    return headMap;
}

void ExcelReader::readKeyValue(int keyColumn, const QMap<QString, int> langColumnMap, QMap<QString, QMap<QString, QString>> *langKVMap)
{
    QAxObject *usedRange = mWorksheet->querySubObject("UsedRange");//有数据的矩形区域
    QVariant var = usedRange->dynamicCall("Value");
    QVariantList varRows=var.toList();
    const int rowCount = varRows.size();

    for (QString lang : langColumnMap.keys()) {
        int valueColumn = langColumnMap.value(lang);
        QVariantList rowData;
        QMap<QString, QString> kvMap;
        for(int i=0;i<rowCount;++i)
        {
            rowData = varRows[i].toList();
            if (rowData.size() >= keyColumn && rowData.size() >= valueColumn) {
                QString strKey = rowData.at(keyColumn - 1).toString();
                if (strKey.isEmpty()) {
                    break;
                }
                QString strValue = rowData.at(valueColumn - 1).toString();
                kvMap.insert(strKey, strValue);
            }
        }
        if (kvMap.size() > 0) {
            if ((*langKVMap).contains(lang)) {
               QMap<QString, QString> tempKVMap = (*langKVMap).value(lang);
               tempKVMap.unite(kvMap);
               (*langKVMap).insert(lang, tempKVMap);
            } else {
               (*langKVMap).insert(lang, kvMap);
            }
        }
    }
}

int ExcelReader::sheetRowCount()
{
    return mRowCount;
}

int ExcelReader::sheetColumnCount()
{
    return mColumnCount;
}

int ExcelReader::sheetCount()
{
    QAxObject *worksheets = mWorkbook->querySubObject("WorkSheets");
    return worksheets->property("Count").toInt();
}
