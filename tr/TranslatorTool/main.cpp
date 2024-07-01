#include "mainwidget.h"
#include <QTextCodec>
#include <QApplication>
#include <QDebug>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    QTextCodec *codec = QTextCodec::codecForName("utf-8");
    QTextCodec::setCodecForLocale(codec);
    MainWidget w;
    if (argc >= 2) {
        QString action = argv[1];
        if (action.toLower() == "auto") {
            w.autoExec();
            return 0;
        }
    }

    w.show();
    return a.exec();
}
