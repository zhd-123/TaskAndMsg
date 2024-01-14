#ifndef THUMBNAILBC_H
#define THUMBNAILBC_H

#include "thumbnail.h"
#include <QObject>
#include <QUndoCommand>
#include "messagecenter.h"
#include "taskmanager.h"

class PageCommand : public QUndoCommand, public QObject
{
    public:
    PageCommand(int pageIndex, const PageData &pageData){
        this->pageIndex = pageIndex;
        this->pageData = pageData;
    }
    void redo()
    {
        QSharedPointer<PageOprTask> task = QSharedPointer<PageOprTask>(new PageOprTask(QSharedPointer<PageOprParam>(new PageOprParam(Insert, pageData))));
        task->setCallback([=](QSharedPointer<TaskOutParam> param) {
            MC->notifyMessage(PageInsert, QSharedPointer<AfsMsg>(new AfsMsg()));
        });
        //undoCommand
        int id = TM->addTask(task);
    }
    void undo()
    {

    }
private:
    int pageIndex;
    PageData pageData;
};

class ThumbnailBC : public QObject
{
    Q_OBJECT
public:
    ThumbnailBC();

    void addPage(int pageIndex, const PageData &pageData);
    void removePage(int pageIndex);
};

#endif // THUMBNAILBC_H
