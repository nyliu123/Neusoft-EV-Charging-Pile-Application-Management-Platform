#include "admin_charts.h"

#include <QFont>
#include <QLabel>
#include <QHBoxLayout>
#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>
#include <QVBoxLayout>

#include <cmath>

namespace ev {

namespace {

double niceMax(double value)
{
    if (value <= 0.0) {
        return 1.0;
    }
    const double exponent = std::floor(std::log10(value));
    const double base = std::pow(10.0, exponent);
    static const double multipliers[] = {1.0, 2.0, 5.0, 10.0};
    for (const double multiplier : multipliers) {
        if (base * multiplier >= value) {
            return base * multiplier;
        }
    }
    return base * 10.0;
}

} // namespace

StatCard::StatCard(const QString &caption, const QString &tone, QWidget *parent)
    : QFrame(parent)
{
    setProperty("uiClass", "statCard");

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(16, 14, 16, 14);
    layout->setSpacing(12);

    auto *bar = new QFrame(this);
    bar->setObjectName(QStringLiteral("statAccent"));
    bar->setProperty("tone", tone);
    bar->setFixedWidth(4);
    layout->addWidget(bar);

    auto *textLayout = new QVBoxLayout();
    textLayout->setSpacing(6);

    captionLabel_ = new QLabel(caption, this);
    captionLabel_->setProperty("uiClass", "muted");

    valueLabel_ = new QLabel(QStringLiteral("--"), this);
    valueLabel_->setProperty("uiClass", "statValue");

    textLayout->addWidget(captionLabel_);
    textLayout->addWidget(valueLabel_);
    layout->addLayout(textLayout);
    layout->addStretch();
}

void StatCard::setCaption(const QString &caption)
{
    captionLabel_->setText(caption);
}

void StatCard::setValue(const QString &value)
{
    valueLabel_->setText(value);
}

LineChartWidget::LineChartWidget(QWidget *parent)
    : QWidget(parent)
{
}

void LineChartWidget::setPoints(const QVector<QPair<QString, double>> &points,
                                const QString &unit)
{
    points_ = points;
    unit_ = unit;
    update();
}

void LineChartWidget::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.fillRect(rect(), QColor("#ffffff"));

    if (points_.isEmpty()) {
        painter.setPen(QColor("#6e6e76"));
        painter.drawText(rect(), Qt::AlignCenter, QStringLiteral("暂无数据"));
        return;
    }

    const int marginLeft = 56;
    const int marginRight = 20;
    const int marginTop = 26;
    const int marginBottom = 34;
    const QRectF chartArea(marginLeft, marginTop,
                           width() - marginLeft - marginRight,
                           height() - marginTop - marginBottom);
    if (chartArea.width() <= 10 || chartArea.height() <= 10) {
        return;
    }

    double maxValue = 0.0;
    for (const auto &point : points_) {
        maxValue = qMax(maxValue, point.second);
    }
    maxValue = niceMax(maxValue);

    QFont smallFont = painter.font();
    smallFont.setPointSizeF(qMax(8.0, smallFont.pointSizeF() - 1.0));
    painter.setFont(smallFont);

    // Horizontal grid + y-axis labels.
    const int divisions = 4;
    for (int i = 0; i <= divisions; ++i) {
        const double ratio = double(i) / divisions;
        const double y = chartArea.bottom() - chartArea.height() * ratio;
        painter.setPen(QPen(QColor("#ebebf0"), 1));
        painter.drawLine(QPointF(chartArea.left(), y), QPointF(chartArea.right(), y));
        painter.setPen(QColor("#6e6e76"));
        const QString label = QString::number(maxValue * ratio, 'f', maxValue < 10 ? 1 : 0);
        painter.drawText(QRectF(0, y - 8, marginLeft - 8, 16),
                         Qt::AlignRight | Qt::AlignVCenter, label);
    }

    // Points.
    const int count = points_.size();
    const int labelEvery = count > 1 && chartArea.width() / (count - 1) < 48 ? 2 : 1;

    QVector<QPointF> linePoints;
    for (int i = 0; i < count; ++i) {
        const double x = count > 1
            ? chartArea.left() + chartArea.width() * i / (count - 1)
            : chartArea.left() + chartArea.width() / 2;
        const double y = chartArea.bottom()
            - chartArea.height() * (points_[i].second / maxValue);
        linePoints.append(QPointF(x, y));

        if (i % labelEvery == 0 || i == count - 1) {
            painter.setPen(QColor("#6e6e76"));
            painter.drawText(QRectF(x - 40, chartArea.bottom() + 4, 80, 16),
                             Qt::AlignHCenter | Qt::AlignVCenter, points_[i].first);
        }
    }

    // A subtle area fill supports the trend without changing the plotted values.
    if (linePoints.size() > 1) {
        QPainterPath area;
        area.moveTo(linePoints.first().x(), chartArea.bottom());
        for (const auto &point : linePoints) area.lineTo(point);
        area.lineTo(linePoints.last().x(), chartArea.bottom());
        area.closeSubpath();
        QLinearGradient fill(chartArea.topLeft(), chartArea.bottomLeft());
        fill.setColorAt(0, QColor(0, 122, 255, 48));
        fill.setColorAt(1, QColor(0, 122, 255, 4));
        painter.fillPath(area, fill);
    }
    // Polyline + dots.
    painter.setPen(QPen(QColor("#007aff"), 2));
    painter.setBrush(Qt::NoBrush);
    painter.drawPolyline(linePoints.constData(), linePoints.size());

    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor("#007aff"));
    for (const QPointF &point : linePoints) {
        painter.drawEllipse(point, 3.0, 3.0);
    }

    // Value label above the last point.
    const QPointF lastPoint = linePoints.last();
    painter.setPen(QColor("#0061c6"));
    QFont boldFont = painter.font();
    boldFont.setBold(true);
    painter.setFont(boldFont);
    const QString lastLabel = QString::number(points_.last().second, 'f', 2) + unit_;
    const qreal labelWidth = painter.fontMetrics().horizontalAdvance(lastLabel) + 8;
    const qreal labelLeft = qBound(0.0, lastPoint.x() - labelWidth / 2,
                                  qMax(0.0, width() - labelWidth));
    painter.drawText(QRectF(labelLeft, lastPoint.y() - 24, labelWidth, 20),
                     Qt::AlignHCenter | Qt::AlignVCenter,
                     lastLabel);
}

PieChartWidget::PieChartWidget(QWidget *parent)
    : QWidget(parent)
{
}

void PieChartWidget::setSlices(const QVector<Slice> &slices, const QString &unit)
{
    slices_ = slices;
    unit_ = unit;
    update();
}

void PieChartWidget::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.fillRect(rect(), QColor("#ffffff"));

    double total = 0.0;
    for (const Slice &slice : slices_) {
        total += slice.value;
    }
    if (slices_.isEmpty() || total <= 0.0) {
        painter.setPen(QColor("#6e6e76"));
        painter.drawText(rect(), Qt::AlignCenter, QStringLiteral("暂无数据"));
        return;
    }

    const int legendWidth = 110;
    const int padding = 16;
    const double diameter = qMin(double(width()) - legendWidth - padding * 2,
                                 double(height()) - padding * 2);
    if (diameter < 40.0) {
        return;
    }
    const QPointF center(padding + diameter / 2, height() / 2.0);
    const QRectF pieRect(center.x() - diameter / 2, center.y() - diameter / 2,
                         diameter, diameter);

    int startAngle = 90 * 16;
    for (const Slice &slice : slices_) {
        const int span = int(slice.value / total * 360.0 * 16.0);
        painter.setPen(QPen(Qt::white, 2));
        painter.setBrush(slice.color);
        painter.drawPie(pieRect, startAngle, -span);
        startAngle -= span;
    }

    // Donut hole + total in the middle.
    const double hole = diameter * 0.55;
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor("#ffffff"));
    painter.drawEllipse(center, hole / 2, hole / 2);

    QFont boldFont = painter.font();
    boldFont.setBold(true);
    painter.setFont(boldFont);
    painter.setPen(QColor(0x42, 0x42, 0x42));
    painter.drawText(QRectF(center.x() - hole / 2, center.y() - hole / 2, hole, hole),
                     Qt::AlignCenter,
                     QStringLiteral("共 %1 %2").arg(int(total)).arg(unit_));

    // Legend on the right.
    QFont legendFont = painter.font();
    legendFont.setBold(false);
    painter.setFont(legendFont);
    const double legendX = padding + diameter + 12;
    double legendY = center.y() - slices_.size() * 22.0 / 2;
    for (const Slice &slice : slices_) {
        painter.setPen(Qt::NoPen);
        painter.setBrush(slice.color);
        painter.drawRect(QRectF(legendX, legendY + 4, 12, 12));
        painter.setPen(QColor(0x42, 0x42, 0x42));
        painter.drawText(QRectF(legendX + 18, legendY, 90, 20), Qt::AlignVCenter,
                         QStringLiteral("%1  %2").arg(slice.label).arg(int(slice.value)));
        legendY += 22;
    }
}

} // namespace ev
