#include "uploadconfirmwidget.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGraphicsDropShadowEffect>

UploadConfirmWidget::UploadConfirmWidget(QWidget *parent)
    : CFramelessWidget{parent}
{
    initView();
    setFixedSize(265 + 16, 340 + 16);
    mVisiblePanel->setFixedSize(265, 340);
}

void UploadConfirmWidget::initView()
{
    mVisiblePanel = new QWidget(this);
    mVisiblePanel->setObjectName("visiblePanel");

    mIconLabel = new QLabel(this);
    mIconLabel->setFixedSize(56, 56);
    mIconLabel->setObjectName("logoIcon");
    mTitleLabel = new QLabel(this);
    mTitleLabel->setFixedHeight(16);
    mTitleLabel->setObjectName("titleLabel");
    mTitleLabel->setAlignment(Qt::AlignCenter);
    mTitleLabel->setText(tr("New version found"));
    mTipLabel = new QLabel(this);
    mTipLabel->setObjectName("tipLabel");
    mTipLabel->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
    mTipLabel->setWordWrap(true);
    mTipLabel->setMinimumHeight(32);
    mTipLabel->setText(tr("A new version is detected in the cloud. How would you like to proceed?"));

    mWarnLabel = new QLabel(this);
    mWarnLabel->setWordWrap(true);
    mWarnLabel->setFixedHeight(32);
    mWarnLabel->setObjectName("warnLabel");
    mWarnLabel->setAlignment(Qt::AlignCenter);
    mWarnLabel->setVisible(false);
    QButtonGroup *buttonGroup = new QButtonGroup(this);
    QRadioButton *updateBtn = new QRadioButton(tr("Update to the latest version"));
    updateBtn->setObjectName("radioBtn");
    updateBtn->setFixedSize(214, 12);
    QRadioButton *discardBtn = new QRadioButton(tr("Discard new version"));
    discardBtn->setObjectName("radioBtn");
    discardBtn->setFixedSize(214, 12);
    QRadioButton *saveAsBtn = new QRadioButton(tr("Save a copy of new version"));
    saveAsBtn->setObjectName("radioBtn");
    saveAsBtn->setFixedSize(214, 12);

    buttonGroup->addButton(updateBtn, Update);
    buttonGroup->addButton(discardBtn, Discard);
    buttonGroup->addButton(saveAsBtn, SaveAs);
    connect(buttonGroup, &QButtonGroup::idPressed, this, &UploadConfirmWidget::onChoicePressed);
    mConfirmBtn = new QPushButton(this);
    connect(mConfirmBtn, &QPushButton::clicked, this, [=]{
        emit uploadChoice((ChoiceType)buttonGroup->checkedId());
        closeWin(Accepted);
    });
    mConfirmBtn->setObjectName("confirmBtn");
    mConfirmBtn->setFixedSize(214, 32);
    mConfirmBtn->setText(tr("Confirm"));
    mConfirmBtn->setEnabled(false);

    QVBoxLayout *contentLayout = new QVBoxLayout(mVisiblePanel);
    contentLayout->setContentsMargins(24, 24, 24, 24);
    contentLayout->addWidget(mIconLabel, 0, Qt::AlignCenter);
    contentLayout->addSpacing(12);
    contentLayout->addWidget(mTitleLabel, 0, Qt::AlignCenter);
    contentLayout->addSpacing(8);
    contentLayout->addWidget(mTipLabel, 0, Qt::AlignCenter);
    contentLayout->addSpacing(8);
    contentLayout->addWidget(updateBtn, 0, Qt::AlignCenter);
    contentLayout->addSpacing(4);
    contentLayout->addWidget(discardBtn, 0, Qt::AlignCenter);
    contentLayout->addSpacing(4);
    contentLayout->addWidget(saveAsBtn, 0, Qt::AlignCenter);
    contentLayout->addSpacing(4);
    contentLayout->addWidget(mWarnLabel, 0, Qt::AlignCenter);
    contentLayout->addSpacing(12);
    contentLayout->addWidget(mConfirmBtn, 0, Qt::AlignCenter);

    QVBoxLayout *layout = new QVBoxLayout;
    layout->addWidget(mVisiblePanel);
    layout->setSpacing(0);
    layout->setContentsMargins(8, 8, 8, 8);
    setLayout(layout);

    QGraphicsDropShadowEffect *shadowEffect = new QGraphicsDropShadowEffect(this);
    shadowEffect->setOffset(0, 0);
    shadowEffect->setColor(QColor(0, 0, 0, 80));
    shadowEffect->setBlurRadius(16);
    mVisiblePanel->setGraphicsEffect(shadowEffect);

}

void UploadConfirmWidget::onChoicePressed(int type)
{
    mConfirmBtn->setEnabled(true);
    mWarnLabel->setVisible(true);
    setFixedSize(265 + 16, 366 + 16);
    mVisiblePanel->setFixedSize(265, 366);

    switch (type) {
    case Update:
        {
           mWarnLabel->setText(tr("Update to the latest version will diacard your existing version."));
        }
        break;
    case Discard:
        {
           mWarnLabel->setText(tr("Discard the new version and continue editing the current version."));
        }
        break;
    case SaveAs:
        {
           mWarnLabel->setText(tr("Save a copy of new version and continue editing the current version."));
        }
        break;
    default:
        break;
    }
}
