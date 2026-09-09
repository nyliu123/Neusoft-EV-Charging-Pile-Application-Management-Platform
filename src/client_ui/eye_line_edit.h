#pragma once

#include <QLineEdit>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QToolButton>

namespace ev {

class EyeLineEdit final : public QLineEdit {
public:
    explicit EyeLineEdit(QWidget *parent=nullptr):QLineEdit(parent),eye_(new QToolButton(this))
    {
        eye_->setCursor(Qt::PointingHandCursor);
        eye_->setFocusPolicy(Qt::NoFocus);
        eye_->setFixedSize(34,34);
        eye_->setIconSize(QSize(24,24));
        eye_->setStyleSheet(QStringLiteral("QToolButton{border:none;background:transparent;padding:0;}"));
        setTextMargins(0,0,48,0);
    }

    QToolButton *eyeButton() const{return eye_;}
    bool hasStoredMask() const{return storedMask_;}
    void setStoredMask(bool enabled)
    {
        storedMask_=enabled;
        if(enabled){setEchoMode(QLineEdit::Password);setText(QString(16,QLatin1Char('x')));}
        else clear();
    }
    void setEyeVisible(bool visible)
    {
        eye_->setIcon(QIcon(visible?QStringLiteral(":/icons/eye-off.svg")
                                  :QStringLiteral(":/icons/eye.svg")));
        eye_->setToolTip(visible?QStringLiteral("隐藏"):QStringLiteral("显示"));
    }

protected:
    void resizeEvent(QResizeEvent *event) override
    {
        QLineEdit::resizeEvent(event);
        eye_->move(width()-eye_->width()-8,(height()-eye_->height())/2);
        eye_->raise();
    }
    void keyPressEvent(QKeyEvent *event) override
    {
        if(storedMask_ && (!event->text().isEmpty() || event->key()==Qt::Key_Backspace
            || event->key()==Qt::Key_Delete || event->matches(QKeySequence::Paste))){
            storedMask_=false;clear();
        }
        QLineEdit::keyPressEvent(event);
    }
    void mousePressEvent(QMouseEvent *event) override
    {
        QLineEdit::mousePressEvent(event);
        if(storedMask_)selectAll();
    }

private:
    QToolButton *eye_;
    bool storedMask_=false;
};

}
