#pragma once

#include <QFrame>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QPixmap>
#include <QResizeEvent>
#include <QStyleOptionButton>
#include <QVBoxLayout>

namespace ev {

// Original vector symbols; no Apple artwork or proprietary font files are bundled.
enum class AppSymbol { Bolt, Compass, Person, Receipt, Overview, Station };

inline void drawAppSymbol(QPainter &p, const QRectF &bounds, AppSymbol symbol,
                          const QColor &color)
{
    p.save();
    p.translate(bounds.topLeft());
    p.scale(bounds.width() / 24.0, bounds.height() / 24.0);
    p.setPen(QPen(color, 1.7, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.setBrush(Qt::NoBrush);
    switch (symbol) {
    case AppSymbol::Bolt:
        p.drawPolygon(QPolygonF{{13.5, 2}, {5, 13}, {11, 13}, {10.5, 22}, {19, 10}, {13, 10}});
        break;
    case AppSymbol::Compass:
        p.drawEllipse(QRectF(2, 2, 20, 20));
        p.drawPolygon(QPolygonF{{16, 8}, {14, 14}, {8, 16}, {10, 10}});
        break;
    case AppSymbol::Person:
        p.drawEllipse(QRectF(8, 3, 8, 8));
        p.drawArc(QRectF(4, 13, 16, 16), 0, 180 * 16);
        p.drawLine(4, 21, 20, 21);
        break;
    case AppSymbol::Receipt:
        p.drawRoundedRect(QRectF(5, 2, 14, 20), 2, 2);
        p.drawLine(9, 7, 15, 7); p.drawLine(9, 12, 15, 12); p.drawLine(9, 17, 13, 17);
        break;
    case AppSymbol::Overview:
        p.drawRoundedRect(QRectF(2, 2, 8, 8), 2, 2);
        p.drawRoundedRect(QRectF(14, 2, 8, 8), 2, 2);
        p.drawRoundedRect(QRectF(2, 14, 8, 8), 2, 2);
        p.drawRoundedRect(QRectF(14, 14, 8, 8), 2, 2);
        break;
    case AppSymbol::Station:
        p.drawRoundedRect(QRectF(3, 3, 12, 18), 2, 2);
        p.drawRoundedRect(QRectF(6, 6, 6, 5), 1, 1);
        p.drawLine(7, 17, 11, 17);
        p.drawPolyline(QPolygonF{{15, 12}, {19, 12}, {19, 18}, {22, 18}, {22, 7}, {19, 4}});
        break;
    }
    p.restore();
}

inline QIcon appSymbolIcon(AppSymbol symbol)
{
    QIcon icon;
    for (int logicalSize : {24, 64}) {
        for (const auto mode : {QIcon::Normal, QIcon::Selected, QIcon::Disabled}) {
            QPixmap pixmap(logicalSize * 2, logicalSize * 2);
            pixmap.setDevicePixelRatio(2);
            pixmap.fill(Qt::transparent);
            QPainter p(&pixmap);
            p.setRenderHint(QPainter::Antialiasing);
            drawAppSymbol(p, QRectF(2, 2, logicalSize - 4, logicalSize - 4), symbol,
                QColor(mode == QIcon::Selected ? "#ffffff" : mode == QIcon::Disabled ? "#9b9ba2" : "#515159"));
            p.end();
            icon.addPixmap(pixmap, mode);
        }
    }
    return icon;
}

inline QWidget *makeAppleBrand(QWidget *parent)
{
    auto *brand = new QWidget(parent);
    brand->setAttribute(Qt::WA_TranslucentBackground);
    auto *row = new QHBoxLayout(brand);
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(10);
    QPixmap pixmap(84, 84);
    pixmap.setDevicePixelRatio(2);
    pixmap.fill(Qt::transparent);
    QPainter p(&pixmap);
    p.setRenderHint(QPainter::Antialiasing);
    QLinearGradient gradient(0, 0, 42, 42);
    gradient.setColorAt(0, QColor("#3a9dff"));
    gradient.setColorAt(1, QColor("#0061dd"));
    p.setPen(Qt::NoPen); p.setBrush(gradient);
    p.drawRoundedRect(QRectF(0, 0, 42, 42), 12, 12);
    drawAppSymbol(p, {9, 8, 24, 26}, AppSymbol::Bolt, Qt::white);
    p.end();
    auto *mark = new QLabel(brand);
    mark->setPixmap(pixmap);
    mark->setFixedSize(42, 42);
    row->addWidget(mark);
    auto *labels = new QVBoxLayout;
    labels->setSpacing(0);
    auto *name = new QLabel(QStringLiteral("轻充"), brand);
    name->setProperty("uiClass", "brandName");
    auto *caption = new QLabel(QStringLiteral("CHARGE"), brand);
    caption->setProperty("uiClass", "eyebrow");
    labels->addWidget(name); labels->addWidget(caption);
    row->addLayout(labels);
    row->addStretch();
    return brand;
}

class AppleEnergyIllustration final : public QWidget {
public:
    explicit AppleEnergyIllustration(QWidget *parent) : QWidget(parent)
    {
        setMinimumHeight(130);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        setAccessibleName(QStringLiteral("充电服务插画，非实时数据"));
    }
protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const QPointF center(width() / 2.0, height() / 2.0);
        const qreal radius = qMin(width() * .46, height() * .48);
        QRadialGradient glow(center, radius);
        glow.setColorAt(0, QColor("#bbdcff"));
        glow.setColorAt(.65, QColor("#e4efff"));
        glow.setColorAt(1, QColor("#f5f5f7"));
        p.setPen(Qt::NoPen); p.setBrush(glow);
        p.drawEllipse(center, radius, radius);
        const qreal size = qMin(152.0, radius * 1.4);
        const QRectF tile(center.x() - size / 2, center.y() - size / 2, size, size);
        p.setBrush(QColor(40, 95, 150, 12));
        p.drawRoundedRect(tile.translated(0, 8), size * .27, size * .27);
        QLinearGradient material(tile.topLeft(), tile.bottomRight());
        material.setColorAt(0, Qt::white); material.setColorAt(1, QColor("#e5efff"));
        p.setBrush(material); p.setPen(QPen(Qt::white, 2));
        p.drawRoundedRect(tile, size * .27, size * .27);
        drawAppSymbol(p, tile.adjusted(size * .24, size * .18, -size * .24, -size * .18),
                      AppSymbol::Bolt, QColor("#007aff"));
    }
};

// Real labels/layouts keep text accessible and allow the system's DPI scaling.
class AppleIdentityPanel final : public QWidget {
public:
    explicit AppleIdentityPanel(bool compact = false, QWidget *parent = nullptr)
        : QWidget(parent)
    {
        auto *layout = new QVBoxLayout(this);
        layout->setContentsMargins(compact ? 26 : 38, compact ? 18 : 32, compact ? 26 : 38, compact ? 18 : 28);
        layout->setSpacing(12);
        if (!compact) {
            setMinimumSize(320, 500);
            layout->addWidget(makeAppleBrand(this));
            layout->addSpacing(32);
        } else {
            setFixedHeight(126);
        }
        auto *title = new QLabel(compact ? QStringLiteral("下一站，充满能量。")
                                         : QStringLiteral("轻松充电。\n从容出发。"), this);
        title->setProperty("uiClass", compact ? "welcomeTitle" : "heroTitle");
        title->setWordWrap(true);
        layout->addWidget(title);
        auto *caption = new QLabel(compact ? QStringLiteral("找到身边的充电站，让每一段旅程更从容。")
            : QStringLiteral("从附近的充电站，到下一段旅程。\n把充电交给轻充，把时间留给生活。"), this);
        caption->setProperty("uiClass", "heroCaption");
        caption->setWordWrap(true);
        layout->addWidget(caption);
        if (!compact) {
            layout->addWidget(new AppleEnergyIllustration(this), 1);
            auto *footer = new QLabel(QStringLiteral("查找站点   ·   预约充电   ·   查看账单"), this);
            footer->setProperty("uiClass", "muted");
            footer->setWordWrap(true);
            layout->addWidget(footer);
        }
    }
};

class AppleElidedLabel final : public QLabel {
public:
    explicit AppleElidedLabel(const QString &text, QWidget *parent)
        : QLabel(parent), fullText_(text)
    {
        setTextFormat(Qt::PlainText);
        setToolTip(text);
        setAccessibleName(text);
        setMinimumWidth(0);
        setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    }
protected:
    void resizeEvent(QResizeEvent *event) override
    {
        QLabel::resizeEvent(event);
        setText(fontMetrics().elidedText(fullText_, Qt::ElideRight, contentsRect().width()));
    }
private:
    QString fullText_;
};

class AppleStationCard final : public QPushButton {
public:
    AppleStationCard(const QString &name, const QString &address, const QString &distance,
                     const QString &price, const QString &availability, const QString &rating,
                     bool available, QWidget *parent) : QPushButton(parent)
    {
        setProperty("uiClass", "stationCard");
        setProperty("available", available);
        setCursor(Qt::PointingHandCursor);
        const QString description = name + QStringLiteral("，") + address + QStringLiteral("，")
            + distance + QStringLiteral("，") + price + QStringLiteral("，") + availability + QStringLiteral("，") + rating;
        // Preserve button text for accessibility and existing UI integrations.
        setText(description);
        setAccessibleName(description);
        auto *row = new QHBoxLayout(this);
        row->setContentsMargins(20, 18, 20, 18);
        row->setSpacing(18);
        auto *symbol = new QLabel(this);
        symbol->setProperty("uiClass", "stationSymbol");
        symbol->setPixmap(appSymbolIcon(AppSymbol::Station).pixmap(28, 28));
        symbol->setAlignment(Qt::AlignCenter);
        symbol->setFixedSize(54, 60);
        symbol->setAttribute(Qt::WA_TransparentForMouseEvents);
        row->addWidget(symbol);
        auto *body = new QVBoxLayout;
        body->setSpacing(6);
        const auto label = [this](const QString &text, const char *role) {
            auto *label = new QLabel(text, this);
            label->setTextFormat(Qt::PlainText);
            label->setProperty("uiClass", role);
            label->setWordWrap(true);
            label->setAttribute(Qt::WA_TransparentForMouseEvents);
            return label;
        };
        body->addWidget(label(name, "cardTitle"));
        auto *location = new AppleElidedLabel(address, this);
        location->setProperty("uiClass", "muted");
        location->setAttribute(Qt::WA_TransparentForMouseEvents);
        body->addWidget(location);
        body->addWidget(label(price + QStringLiteral("   ·   ") + availability, "stationPrice"));
        auto *review = new AppleElidedLabel(rating, this);
        review->setProperty("uiClass", "muted");
        review->setAttribute(Qt::WA_TransparentForMouseEvents);
        body->addWidget(review);
        row->addLayout(body, 1);
        auto *trailing = new QVBoxLayout;
        trailing->setSpacing(5);
        auto *distanceLabel = label(distance, "stationDistance");
        distanceLabel->setAlignment(Qt::AlignRight);
        trailing->addWidget(distanceLabel);
        auto *detail = label(QStringLiteral("查看详情  ›"), "linkCaption");
        detail->setAlignment(Qt::AlignRight);
        trailing->addWidget(detail);
        row->addLayout(trailing);
        setMinimumHeight(140);
    }
    QSize sizeHint() const override { return {540, 148}; }
protected:
    void paintEvent(QPaintEvent *) override
    {
        QStyleOptionButton option;
        initStyleOption(&option);
        option.text.clear(); // Child labels provide the visible typography.
        QPainter painter(this);
        style()->drawControl(QStyle::CE_PushButton, &option, &painter, this);
    }
};

} // namespace ev
