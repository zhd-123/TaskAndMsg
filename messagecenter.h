#ifndef MESSAGECENTER_H
#define MESSAGECENTER_H

#include "basedefined.h"
#include <QMap>

#define MC MessageCenter::instance()

class MessageCenter
{
public:
    MessageCenter();
    static MessageCenter* instance();

    void registerMessage(MessageTypes messages, MessageNotify *inst);
    void notifyMessage(MessageType message, QSharedPointer<AfsMsg> msg);
    void dispatchMessage(MessageType message, QSharedPointer<AfsMsg> msg);
private:
    QMap<MessageNotify *, MessageTypes> map;
};

#endif // MESSAGECENTER_H
