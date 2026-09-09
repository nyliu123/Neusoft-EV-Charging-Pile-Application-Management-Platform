#pragma once

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPixmap>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>

namespace ev {

// Original, resolution-independent energy emblem. No external image/plugin dependency.
inline void drawEnergyMark(QPainter &p, const QRectF &bounds, const QColor &color)
{
    p.save();
    p.translate(bounds.topLeft());
    p.scale(bounds.width() / 100.0, bounds.height() / 100.0);
    p.setPen(QPen(color, 5, Qt::SolidLine, Qt::SquareCap, Qt::MiterJoin));
    p.setBrush(Qt::NoBrush);
    p.drawPolygon(QPolygonF{{8, 28}, {30, 16}, {50, 28}, {50, 68}, {30, 80}, {8, 68}});
    p.drawPolygon(QPolygonF{{50, 28}, {72, 16}, {94, 28}, {94, 68}, {72, 80}, {50, 68}});
    p.drawLine(QPointF(22, 48), QPointF(38, 48));
    p.drawLine(QPointF(64, 48), QPointF(80, 48));
    p.drawLine(QPointF(72, 40), QPointF(72, 56));
    p.restore();
}

inline QWidget *makeRhineBrand(QWidget *parent, bool dark = false)
{
    auto *brand = new QWidget(parent);
    brand->setAttribute(Qt::WA_TranslucentBackground);
    auto *layout = new QHBoxLayout(brand);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(10);
    auto *mark = new QLabel(brand);
    QPixmap pixmap(104, 104);
    pixmap.setDevicePixelRatio(2);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    drawEnergyMark(painter, QRectF(0, 0, 52, 52), QColor(dark ? "#c2dd87" : "#496239"));
    painter.end();
    mark->setPixmap(pixmap);
    mark->setFixedSize(52, 52);
    layout->addWidget(mark);
    auto *textLayout = new QVBoxLayout;
    textLayout->setSpacing(0);
    auto *name = new QLabel(QStringLiteral("RHINE / EV"), brand);
    name->setProperty("uiClass", "brandName");
    auto *caption = new QLabel(QStringLiteral("ENERGY RESEARCH SYSTEM"), brand);
    caption->setProperty("uiClass", "eyebrow");
    textLayout->addWidget(name);
    textLayout->addWidget(caption);
    layout->addLayout(textLayout);
    return brand;
}

// Decorative schematic, never used to represent live station/charging data.
class RhineIdentityPanel final : public QWidget {
public:
    explicit RhineIdentityPanel(bool compact = false, QWidget *parent = nullptr)
        : QWidget(parent), compact_(compact)
    {
        setSizePolicy(QSizePolicy::Expanding, compact ? QSizePolicy::Fixed : QSizePolicy::Expanding);
        if (compact) setFixedHeight(120);
        else setMinimumSize(330, 500);
        setAccessibleName(QStringLiteral("能源研究系统主题装饰"));
    }
protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        p.fillRect(rect(), QColor("#202e29"));
        p.setPen(QPen(QColor("#34443a"), 1));
        for (int x = 0; x < width(); x += 28) p.drawLine(x, 0, x, height());
        for (int y = 0; y < height(); y += 28) p.drawLine(0, y, width(), y);
        const auto label = [&](QRectF box, const QString &text, int size, QColor color, bool bold = false) {
            QFont font = this->font();
            font.setPixelSize(size);
            font.setBold(bold);
            p.setFont(font);
            p.setPen(color);
            p.drawText(box, Qt::AlignLeft | Qt::AlignVCenter, text);
        };
        const QColor lime("#c5df8e"), white("#eff3e7");
        if (compact_) {
            label({26, 15, 400, 20}, QStringLiteral("FIELD OPERATIONS  /  01"), 11, lime, true);
            label({26, 39, 410, 43}, QStringLiteral("为下一段旅程，补充能量。"), 24, white, true);
            label({26, 93, 410, 22}, QStringLiteral("定位站点  /  选择充电桩  /  开始充电"), 12, QColor("#b3c2a6"));
            if (width() > 640) drawInstrument(p, QPointF(width() - 115, 60), 50);
            p.fillRect(0, 0, 4, height(), lime);
            return;
        }
        drawEnergyMark(p, {32, 30, 68, 68}, lime);
        label({116, 38, 230, 26}, QStringLiteral("RHINE / EV"), 21, white, true);
        label({116, 66, 230, 18}, QStringLiteral("ENERGY RESEARCH SYSTEM"), 10, lime);
        label({34, 128, width() - 68.0, 65}, QStringLiteral("ENERGY"), qMin(54, int(width() / 7.5)), white, true);
        label({34, 189, width() - 68.0, 65}, QStringLiteral("FOR TOMORROW."), qMin(25, int(width() / 16)), lime, true);
        label({36, 250, width() - 72.0, 28}, QStringLiteral("让每一份能量，抵达更远的地方。"), 13, QColor("#bac8af"));
        const qreal radius = qMin(width() * 0.29, (height() - 375.0) / 2);
        if (radius > 45) drawInstrument(p, QPointF(width() / 2.0, 303 + radius), radius);
        p.setPen(QPen(QColor("#657853"), 1));
        p.drawLine(34, height() - 52, width() - 34, height() - 52);
        label({34, height() - 44.0, width() - 68.0, 25}, QStringLiteral("EV CHARGING  /  充电服务平台"), 11, lime);
        for (int i = 0; i < 23; ++i) p.fillRect(width() - 44 - i * 3, 105, i % 3 == 0 ? 2 : 1, 11, lime);
    }
private:
    static void drawInstrument(QPainter &p, QPointF center, qreal radius)
    {
        p.save();
        p.translate(center);
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(QColor("#667c51"), 1));
        p.drawEllipse(QPointF(), radius, radius);
        p.drawEllipse(QPointF(), radius * .76, radius * .76);
        p.drawLine(QPointF(-radius - 16, 0), QPointF(radius + 16, 0));
        p.drawLine(QPointF(0, -radius - 16), QPointF(0, radius + 16));
        for (int i = 0; i < 60; ++i) {
            p.save(); p.rotate(i * 6);
            p.drawLine(QPointF(0, -radius), QPointF(0, -radius + (i % 5 == 0 ? 8 : 3)));
            p.restore();
        }
        p.setPen(QPen(QColor("#c5df8e"), 5));
        const QRectF arc(-radius * .87, -radius * .87, radius * 1.74, radius * 1.74);
        p.drawArc(arc, 18 * 16, 105 * 16); p.drawArc(arc, 198 * 16, 105 * 16);
        p.setPen(QPen(QColor("#c5df8e"), 2));
        p.setBrush(QColor("#2c3e30"));
        p.drawPolygon(QPolygonF{{0, -radius * .57}, {radius * .49, 0}, {0, radius * .57}, {-radius * .49, 0}});
        p.setPen(Qt::NoPen); p.setBrush(QColor("#c5df8e"));
        const qreal s = radius * .31;
        p.drawPolygon(QPolygonF{{s * .24, -s}, {-s * .62, s * .15}, {-s * .08, s * .15},
                                {-s * .24, s}, {s * .62, -s * .15}, {s * .08, -s * .15}});
        p.restore();
    }
    bool compact_;
};

class RhineStationCard final : public QPushButton {
public:
    RhineStationCard(int index, const QString &name, const QString &address,
                     const QString &distance, const QString &price,
                     const QString &availability, bool available, QWidget *parent)
        : QPushButton(parent)
    {
        setProperty("uiClass", "stationCard");
        setProperty("available", available);
        setCursor(Qt::PointingHandCursor);
        setAccessibleName(name + QStringLiteral("，") + address + QStringLiteral("，")
                          + distance + QStringLiteral("，") + price + QStringLiteral("，") + availability);
        auto *row = new QHBoxLayout(this);
        row->setContentsMargins(22, 17, 22, 17);
        row->setSpacing(20);
        const auto label = [this](const QString &text, const char *role) {
            auto *item = new QLabel(text, this);
            item->setTextFormat(Qt::PlainText);
            item->setProperty("uiClass", role);
            item->setAttribute(Qt::WA_TransparentForMouseEvents);
            return item;
        };
        auto *number = label(QStringLiteral("%1").arg(index, 2, 10, QLatin1Char('0')), "stationIndex");
        number->setAlignment(Qt::AlignCenter);
        number->setFixedSize(48, 56);
        row->addWidget(number);
        auto *body = new QVBoxLayout;
        body->setSpacing(5);
        auto *title = label(name, "cardTitle");
        title->setWordWrap(true);
        body->addWidget(title);
        auto *location = label(address, "muted");
        location->setWordWrap(true);
        body->addWidget(location);
        body->addWidget(label(price + QStringLiteral("    /    ") + availability, "stationPrice"));
        row->addLayout(body, 1);
        auto *distanceLabel = label(distance, "stationDistance");
        row->addWidget(distanceLabel);
        row->addWidget(label(QStringLiteral("↗"), "stationArrow"));
    }
};

} // namespace ev
