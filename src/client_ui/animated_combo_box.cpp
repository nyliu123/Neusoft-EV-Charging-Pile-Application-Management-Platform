#include "client_ui/animated_combo_box.h"

#include <QEasingCurve>
#include <QPainter>
#include <QPaintEvent>
#include <QStyleOptionComboBox>

namespace ev {

AnimatedComboBox::AnimatedComboBox(QWidget *parent)
    : QComboBox(parent), arrowAnimation_(this, "arrowAngle", this)
{
    arrowAnimation_.setDuration(180);
    arrowAnimation_.setEasingCurve(QEasingCurve::InOutCubic);
}

qreal AnimatedComboBox::arrowAngle() const
{
    return arrowAngle_;
}

void AnimatedComboBox::setArrowAngle(qreal angle)
{
    arrowAngle_ = angle;
    update();
}

void AnimatedComboBox::showPopup()
{
    QComboBox::showPopup();
    animateArrow(180.0);
}

void AnimatedComboBox::hidePopup()
{
    QComboBox::hidePopup();
    animateArrow(0.0);
}

void AnimatedComboBox::paintEvent(QPaintEvent *event)
{
    QComboBox::paintEvent(event);

    QStyleOptionComboBox option;
    initStyleOption(&option);
    const QRect arrowRect = style()->subControlRect(
        QStyle::CC_ComboBox, &option, QStyle::SC_ComboBoxArrow, this);
    const QPointF center = arrowRect.center();

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    QPen pen(isEnabled() ? QColor(QStringLiteral("#5f6368"))
                         : QColor(QStringLiteral("#a0a6af")));
    pen.setWidthF(1.6);
    pen.setCapStyle(Qt::RoundCap);
    pen.setJoinStyle(Qt::RoundJoin);
    painter.setPen(pen);
    painter.translate(center);
    painter.rotate(arrowAngle_);
    painter.drawLine(QPointF(-4.0, -2.0), QPointF(0.0, 2.0));
    painter.drawLine(QPointF(0.0, 2.0), QPointF(4.0, -2.0));
}

void AnimatedComboBox::animateArrow(qreal targetAngle)
{
    // A frequent form interaction: no decorative motion or keyboard delay.
    arrowAnimation_.stop();
    setArrowAngle(targetAngle);
}

} // namespace ev
