#include "NativeWindowHelper.h"
#include "NativeWindowHelper_p.h"

#include <windows.h>
#include <windowsx.h>
#include <winuser.h>

#include <QApplication>
#include <QScreen>
#include <QEvent>
#include <QtWin>
#include <QDebug>
#include <QTimer>

#include <QOperatingSystemVersion>
#include <ShlObj_core.h>

#include "NativeWindowFilter.h"
#include "titlebarhelper.h"
#include "./src/util/loginutil.h"
#include "./src/util/coreutil.h"
#include "./src/bar/titlebar.h"
#include "./src/customwidget/cshowtipsbtn.h"
#pragma comment (lib, "Shell32.lib")

const ULONG_PTR CUSTOM_TYPE_LOGIN = 10000;
const ULONG_PTR CUSTOM_TYPE_LOGOUT = 10001;

#if defined(__GNUC__)
//我电脑上的mingw报错，说没定义，那咋就给它定义一个
//make mingw happy
#define WM_DPICHANGED       0x02E0
#endif

// class NativeWindowHelper

NativeWindowHelper::NativeWindowHelper(QWindow *window, NativeWindowTester *tester,TitleBar* titleBar)
    : QObject(window)
    , d_ptr(new NativeWindowHelperPrivate())
    , mTitlebarHelper(new TitleBarHelper(titleBar))
{
    d_ptr->q_ptr = this;

    Q_D(NativeWindowHelper);

    Q_CHECK_PTR(window);
    Q_CHECK_PTR(tester);

    d->window = window;
    d->tester = tester;

    if (d->window) {
        d->scaleFactor = d->window->screen()->devicePixelRatio();

        if (d->window->flags() & Qt::FramelessWindowHint) {
            d->window->installEventFilter(this);
            d->updateWindowStyle();
        }
    }
}

NativeWindowHelper::NativeWindowHelper(QWindow *window)
    : QObject(window)
    , d_ptr(new NativeWindowHelperPrivate())
{
    d_ptr->q_ptr = this;

    Q_D(NativeWindowHelper);

    Q_CHECK_PTR(window);
    d->window = window;

    if (d->window) {
        d->scaleFactor = d->window->screen()->devicePixelRatio();
        if (d->window->flags() & Qt::FramelessWindowHint) {
            d->window->installEventFilter(this);
            d->updateWindowStyle();
        }
    }
}

NativeWindowHelper::~NativeWindowHelper()
{
}

bool NativeWindowHelper::nativeEventFilter(void *msg, long *result)
{
    Q_D(NativeWindowHelper);

    Q_CHECK_PTR(d->window);

    LPMSG lpMsg = reinterpret_cast<LPMSG>(msg);
    WPARAM wParam = lpMsg->wParam;
    LPARAM lParam = lpMsg->lParam;

    if (s_flag) {
        ChangeWindowMessageFilterEx ( HWND(d->window->winId()) , WM_DROPFILES , MSGFLT_ALLOW , nullptr );
        ChangeWindowMessageFilterEx ( HWND(d->window->winId()) , WM_COPYDATA , MSGFLT_ALLOW , nullptr );
        ChangeWindowMessageFilterEx ( HWND(d->window->winId()) , 0x49 , MSGFLT_ALLOW , nullptr );
        s_flag = false;
    }
    if(WM_DROPFILES == lpMsg->message )
    {
        if(IsUserAnAdmin())
        {
            HDROP hDropInfo = (HDROP)wParam;
            UINT count = DragQueryFileW(hDropInfo, 0xFFFFFFFF, NULL, 0);
			QStringList fileUrlList;
            QStringList createList;
            if (count)
            {
                for (UINT i = 0; i < count; i++)
                {
                    int size = DragQueryFileW(hDropInfo, i,nullptr,0) + 2;
                    auto filePath = CoreUtil::getBuffer<wchar_t>(size);
                    DragQueryFileW(hDropInfo, i, filePath.data(), size);
                    QString fileUrl = QString::fromUtf16((ushort*)filePath.data());
					if (fileUrl.endsWith(".pdf", Qt::CaseInsensitive)) {
						fileUrlList.push_back(fileUrl);
                    } else if(GuiUtil::isAllowToCreated(fileUrl,true)) {
                        createList.push_back(fileUrl);
                    }
                }
            }

            if (!fileUrlList.isEmpty()){
                emit dropOpenFile(fileUrlList);
            }
            if (!createList.isEmpty()){
                emit dropCreateFile(createList);
            }
            DragFinish(hDropInfo);
        }
    }

    if( WM_COPYDATA == lpMsg->message){
        COPYDATASTRUCT *cds = reinterpret_cast<COPYDATASTRUCT*>(lpMsg->lParam);
        if (cds->dwData == CUSTOM_TYPE_LOGIN)
        {
            QString strMessage = QString::fromUtf8(reinterpret_cast<char*>(cds->lpData), cds->cbData);
            qDebug()<<"CUSTOM_TYPE_LOGIN"<<strMessage;
            QTimer::singleShot(100,this,[this,strMessage](){
                emit receiveMessage(strMessage);
            });
        }
        else if( cds->dwData == CUSTOM_TYPE_LOGOUT)
        {
            QString strMessage = QString::fromUtf8(reinterpret_cast<char*>(cds->lpData), cds->cbData);
            qDebug()<<"strMessage"<<strMessage;
            emit receiveMessage(strMessage);
        }
        *result = 1;
        return true;
    }
    else if (WM_NCHITTEST == lpMsg->message) {

        *result = d->hitTest();
        if(*result != HTCLIENT){
            return true;
        }

        if (Qt::WindowMaximized != d->window->windowState() || QCursor::pos().y() >= 0)
        {
            auto maxBtn = mTitlebarHelper->getMaxBtn();
            if (maxBtn->rect().contains(maxBtn->mapFromGlobal(QCursor::pos())))
            {
                mTitlebarHelper->mouseEnterLeaveDeal();
                *result = HTMAXBUTTON;
                return true;
            }
        }
        return false;
    } else if (WM_NCACTIVATE == lpMsg->message) {
        if (!QtWin::isCompositionEnabled()) {
            if (result) *result = 1;
            return true;
        }
    } else if (WM_NCCALCSIZE == lpMsg->message) {
        if (TRUE == wParam) {
            if (d->isMaximized()) {
                NCCALCSIZE_PARAMS &params = *reinterpret_cast<NCCALCSIZE_PARAMS *>(lParam);

                QRect g = d->availableGeometry();
                QMargins m = d->maximizedMargins();

                params.rgrc[0].top = g.top() - m.top();
                params.rgrc[0].left = g.left() - m.left();
                params.rgrc[0].right = g.right() + m.right();
                params.rgrc[0].bottom = g.bottom() + m.bottom();
            }

            if (result) *result = (QOperatingSystemVersion::current() < QOperatingSystemVersion::Windows8? WVR_REDRAW: 0);
            return true;
        }
    } else if (WM_GETMINMAXINFO == lpMsg->message) {
        LPMINMAXINFO lpMinMaxInfo = reinterpret_cast<LPMINMAXINFO>(lParam);

        QRect g = d->availableGeometry();
        QMargins m = d->maximizedMargins();

        lpMinMaxInfo->ptMaxPosition.x = - m.left();
        lpMinMaxInfo->ptMaxPosition.y =  - m.top();
        lpMinMaxInfo->ptMaxSize.x = g.right() - g.left() + m.left() + m.right();
        lpMinMaxInfo->ptMaxSize.y = g.bottom() - g.top() + m.top() + m.bottom();

        lpMinMaxInfo->ptMinTrackSize.x = d->window->minimumWidth();
        lpMinMaxInfo->ptMinTrackSize.y = d->window->minimumHeight();
        lpMinMaxInfo->ptMaxTrackSize.x = d->window->maximumWidth();
        lpMinMaxInfo->ptMaxTrackSize.y = d->window->maximumHeight();

        if (result) *result = 0;
        return true;
    } else if (WM_NCLBUTTONDBLCLK == lpMsg->message) {
        auto minimumSize = d->window->minimumSize();
        auto maximumSize = d->window->maximumSize();
        if ((minimumSize.width() >= maximumSize.width())
                || (minimumSize.height() >= maximumSize.height())) {
            if (result) *result = 0;
            return true;
        }
    } else if (WM_DPICHANGED == lpMsg->message) {
        qreal old = d->scaleFactor;
        if (HIWORD(wParam) < 144) {
            d->scaleFactor = 1.0;
        } else {
            d->scaleFactor = 2.0;
        }

        if (!qFuzzyCompare(old, d->scaleFactor)) {
            emit scaleFactorChanged(d->scaleFactor);
        }

        auto hWnd = reinterpret_cast<HWND>(d->window->winId());
        auto rect = reinterpret_cast<LPRECT>(lParam);
        SetWindowPos(hWnd,
                     NULL,
                     rect->left,
                     rect->top,
                     rect->right - rect->left,
                     rect->bottom - rect->top,
                     SWP_NOZORDER | SWP_NOACTIVATE);
    }   
    else if(WM_MOUSEMOVE == lpMsg->message){
        mTitlebarHelper->mouseEnterLeaveDeal();
        return false;
    }
    else if(WM_NCLBUTTONDOWN == lpMsg->message) //过滤系统最大化按钮按下事件 交由UPDF处理
    {
        if(lpMsg->wParam == HTMAXBUTTON) {
            return mTitlebarHelper->mousePressDeal();
        }
        return false;
    }
    else if(WM_NCLBUTTONUP == lpMsg->message) //过滤系统最大化按钮按下事件 交由UPDF处理
    {
        if(lpMsg->wParam == HTMAXBUTTON) {
            return mTitlebarHelper->mouseRealseDeal();
        }
        return false;
    }
    else if (WM_POWERBROADCAST == lpMsg->message 
        && PBT_APMRESUMESUSPEND == wParam)
    {

        if (qApp->screens().size() > 1)
        {
            QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
            QApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
        }
    }
    return false;
}

bool NativeWindowHelper::eventFilter(QObject *obj, QEvent *ev)
{
    Q_D(NativeWindowHelper);

    if (ev->type() == QEvent::WinIdChange) {
        d->updateWindowStyle();
    } else if (ev->type() == QEvent::Show) {
        d->updateWindowStyle();
    }

    return QObject::eventFilter(obj, ev);
}

qreal NativeWindowHelper::scaleFactor() const
{
    Q_D(const NativeWindowHelper);

    return d->scaleFactor;
}

// class NativeWindowHelperPrivate

NativeWindowHelperPrivate::NativeWindowHelperPrivate()
    : q_ptr(nullptr)
    , window(nullptr)
    , tester(nullptr)
    , oldWindow(NULL)
    , scaleFactor(1.0)
{
}

NativeWindowHelperPrivate::~NativeWindowHelperPrivate()
{
    if (window)
        NativeWindowFilter::deliver(window, nullptr);
}

void NativeWindowHelperPrivate::updateWindowStyle()
{
    Q_Q(NativeWindowHelper);

    Q_CHECK_PTR(window);

    HWND hWnd = reinterpret_cast<HWND>(window->winId());
    if (NULL == hWnd)
        return;
    else if (oldWindow == hWnd) {
        return;
    }
    oldWindow = hWnd;

    NativeWindowFilter::deliver(window, q);

    //QOperatingSystemVersion currentVersion = QOperatingSystemVersion::current();
    //if (currentVersion < QOperatingSystemVersion::Windows8) {
    //    return;
    //}

    LONG oldStyle = WS_OVERLAPPEDWINDOW | WS_THICKFRAME
            | WS_CAPTION | WS_SYSMENU | WS_MAXIMIZEBOX | WS_MINIMIZEBOX;;
    LONG newStyle = WS_POPUP            | WS_THICKFRAME;

    if (QtWin::isCompositionEnabled())
        newStyle |= WS_CAPTION;

    if (window->flags() & Qt::CustomizeWindowHint) {
        if (window->flags() & Qt::WindowSystemMenuHint)
            newStyle |= WS_SYSMENU;
        if (window->flags() & Qt::WindowMinimizeButtonHint)
            newStyle |= WS_MINIMIZEBOX;
        if (window->flags() & Qt::WindowMaximizeButtonHint)
            newStyle |= WS_MAXIMIZEBOX;
    } else {
        newStyle |= WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX;
    }

    LONG currentStyle = GetWindowLong(hWnd, GWL_STYLE);
    SetWindowLong(hWnd, GWL_STYLE, (currentStyle & ~oldStyle) | newStyle);

    SetWindowPos(hWnd, NULL, 0, 0, 0 , 0,
                 SWP_NOOWNERZORDER | SWP_NOZORDER |
                 SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE);

    if (QtWin::isCompositionEnabled())
        QtWin::extendFrameIntoClientArea(window, 1, 0, 1, 1);
}

int NativeWindowHelperPrivate::hitTest() const
{
    Q_CHECK_PTR(window);

    auto pos = window->mapFromGlobal(QCursor::pos());
    auto x = pos.x();
    auto y = pos.y();

    enum RegionMask {
        Client = 0x0000,
        Top    = 0x0001,
        Left   = 0x0010,
        Right  = 0x0100,
        Bottom = 0x1000,
    };

    auto g = window->frameGeometry();
    auto wfg = QRect(window->mapFromGlobal(g.topLeft()), window->mapFromGlobal(g.bottomRight())).normalized();
    QMargins draggableMargins
            = this->draggableMargins();

    int top = draggableMargins.top();
    int left = draggableMargins.left();
    int right = draggableMargins.right();
    int bottom = draggableMargins.bottom();

    if (top <= 0)
        top = GetSystemMetrics(SM_CYFRAME);
    if (left <= 0)
        left = GetSystemMetrics(SM_CXFRAME);
    if (right <= 0)
        right = GetSystemMetrics(SM_CXFRAME);
    if (bottom <= 0)
        bottom = GetSystemMetrics(SM_CYFRAME);

    auto result =
            (Top *    (y < (wfg.top() +    top))) |
            (Left *   (x < (wfg.left() +   left))) |
            (Right *  (x > (wfg.right() -  right))) |
            (Bottom * (y > (wfg.bottom() - bottom)));

    bool wResizable = window->minimumWidth() < window->maximumWidth();
    bool hResizable = window->minimumHeight() < window->maximumHeight();

    switch (result) {    
    case Top | Left    : return wResizable && hResizable ? HTTOPLEFT     : HTCLIENT;
    case Top           : return hResizable               ? HTTOP         : HTCLIENT;
    case Top | Right   : return wResizable && hResizable ? HTTOPRIGHT    : HTCLIENT;
    case Right         : return wResizable               ? HTRIGHT       : HTCLIENT;
    case Bottom | Right: return wResizable && hResizable ? HTBOTTOMRIGHT : HTCLIENT;
    case Bottom        : return hResizable               ? HTBOTTOM      : HTCLIENT;
    case Bottom | Left : return wResizable && hResizable ? HTBOTTOMLEFT  : HTCLIENT;
    case Left          : return wResizable               ? HTLEFT        : HTCLIENT;
    }

    return ((nullptr != tester) && tester->hitTest()) ?  HTCAPTION: HTCLIENT;
}

bool NativeWindowHelperPrivate::isMaximized() const
{
    Q_CHECK_PTR(window);

    HWND hWnd = reinterpret_cast<HWND>(window->winId());
    if (NULL == hWnd)
        return false;

    WINDOWPLACEMENT windowPlacement;
    if (!GetWindowPlacement(hWnd, &windowPlacement))
        return false;

    return (SW_MAXIMIZE == windowPlacement.showCmd);
}

QMargins NativeWindowHelperPrivate::draggableMargins() const
{
    return tester ? tester->draggableMargins() * scaleFactor : QMargins();
}

QMargins NativeWindowHelperPrivate::maximizedMargins() const
{
    return tester ? tester->maximizedMargins() * scaleFactor : QMargins();
}

QRect NativeWindowHelperPrivate::availableGeometry() const
{
    MONITORINFO mi{0};
    mi.cbSize = sizeof(MONITORINFO);

    auto hWnd = reinterpret_cast<HWND>(window->winId());
    auto hMonitor = MonitorFromWindow(hWnd, MONITOR_DEFAULTTONEAREST);
    if (!hMonitor || !GetMonitorInfoW(hMonitor, &mi)) {
        //Q_ASSERT(NULL != hMonitor);
        return window->screen()->availableGeometry();
    }

    if (mi.rcWork.left == mi.rcMonitor.left && mi.rcWork.right == mi.rcMonitor.right
        && mi.rcWork.top == mi.rcMonitor.top && mi.rcWork.bottom == mi.rcMonitor.bottom) {
        return QRect(mi.rcWork.left, mi.rcWork.top, mi.rcWork.right - mi.rcWork.left + 1, mi.rcWork.bottom - mi.rcWork.top);
    }

    return QRect(mi.rcWork.left, mi.rcWork.top, mi.rcWork.right - mi.rcWork.left + 1, mi.rcWork.bottom - mi.rcWork.top + 1);
}
