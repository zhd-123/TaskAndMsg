#include "basechatmodel.h"
#include <QDebug>

AIToolsResultModel::AIToolsResultModel(QObject *parent)
    : ListModelData<RspGptStruct>(parent)
{

}

ChatPDFResultModel::ChatPDFResultModel(QObject *parent)
    : ListModelData<GptRspPDFStruct>(parent)
{

}

TranslateResultModel::TranslateResultModel(QObject *parent)
    : ListModelData<RspTranslateStruct>(parent)
{

}
