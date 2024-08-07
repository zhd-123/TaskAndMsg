#include "FramelessHelper.h"
#include "FramelessHelper_p.h"

#include <windows.h>

#include <QDebug>

// class FramelessHelper

FramelessHelper::FramelessHelper(TitleBar* titleBar,QWidget *parent)
    : QObject(parent)
    , d_ptr(new FramelessHelperPrivate())
{
    Q_D(FramelessHelper);
    d->window = parent;
    d->mTitleBar = titleBar;
    Q_CHECK_PTR(parent);

    if (d->window)
        d->window->installEventFilter(this);
}

FramelessHelper::~FramelessHelper()
{
}

void FramelessHelper::setDraggableMargins(int left, int top, int right, int bottom)
{
    Q_D(FramelessHelper);

    d->priDraggableMargins = QMargins(left, top, right, bottom);
}

void FramelessHelper::setMaximizedMargins(int left, int top, int right, int bottom)
{
    Q_D(FramelessHelper);

    d->priMaximizedMargins = QMargins(left, top, right, bottom);
}

void FramelessHelper::setDraggableMargins(const QMargins &margins)
{
    Q_D(FramelessHelper);

    d->priDraggableMargins = margins;
}

void FramelessHelper::setMaximizedMargins(const QMargins &margins)
{
    Q_D(FramelessHelper);

    d->priMaximizedMargins = margins;
}

QMargins FramelessHelper::draggableMargins() const
{
    Q_D(const FramelessHelper);

    return d->priDraggableMargins;
}

QMargins FramelessHelper::maximizedMargins() const
{
    Q_D(const FramelessHelper);

    return d->priMaximizedMargins;
}

void FramelessHelper::addIncludeItem(QWidget *item)
{
    Q_D(FramelessHelper);

    d->includeItems.insert(item);
}

void FramelessHelper::removeIncludeItem(QWidget *item)
{
    Q_D(FramelessHelper);

    d->includeItems.remove(item);
}

void FramelessHelper::addExcludeItem(QWidget *item)
{
    Q_D(FramelessHelper);

    d->excludeItems.insert(item);
}

void FramelessHelper::removeExcludeItem(QWidget *item)
{
    Q_D(FramelessHelper);

    d->excludeItems.remove(item);
}

void FramelessHelper::setTitleBar(QWidget* titleBar)
{
    Q_D(FramelessHelper);
    d->titleBar = titleBar;
}

qreal FramelessHelper::scaleFactor() const
{
    Q_D(const FramelessHelper);

    return d->helper ? d->helper->scaleFactor() : 1.0;
}

bool FramelessHelper::isMaximized() const
{
    Q_D(const FramelessHelper);

    return d->maximized;
}

void FramelessHelper::triggerMinimizeButtonAction()
{
    Q_D(FramelessHelper);

    if (d->window) {
        d->window->showMinimized();
    }
}

void FramelessHelper::triggerMaximizeButtonAction()
{
    Q_D(FramelessHelper);

    if (d->window) {
        if(d->window->isMaximized())
            d->window->showNormal();
        else
            d->window->showMaximized();

//        if (d->window->windowState() & Qt::WindowMaximized) {
//            d->window->showNormal();
//        } else {
//            d->window->showMaximized();
//        }
    }
    qDebug() << "triggerMaximizeButtonAction";
}

void FramelessHelper::triggerCloseButtonAction()
{
    Q_D(FramelessHelper);

    if (d->window) {
        d->window->close();
    }
}

bool FramelessHelper::eventFilter(QObject *obj, QEvent *ev)
{
    Q_D(FramelessHelper);

    if (ev->type() == QEvent::WindowStateChange) {
        bool maximized = d->window->windowState() & Qt::WindowMaximized;
        if (maximized != d->maximized) {
            d->maximized = maximized;
            emit maximizedChanged(maximized);
        }
    } else if (ev->type() == QEvent::WinIdChange) {
        if (nullptr == d->helper) {
            auto w = d->window->windowHandle();

            d->helper = new NativeWindowHelper(w, d,d->mTitleBar);
            connect(d->helper, &NativeWindowHelper::scaleFactorChanged,
                    this, &FramelessHelper::scaleFactorChanged);
            connect(d->helper, &NativeWindowHelper::receiveMessage,this,[this](QString msg){
                emit receiveMessage(msg);
            });
            connect(d->helper, &NativeWindowHelper::dropOpenFile,this,[this](const QStringList& fileUrlList){
                emit dropOpenFile(fileUrlList);
            });
            connect(d->helper, &NativeWindowHelper::dropCreateFile,this,[this](const QStringList& fileUrlList){
                emit dropCreateFile(fileUrlList);
            });
            if (!qFuzzyCompare(d->helper->scaleFactor(), 1.0)) {
                emit scaleFactorChanged(d->helper->scaleFactor());
            }
        }
    }

#if QT_VERSION < QT_VERSION_CHECK(5, 9, 0)
    if ((ev->type() == QEvent::Resize) || (ev->type() == QEvent::WindowStateChange)) {
        if (d->window->windowState() & Qt::WindowMaximized) {
            const QMargins &m = d->priMaximizedMargins;
            int r = GetSystemMetrics(SM_CXFRAME) * 2 - m.left() - m.right();
            int b = GetSystemMetrics(SM_CYFRAME) * 2 - m.top() - m.bottom();

            d->window->setContentsMargins(0, 0, r, b);
        } else {
            d->window->setContentsMargins(0 ,0, 0, 0);
        }
    }
#endif

    return QObject::eventFilter(obj, ev);
}

// class FramelessHelperPrivate

FramelessHelperPrivate::FramelessHelperPrivate()
    : window(nullptr)
    , helper(nullptr)
    , titleBar(nullptr)
    , maximized(false)
{
}

FramelessHelperPrivate::~FramelessHelperPrivate()
{
}

QMargins FramelessHelperPrivate::draggableMargins() const
{
    return priDraggableMargins;
}

QMargins FramelessHelperPrivate::maximizedMargins() const
{
    return priMaximizedMargins;
}

bool FramelessHelperPrivate::hitTest() const
{
    if (!titleBar)
    {
        return false;
    }
    auto pos = titleBar->mapFromGlobal(QCursor::pos());
    auto rc = titleBar->rect();
    if (!rc.contains(pos))
    {
        return false;
    }

    auto child = titleBar->childAt(pos);
    if (!child)
    {
        return true;
    }

    if (dynamic_cast<QLabel*>(child))
    {
        return true;
    }

    if (includeItems.contains(child))
    {
        return true;
    }

    return false;
}
