#ifndef OPENCONFIRMWIDGET_H
#define OPENCONFIRMWIDGET_H

#include "cframelesswidget.h"
#include <QPushButton>
#include <QLabel>
#include <QCheckBox>


class OpenConfirmWidget : public CFramelessWidget
{
    Q_OBJECT
public:
    explicit OpenConfirmWidget(QWidget *parent = nullptr);
    void initView();
signals:
    void rememberConfirm(bool continueAction, bool check);
private:
    QWidget *mVisiblePanel;
    QLabel* mIconLabel;
    QLabel* mTitleLabel;
    QLabel* mTipLabel;
    QCheckBox* mCheckBox;
    QPushButton* mConfirmBtn;
    QPushButton* mCancelBtn;

};

#endif // OPENCONFIRMWIDGET_H
