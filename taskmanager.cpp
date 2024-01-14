#include "taskmanager.h"
#include <QMutexLocker>

TaskManager* TaskManager::instance()
{
    static TaskManager inst;
    return &inst;
}

TaskManager::TaskManager()
    : m_id(0)
{

}
int TaskManager::addTask(QSharedPointer<TaskBase> task)
{
    QMutexLocker locker(&m_mutex);
    if (m_id > 9999) {
        m_id = 0;
    }
    m_id++;
    map.insert(m_id, task);
    taskExec();
    return m_id;
}
void TaskManager::cancelTask(int id)
{
    if (m_runningId == id) {
        m_runningTask->cancelTask();
    } else {
        QMutexLocker locker(&m_mutex);
        if (map.contains(id)) {
            map.remove(id);
        }
    }
}
void TaskManager::taskExec()
{
    while (1) {
        if (!map.isEmpty()) {
            QSharedPointer<TaskBase> task = nullptr;
            {
                QMutexLocker locker(&m_mutex);
                m_runningId = map.firstKey();
                task = map.take(m_runningId);
            }

            QSharedPointer<TaskOutParam> param;
            m_runningTask = task;
            if (task->execute(&param)) {
                task->callback();
            }
        }
    }
}
