#pragma once

#include <QColor>
#include <QFrame>
#include <QPair>
#include <QString>
#include <QVector>
#include <QWidget>

class QLabel;

namespace ev {

// Small stat card: caption + big value + colored accent bar.
class StatCard final : public QFrame {
    Q_OBJECT

public:
    explicit StatCard(const QString &caption, const QString &tone,
                      QWidget *parent = nullptr);

    void setCaption(const QString &caption);
    void setValue(const QString &value);

private:
    QLabel *captionLabel_ = nullptr;
    QLabel *valueLabel_ = nullptr;
};

// Line chart painted with QPainter (QtCharts is unavailable on the target).
class LineChartWidget final : public QWidget {
    Q_OBJECT

public:
    explicit LineChartWidget(QWidget *parent = nullptr);

    void setPoints(const QVector<QPair<QString, double>> &points,
                   const QString &unit = QStringLiteral("元"));
    QSize minimumSizeHint() const override { return {360, 240}; }

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QVector<QPair<QString, double>> points_;
    QString unit_;
};

// Donut chart with a right-hand legend.
class PieChartWidget final : public QWidget {
    Q_OBJECT

public:
    struct Slice {
        QString label;
        double value = 0.0;
        QColor color;
    };

    explicit PieChartWidget(QWidget *parent = nullptr);

    void setSlices(const QVector<Slice> &slices, const QString &unit = QStringLiteral("台"));
    QSize minimumSizeHint() const override { return {300, 240}; }

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QVector<Slice> slices_;
    QString unit_;
};

} // namespace ev
