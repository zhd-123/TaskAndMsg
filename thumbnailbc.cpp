#include "thumbnailbc.h"


ThumbnailBC::ThumbnailBC()
{

}
void ThumbnailBC::addPage(int pageIndex, const PageData &pageData)
{
    PageCommand *command = new PageCommand(pageIndex, pageData);
}

void ThumbnailBC::removePage(int pageIndex)
{

}

