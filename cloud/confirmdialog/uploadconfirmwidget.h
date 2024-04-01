#ifndef UPLOADCONFIRMWIDGET_H
#define UPLOADCONFIRMWIDGET_H

#include "cframelesswidget.h"
#include <QPushButton>
#include <QLabel>
#include <QRadioButton>
#include <QButtonGroup>

class UploadConfirmWidget : public CFramelessWidget
{
    Q_OBJECT
public:
    enum ChoiceType {
        Update = 0,
        Discard,
        SaveAs,
    };
    Q_ENUM(ChoiceType)
    explicit UploadConfirmWidget(QWidget *parent = nullptr);
    void initView();
signals:
    void uploadChoice(ChoiceType type);
private slots:
    void onChoicePressed(int type);
private:
    QWidget *mVisiblePanel;
    QLabel* mIconLabel;
    QLabel* mTitleLabel;
    QLabel* mTipLabel;
    QButtonGroup* mChoiceBtnGroup;
    QLabel* mWarnLabel;
    QPushButton* mConfirmBtn;
};

#endif // UPLOADCONFIRMWIDGET_H
