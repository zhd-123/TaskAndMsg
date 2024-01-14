#ifndef BASEDEFINED_H
#define BASEDEFINED_H

#include <QObject>
#include <QSharedPointer>

enum MessageType
{
    PageInsert = 0x1,
    PageRemove = 0x2,
    PageSwap = 0x4,
    BookInsert = 0x01,
    BookRemove = 0x02,
    BookUpdate = 0x04,
};
Q_DECLARE_FLAGS(MessageTypes, MessageType)


struct AfsMsg
{
    MessageType type;
};


class NotifyInterface
{
public:
    virtual void onMessageNotify(MessageType type, QSharedPointer<AfsMsg> msg) = 0;
};


struct TaskInParam
{
    bool cancelTask;
    TaskInParam()
    {
        cancelTask = false;
    }
};

struct TaskOutParam
{
    QSharedPointer<TaskInParam> inParam;
};

class TaskBase : public QObject
{
    Q_OBJECT
public:
    explicit TaskBase(QSharedPointer<TaskInParam> param)
        : m_param(param)
        , m_callback(nullptr){}
    void setCallback(std::function<void(QSharedPointer<TaskOutParam>)> fun) {
        m_callback = fun;
    }
    virtual bool execute(QSharedPointer<TaskOutParam> *param) = 0;
    virtual void cancelTask() {
        m_param->cancelTask = true;
    };
    virtual void callback() {
        if (m_callback) {
            m_callback(m_out_param);
        }
    };
protected:
    QSharedPointer<TaskInParam> m_param;
    QSharedPointer<TaskOutParam> m_out_param;
    std::function<void(QSharedPointer<TaskOutParam>)> m_callback;
};

class BaseDefined : public QObject
{
    Q_OBJECT
public:
    explicit BaseDefined(QObject *parent = nullptr);

signals:

};

#endif // BASEDEFINED_H
