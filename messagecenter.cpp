#include "messagecenter.h"

MessageCenter::MessageCenter()
{

}
MessageCenter* MessageCenter::instance()
{
    static MessageCenter inst;
    return &inst;
}

void MessageCenter::registerMessage(MessageTypes messages, NotifyInterface *inst)
{
    map.insert(inst, messages);
}
void MessageCenter::notifyMessage(MessageType message, QSharedPointer<AfsMsg> msg)
{
    dispatchMessage(message, msg);
}
void MessageCenter::dispatchMessage(MessageType message, QSharedPointer<AfsMsg> msg)
{
    for (NotifyInterface *inst : map.keys()) {
        if (map.value(inst).testFlag(message)) {
            inst->onMessageNotify(message, msg);
        }
    }
}
