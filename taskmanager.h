#ifndef TASKMANAGER_H
#define TASKMANAGER_H

#include "basedefined.h"
#include <QMutex>
#include <QMap>
#define TM TaskManager::instance()

class TaskManager
{
public:
    static TaskManager* instance();
    TaskManager();

    int addTask(QSharedPointer<TaskBase> task);
    void cancelTask(int id);
    void taskExec();
private:
    QMap<int, QSharedPointer<TaskBase>> map;
    QMutex m_mutex;
    int m_id;
    int m_runningId;
    QSharedPointer<TaskBase> m_runningTask;
};

#endif // TASKMANAGER_H
