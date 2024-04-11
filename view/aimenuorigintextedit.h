#ifndef AIMENUORIGINTEXTEDIT_H
#define AIMENUORIGINTEXTEDIT_H

#include <QWidget>
#include <QTextEdit>
#include "./src/pdfeditor/items/itemsettingwin/itempropertyctrlwin.h"
#include "./src/basicwidget/basicitemview.h"
#include "./src/baseabstract/abstracttoolswidget.h"

class MarkupPageCtrlPriv;
class ScrollBarContainer;
class QPropertyAnimation;
class ToolTipWin;

class GenerateWating: public QLabel
{
    Q_OBJECT
    Q_PROPERTY(int animationIndex READ animationIndex WRITE setAnimationIndex NOTIFY animationIndexChanged)
public:
    explicit GenerateWating(QWidget* parent = nullptr);

    int animationIndex();
    void setAnimationIndex(int index);

private:
    int mAnimationIndex;
    QPropertyAnimation *mIndexAnimation;

    void runAnimation(bool run);

protected:
    virtual void showEvent(QShowEvent*) override;
    virtual void closeEvent(QCloseEvent*) override;

signals:
    void animationIndexChanged();
};

class OriginTextEdit: public AbstractItemViewType<QtTextEdit>
{
    Q_OBJECT
public:
    explicit OriginTextEdit(QWidget* parent = nullptr);
    void setOriginText(const QString&);
    QString originText();
    void initView();
    bool isExpand();

private:
    QString mOriginText;
    QString mText;
    QPushButton* mExpandBtn;
    QWidget* mViewContainer;


private:
    void adjustHeight(bool open);
    void adjustShow(bool open);
    int maxHeight(int max);
    void updateBtnPos();
    void updateLineHeight();
    void onExpandBtnClicked(bool checked);

protected:
    virtual void showEvent(QShowEvent *) override;
    virtual void resizeEvent(QResizeEvent*) override;
    virtual void mousePressEvent(QMouseEvent*) override;
signals:
    void sameText(bool same);
    void expand(bool expand);
};

#endif // AIMENUORIGINTEXTEDIT_H
