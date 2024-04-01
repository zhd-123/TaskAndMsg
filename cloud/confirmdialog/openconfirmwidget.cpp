#include "openconfirmwidget.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGraphicsDropShadowEffect>

OpenConfirmWidget::OpenConfirmWidget(QWidget *parent)
    : CFramelessWidget{parent}
{
    initView();
    setFixedSize(265 + 16, 300 + 16);
    mVisiblePanel->setFixedSize(265, 300);
}

void OpenConfirmWidget::initView()
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
    mTipLabel->setWordWrap(true);
    mTipLabel->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
    mTipLabel->setText(tr("A new version is detected in the cloud. Would you like to open this file with the latest version?"));

    mCheckBox = new QCheckBox(tr("Remember my choice"), this);
    mCheckBox->setFixedHeight(16);
    mCheckBox->setObjectName("checkBox");
    mConfirmBtn = new QPushButton(this);
    connect(mConfirmBtn, &QPushButton::clicked, this, [=]{
        emit rememberConfirm(true, mCheckBox->isChecked());
        closeWin(Accepted);
    });
    mConfirmBtn->setObjectName("confirmBtn");
    mConfirmBtn->setFixedSize(214, 32);
    mConfirmBtn->setText(tr("Yes"));

    mCancelBtn = new QPushButton(this);
    connect(mCancelBtn, &QPushButton::clicked, this, [=]{
        emit rememberConfirm(false, mCheckBox->isChecked());
        closeWin(Cancel);
    });
    mCancelBtn->setObjectName("frameBtn");
    mCancelBtn->setFixedSize(214, 32);
    mCancelBtn->setText(tr("No"));

    QVBoxLayout *contentLayout = new QVBoxLayout(mVisiblePanel);
    contentLayout->setContentsMargins(24, 24, 24, 10);
    contentLayout->addWidget(mIconLabel, 0, Qt::AlignCenter);
    contentLayout->addSpacing(16);
    contentLayout->addWidget(mTitleLabel, 0, Qt::AlignCenter);
    contentLayout->addSpacing(6);
    contentLayout->addWidget(mTipLabel, 0, Qt::AlignCenter);
    contentLayout->addSpacing(12);
    contentLayout->addWidget(mConfirmBtn, 0, Qt::AlignCenter);
    contentLayout->addSpacing(8);
    contentLayout->addWidget(mCancelBtn, 0, Qt::AlignCenter);
    contentLayout->addSpacing(8);
    contentLayout->addWidget(mCheckBox, 0, Qt::AlignCenter);
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
