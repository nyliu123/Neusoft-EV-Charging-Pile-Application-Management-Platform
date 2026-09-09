#pragma once

#include <QFrame>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QIcon>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QPixmap>
#include <QResizeEvent>
#include <QStyleOptionButton>
#include <QVBoxLayout>
#include <QVector>
#include <QtMath>

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
                QColor(mode == QIcon::Selected ? "#4659c9" : mode == QIcon::Disabled ? "#9b9ba2" : "#515159"));
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
    gradient.setColorAt(0, QColor("#5a72ed"));
    gradient.setColorAt(1, QColor("#8998ff"));
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

// An original, scalable charging-device illustration, never live equipment data.
class AppleEnergyIllustration final : public QWidget {
public:
    explicit AppleEnergyIllustration(QWidget *parent) : QWidget(parent)
    {
        setAttribute(Qt::WA_TranslucentBackground);
        setMinimumSize(200, 64);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        setAccessibleName(QStringLiteral("充电设备主题插画，非实时设备数据"));
    }
protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        p.translate(width() / 2.0, height() / 2.0);
        const qreal scale = qMin(width() / 320.0, height() / 216.0);
        p.scale(scale, scale);
        p.setPen(Qt::NoPen);
        QRadialGradient glow(QPointF(5, 0), 165);
        glow.setColorAt(0, QColor(123, 143, 255, 80));
        glow.setColorAt(1, QColor(123, 143, 255, 0));
        p.setBrush(glow);
        p.drawEllipse(QRectF(-165, -105, 330, 210));
        p.setBrush(QColor(12, 19, 53, 95));
        p.drawEllipse(QRectF(-129, 54, 267, 45));
        const auto charger = [&p](qreal x, qreal y, qreal size, qreal angle) {
            p.save();
            p.translate(x, y); p.scale(size, size); p.rotate(angle);
            p.setPen(Qt::NoPen);
            p.setBrush(QColor("#7488ed"));
            p.drawPolygon(QPolygonF{{38, -74}, {58, -62}, {58, 75}, {38, 87}});
            QLinearGradient body(-42, -74, 38, 87);
            body.setColorAt(0, QColor("#ffffff")); body.setColorAt(1, QColor("#c7d1ff"));
            p.setBrush(body);
            p.drawRoundedRect(QRectF(-42, -74, 82, 161), 18, 18);
            p.setBrush(QColor("#202c56"));
            p.drawRoundedRect(QRectF(-25, -53, 47, 43), 8, 8);
            drawAppSymbol(p, {-11, -45, 20, 27}, AppSymbol::Bolt, QColor("#c8f5df"));
            p.setBrush(QColor("#a5b6ed"));
            p.drawRoundedRect(QRectF(-22, 10, 41, 4), 2, 2);
            p.drawRoundedRect(QRectF(-22, 21, 30, 4), 2, 2);
            p.setBrush(QColor("#516ce0"));
            p.drawRoundedRect(QRectF(-18, 54, 33, 9), 4, 4);
            QPainterPath cable;
            cable.moveTo(57, -24);
            cable.cubicTo(100, -24, 101, 57, 78, 57);
            cable.cubicTo(59, 57, 74, 22, 67, 16);
            p.setBrush(Qt::NoBrush);
            p.setPen(QPen(QColor("#9eaffc"), 6, Qt::SolidLine, Qt::RoundCap));
            p.drawPath(cable);
            p.setPen(QPen(QColor("#d7defe"), 9, Qt::SolidLine, Qt::RoundCap));
            p.drawLine(QPointF(67, 16), QPointF(64, 3));
            p.restore();
        };
        charger(-85, 17, .62, -12);
        charger(24, -4, .94, 8);
    }
};

class ModernHeroBanner final : public QWidget {
public:
    explicit ModernHeroBanner(QWidget *parent = nullptr) : QWidget(parent)
    {
        setObjectName(QStringLiteral("discoveryHero"));
        setFixedHeight(172);
        auto *row = new QHBoxLayout(this);
        row_ = row;
        row->setContentsMargins(30, 20, 24, 20);
        auto *text = new QVBoxLayout;
        text_ = text;
        text->setSpacing(8);
        auto *overline = new QLabel(QStringLiteral("轻充  /  为每一段旅程蓄能"), this);
        overline_ = overline;
        overline->setProperty("uiClass", "heroOverline");
        text->addWidget(overline);
        auto *title = new QLabel(QStringLiteral("好旅程，从满电开始。"), this);
        title_ = title;
        title->setProperty("uiClass", "discoveryTitle");
        text->addWidget(title);
        auto *caption = new QLabel(QStringLiteral("找到身边的充电站，轻松驶向下一站。"), this);
        caption->setProperty("uiClass", "heroCaption");
        text->addWidget(caption);
        row->addLayout(text, 3);
        auto *illustration = new AppleEnergyIllustration(this);
        row->addWidget(illustration, 2);
    }
    void setCompact(bool compact)
    {
        if (compact_ == compact) return;
        compact_ = compact;
        setFixedHeight(compact ? 88 : 172);
        row_->setContentsMargins(30, compact ? 10 : 20, 24, compact ? 10 : 20);
        text_->setSpacing(compact ? 4 : 8);
        overline_->setVisible(!compact);
        title_->setProperty("uiClass", compact ? "discoveryCompactTitle" : "discoveryTitle");
        title_->style()->unpolish(title_);
        title_->style()->polish(title_);
        title_->updateGeometry();
    }
protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        QLinearGradient background(0, 0, width(), height());
        background.setColorAt(0, QColor("#19233e"));
        background.setColorAt(.55, QColor("#293565"));
        background.setColorAt(1, QColor("#5268b4"));
        p.setPen(Qt::NoPen); p.setBrush(background);
        p.drawRoundedRect(rect(), 24, 24);
    }
private:
    QHBoxLayout *row_;
    QVBoxLayout *text_;
    QLabel *overline_;
    QLabel *title_;
    bool compact_ = false;
};

// Real labels/layouts preserve accessibility and the system's DPI scaling.
class AppleIdentityPanel final : public QWidget {
public:
    explicit AppleIdentityPanel(bool compact = false, QWidget *parent = nullptr)
        : QWidget(parent)
    {
        setObjectName(QStringLiteral("loginVisual"));
        auto *layout = new QVBoxLayout(this);
        layout->setContentsMargins(34, 30, 34, 28);
        layout->setSpacing(14);
        if (!compact) {
            setMinimumSize(340, 500);
            layout->addWidget(makeAppleBrand(this));
            layout->addSpacing(24);
        } else {
            setFixedHeight(156);
        }
        auto *title = new QLabel(compact ? QStringLiteral("好旅程，从满电开始。")
                                         : QStringLiteral("充满能量，\n自在出发。"), this);
        title->setProperty("uiClass", compact ? "welcomeTitle" : "heroTitle");
        title->setWordWrap(true);
        layout->addWidget(title);
        auto *caption = new QLabel(QStringLiteral("连接你与下一段旅程。\n让日常充电，成为一件轻松的小事。"), this);
        caption->setProperty("uiClass", "heroCaption");
        caption->setWordWrap(true);
        layout->addWidget(caption);
        if (!compact) {
            layout->addWidget(new AppleEnergyIllustration(this), 1);
            auto *footer = new QLabel(QStringLiteral("查找站点   /   预约充电   /   轻松结算"), this);
            footer->setProperty("uiClass", "heroOverline");
            footer->setWordWrap(true);
            layout->addWidget(footer);
        }
    }
protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        QLinearGradient background(0, 0, width(), height());
        background.setColorAt(0, QColor("#19233e"));
        background.setColorAt(1, QColor("#405393"));
        p.setBrush(background); p.setPen(Qt::NoPen);
        p.drawRoundedRect(rect(), 26, 26);
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

class StarRatingBar final : public QWidget {
public:
    explicit StarRatingBar(double rating, QWidget *parent = nullptr)
        : QWidget(parent), rating_(qBound(0.0, rating, 5.0))
    {
        setFixedSize(96, 18);
        setAccessibleName(QStringLiteral("评分 %1 / 5").arg(rating_, 0, 'f', 1));
        setAttribute(Qt::WA_TransparentForMouseEvents);
    }
protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        QPainterPath stars;
        constexpr qreal outerRadius = 7.5;
        constexpr qreal innerRadius = 3.2;
        constexpr qreal spacing = 19.0;
        for (int star = 0; star < 5; ++star) {
            QPainterPath path;
            const QPointF center(star * spacing + outerRadius + 1.0, height() / 2.0);
            for (int point = 0; point < 10; ++point) {
                const qreal angle = -M_PI_2 + point * M_PI / 5.0;
                const qreal radius = point % 2 == 0 ? outerRadius : innerRadius;
                const QPointF vertex(center.x() + qCos(angle) * radius,
                                     center.y() + qSin(angle) * radius);
                if (point == 0) {
                    path.moveTo(vertex);
                } else {
                    path.lineTo(vertex);
                }
            }
            path.closeSubpath();
            stars.addPath(path);
        }
        painter.setPen(QPen(QColor("#d7a43c"), 1.0));
        painter.setBrush(QColor("#fff4d8"));
        painter.drawPath(stars);
        painter.save();
        painter.setClipRect(QRectF(0, 0, width() * rating_ / 5.0, height()));
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor("#f2a900"));
        painter.drawPath(stars);
        painter.restore();
    }
private:
    double rating_;
};

class AppleStationCard final : public QPushButton {
public:
    AppleStationCard(const QString &name, const QString &address, const QString &distance,
                     const QString &price, const QString &availability, const QString &rating,
                     bool available, QWidget *parent)
        : AppleStationCard(name, address, distance, price, availability,
                           -1.0, QString(), available, parent)
    {
        setAccessibleDescription(rating);
    }
    AppleStationCard(const QString &name, const QString &address, const QString &distance,
                     const QString &price, const QString &availability, double rating,
                     const QString &hotReview, bool available, QWidget *parent) : QPushButton(parent)
    {
        setProperty("uiClass", "stationCard");
        setProperty("available", available);
        setCursor(Qt::PointingHandCursor);
        const QString description = name + QStringLiteral("，") + address + QStringLiteral("，")
            + distance + QStringLiteral("，") + price + QStringLiteral("，") + availability
            + (rating >= 0.0 ? QStringLiteral("，评分 %1").arg(rating, 0, 'f', 1) : QString())
            + (hotReview.isEmpty() ? QString() : QStringLiteral("，") + hotReview);
        setText(description);
        setAccessibleName(description);
        auto *column = new QVBoxLayout(this);
        column->setContentsMargins(22, 20, 22, 18);
        column->setSpacing(8);
        auto *top = new QHBoxLayout;
        auto *symbol = new QLabel(this);
        symbol->setProperty("uiClass", "stationSymbol");
        symbol->setPixmap(appSymbolIcon(AppSymbol::Station).pixmap(25, 25));
        symbol->setAlignment(Qt::AlignCenter);
        symbol->setFixedSize(44, 44);
        symbol->setAttribute(Qt::WA_TransparentForMouseEvents);
        top->addWidget(symbol);
        auto *addressLabel = new AppleElidedLabel(address, this);
        addressLabel->setProperty("uiClass", "muted");
        addressLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
        top->addWidget(addressLabel, 1);
        auto *distanceLabel = new QLabel(distance, this);
        distanceLabel->setProperty("uiClass", "distancePill");
        distanceLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
        top->addWidget(distanceLabel);
        column->addLayout(top);
        const auto elided = [this](const QString &text, const char *role) {
            auto *label = new AppleElidedLabel(text, this);
            label->setProperty("uiClass", role);
            label->setAttribute(Qt::WA_TransparentForMouseEvents);
            return label;
        };
        column->addWidget(elided(name, "stationTitle"));
        if (rating >= 0.0) {
            auto *ratingRow = new QHBoxLayout;
            ratingRow->setSpacing(6);
            ratingRow->addWidget(new StarRatingBar(rating, this));
            auto *scoreLabel = new QLabel(QString::number(rating, 'f', 1), this);
            scoreLabel->setProperty("uiClass", "stationRating");
            scoreLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
            ratingRow->addWidget(scoreLabel);
            ratingRow->addStretch();
            column->addLayout(ratingRow);
        } else {
            column->addWidget(elided(QStringLiteral("暂无评分"), "stationRating"));
        }
        if (!hotReview.isEmpty()) {
            auto *hotReviewLabel = elided(hotReview, "stationHotReview");
            hotReviewLabel->setStyleSheet(QStringLiteral(
                "background-color:#fff0df;color:#c43f2c;border-radius:8px;"
                "padding:4px 7px;font-size:11px;"));
            column->addWidget(hotReviewLabel);
        }
        column->addStretch();
        auto *bottom = new QHBoxLayout;
        bottom->setSpacing(8);
        auto *priceLabel = new QLabel(price, this);
        priceLabel->setProperty("uiClass", "stationPrice");
        priceLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
        bottom->addWidget(priceLabel);
        bottom->addStretch();
        auto *availabilityLabel = new QLabel(availability, this);
        availabilityLabel->setProperty("uiClass", available ? "availabilityPill" : "unavailablePill");
        availabilityLabel->setWordWrap(true);
        availabilityLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
        bottom->addWidget(availabilityLabel);
        column->addLayout(bottom);
        setFixedHeight(238);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    }
    QSize sizeHint() const override { return {350, 238}; }
    QSize minimumSizeHint() const override { return {260, 238}; }
protected:
    void paintEvent(QPaintEvent *) override
    {
        QStyleOptionButton option;
        initStyleOption(&option);
        option.text.clear();
        QPainter painter(this);
        style()->drawControl(QStyle::CE_PushButton, &option, &painter, this);
    }
};

// Reflow the same buttons instead of recreating them, preserving focus and signals.
class AdaptiveStationGrid final : public QWidget {
public:
    explicit AdaptiveStationGrid(QWidget *parent = nullptr) : QWidget(parent)
    {
        setObjectName(QStringLiteral("stationList"));
        grid_ = new QGridLayout(this);
        grid_->setContentsMargins(1, 4, 8, 4);
        grid_->setSpacing(16);
        grid_->setAlignment(Qt::AlignTop);
        grid_->setSizeConstraint(QLayout::SetNoConstraint);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
        emptyHint_ = new QLabel(QStringLiteral("从你所在的位置开始\n选择区域或输入地址，发现附近充电站。"), this);
        emptyHint_->setProperty("uiClass", "muted");
        emptyHint_->setAlignment(Qt::AlignCenter);
        emptyHint_->setWordWrap(true);
    }
    void clear()
    {
        while (auto *item = grid_->takeAt(0)) {
            delete item->widget();
            delete item;
        }
        cards_.clear();
        reflow();
    }
    void addCard(QWidget *card)
    {
        reflow();
        const int index = cards_.size();
        cards_.append(card);
        grid_->addWidget(card, index / columns_, index % columns_);
        updateContentHeight();
    }
    QSize minimumSizeHint() const override { return {280, minimumHeight()}; }
    QSize sizeHint() const override { return {350, minimumHeight()}; }
protected:
    void resizeEvent(QResizeEvent *event) override
    {
        QWidget::resizeEvent(event);
        reflow();
    }
private:
    void reflow()
    {
        const int columns = width() >= 1050 ? 3 : width() >= 680 ? 2 : 1;
        if (columns != columns_) {
            columns_ = columns;
            while (auto *item = grid_->takeAt(0)) delete item;
            for (int column = 0; column < 3; ++column)
                grid_->setColumnStretch(column, column < columns ? 1 : 0);
            for (int i = 0; i < cards_.size(); ++i)
                grid_->addWidget(cards_[i], i / columns, i % columns);
        }
        setProperty("columns", columns);
        updateContentHeight();
    }
    void updateContentHeight()
    {
        const int rows = (cards_.size() + columns_ - 1) / columns_;
        setMinimumHeight(rows ? rows * 238 + (rows - 1) * 16 + 8 : 0);
        emptyHint_->setGeometry(rect());
        emptyHint_->setVisible(cards_.isEmpty());
    }
    QGridLayout *grid_;
    QLabel *emptyHint_;
    QVector<QWidget *> cards_;
    int columns_ = 0;
};

} // namespace ev
