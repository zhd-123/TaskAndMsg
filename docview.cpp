#include "docview.h"
#include "messagecenter.h"

DocView::DocView()
{
    MC->registerMessage(PageInsert|PageRemove, this);
}

void DocView::onMessageNotify(MessageType type, QSharedPointer<AfsMsg> msg)
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
