#ifndef CHATINPUTWIDGET_H
#define CHATINPUTWIDGET_H

#include <QTextEdit>
#include <QEvent>
#include <QPushButton>
#include <QLabel>
#include "./src/pdfeditor/priv/backgroundwatermark/subwidgets/inputtextedit.h"
#include "./src/customwidget/cshowtipsbtn.h"

class ChatInputWidget : public InputTextEdit
{
    Q_OBJECT
public:
    explicit ChatInputWidget(QWidget *parent = nullptr);
    void setFuncButtonVisiable(bool visiable);
    void setFuncButtonChecked(bool checked);
    void setFuncButtonStyle(const QString &type);
    bool funButtonVisiable();
protected:
    virtual bool eventFilter(QObject* watched, QEvent* event) override;
    virtual void resizeEvent(QResizeEvent *event) override;

signals:
    void commitText(const QString &text);
    void adjustHeight(int height);
    void triggerFunc();
private slots:
    void commit();
    void onTextChanged();
private:
    QPushButton* mSendBtn;
    CShowTipsBtn* mFuncBtn;
    QLabel* mTopGradientLabel;
    bool mFuncBtnVisiable;
};

#endif // CHATINPUTWIDGET_H
