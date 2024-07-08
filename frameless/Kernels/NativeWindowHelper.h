#ifndef NATIVEWINDOWHELPER_H
#define NATIVEWINDOWHELPER_H

#include <QPoint>
#include <QWindow>
#include <QMargins>

class NativeWindowTester
{
public:
    virtual QMargins draggableMargins() const = 0;
    virtual QMargins maximizedMargins() const = 0;

    virtual bool hitTest() const = 0;
};

class NativeWindowHelperPrivate;
class TitleBarHelper;
class TitleBar;
class NativeWindowHelper : public QObject
{
    Q_OBJECT
    Q_DECLARE_PRIVATE(NativeWindowHelper)

public:
    NativeWindowHelper(QWindow *window, NativeWindowTester *tester,TitleBar* titleBar);
    explicit NativeWindowHelper(QWindow *window);
    ~NativeWindowHelper();

public:
    bool nativeEventFilter(void *msg, long *result);
protected:
    bool eventFilter(QObject *obj, QEvent *ev) final;
protected:
    QScopedPointer<NativeWindowHelperPrivate> d_ptr;
    QScopedPointer<TitleBarHelper> mTitlebarHelper;

signals:
    void scaleFactorChanged(qreal factor);
    void receiveMessage(QString msg);
    void dropOpenFile(const QStringList& url);
    void dropCreateFile(const QStringList& url);
    void doubleClickHTCAPTION();
public:
    qreal scaleFactor() const;
private:
    bool s_flag = true;
};

#endif // NATIVEWINDOWHELPER_H
