#ifndef DOCVIEW_H
#define DOCVIEW_H

#include "basedefined.h"

class DocView : public MessageNotify
{
public:
    DocView();
    void onMessageNotify(MessageType type, QSharedPointer<AfsMsg> msg) override;
};

#endif // DOCVIEW_H
