#ifndef FRAMELESSHELPER_H
#define FRAMELESSHELPER_H

#include <QWidget>
#include "./src/bar/titlebar.h"

class FramelessHelperPrivate;
class FramelessHelper : public QObject
{
    Q_OBJECT
    Q_DECLARE_PRIVATE(FramelessHelper)

public:
    explicit FramelessHelper(TitleBar* titleBar,QWidget *parent = nullptr);
    virtual ~FramelessHelper();

public:
    void setDraggableMargins(int left, int top, int right, int bottom);
    void setMaximizedMargins(int left, int top, int right, int bottom);
public:
    void setDraggableMargins(const QMargins &margins);
    void setMaximizedMargins(const QMargins &margins);
public:
    QMargins draggableMargins() const;
    QMargins maximizedMargins() const;

public:
    void addIncludeItem(QWidget *item);
    void removeIncludeItem(QWidget *item);
public:
    void addExcludeItem(QWidget *item);
    void removeExcludeItem(QWidget *item);

signals:
    void receiveMessage(QString msg);
    void dropOpenFile(const QStringList& url);
    void dropCreateFile(const QStringList& url);

public slots:
    void setTitleBar(QWidget* titleBar);

signals:
    void scaleFactorChanged(qreal factor);
public:
    qreal scaleFactor() const;

signals:
    void maximizedChanged(bool maximized);
public:
    bool isMaximized() const;

public slots:
    void triggerMinimizeButtonAction();
    void triggerMaximizeButtonAction();
    void triggerCloseButtonAction();

protected:
    bool eventFilter(QObject *obj, QEvent *ev) final;
protected:
    QScopedPointer<FramelessHelperPrivate> d_ptr;
};

#endif // FRAMELESSHELPER_H
