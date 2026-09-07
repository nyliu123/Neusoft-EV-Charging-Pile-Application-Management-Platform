#pragma once

#include <QComboBox>
#include <QPropertyAnimation>

namespace ev {

class AnimatedComboBox final : public QComboBox {
    Q_OBJECT
    Q_PROPERTY(qreal arrowAngle READ arrowAngle WRITE setArrowAngle)

public:
    explicit AnimatedComboBox(QWidget *parent = nullptr);

    qreal arrowAngle() const;
    void setArrowAngle(qreal angle);

    void showPopup() override;
    void hidePopup() override;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    void animateArrow(qreal targetAngle);

    qreal arrowAngle_ = 0.0;
    QPropertyAnimation arrowAnimation_;
};

} // namespace ev
