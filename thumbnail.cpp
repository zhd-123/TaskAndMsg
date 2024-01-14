#include "thumbnail.h"
#include "messagecenter.h"
Thumbnail::Thumbnail()
{
    MC->registerMessage(PageInsert|PageRemove, this);
}
void Thumbnail::onMessageNotify(MessageType type, QSharedPointer<AfsMsg> msg)
{
    switch (type) {
    case PageInsert:

        break;
    case PageRemove:

        break;
    default:
        break;
    }
}
void Thumbnail::insertPage()
{

}

PageOprTask::PageOprTask(QSharedPointer<TaskInParam> param)
    : TaskBase(param)
{

}
bool PageOprTask::execute(QSharedPointer<TaskOutParam> *param)
{
    *param = QSharedPointer<TaskOutParam>(new PageOprResultParam());
    (*param)->inParam = m_param;
    // work ...

    m_out_param = *param;
}

