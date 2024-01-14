#ifndef THUMBNAIL_H
#define THUMBNAIL_H

#include "basedefined.h"

struct PageData
{
    int pageIndex;
    int width;
    int height;
};

class Thumbnail : public NotifyInterface
{
public:
    Thumbnail();
    void insertPage();
};
enum PageOprType
{
    Insert,
    Remove,
    Swap,
};


struct PageOprParam : public TaskInParam
{
    PageOprType oprType;
    PageData pageData;
    PageOprParam(PageOprType oprType, const PageData &pageData)
    {
        this->oprType = oprType;
        this->pageData = pageData;
    }
};
struct PageOprResultParam : public TaskOutParam
{

};


class PageOprTask : public TaskBase
{
public:
    explicit PageOprTask(QSharedPointer<TaskInParam> param);

    bool execute(QSharedPointer<TaskOutParam> *param);
};




#endif // THUMBNAIL_H
