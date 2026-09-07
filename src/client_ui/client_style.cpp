#include "client_ui/client_style.h"

#include <QAbstractItemView>
#include <QApplication>
#include <QComboBox>
#include <QDir>
#include <QEvent>
#include <QLibraryInfo>
#include <QPainter>
#include <QProxyStyle>

namespace {

class ClientProxyStyle final : public QProxyStyle {
public:
    int styleHint(StyleHint hint, const QStyleOption *option = nullptr,
                  const QWidget *widget = nullptr,
                  QStyleHintReturn *returnData = nullptr) const override
    {
        if (hint == QStyle::SH_ComboBox_Popup) {
            return 0;
        }
        return QProxyStyle::styleHint(hint, option, widget, returnData);
    }

    void drawPrimitive(PrimitiveElement element, const QStyleOption *option,
                       QPainter *painter, const QWidget *widget = nullptr) const override
    {
        if (element == QStyle::PE_IndicatorArrowDown) {
            // AnimatedComboBox paints its own arrow after the normal control.
            return;
        }
        QProxyStyle::drawPrimitive(element, option, painter, widget);
    }
};

class ComboBoxPopupFilter final : public QObject {
public:
    using QObject::QObject;

protected:
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        if (event->type() == QEvent::Polish) {
            if (auto *comboBox = qobject_cast<QComboBox *>(watched)) {
                comboBox->setMaxVisibleItems(5);
                comboBox->view()->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
                comboBox->view()->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
            }
        }
        return QObject::eventFilter(watched, event);
    }
};

} // namespace

namespace ev {

void configureClientInputMethod()
{
    const QByteArray requested = qgetenv("QT_IM_MODULE");
    const QByteArray xModifiers = qgetenv("XMODIFIERS");
    if (!xModifiers.contains("fcitx")
        || (!requested.isEmpty() && !requested.startsWith("fcitx"))) {
        return;
    }

    const QDir inputPluginDirectory(
        QLibraryInfo::path(QLibraryInfo::PluginsPath)
            + QStringLiteral("/platforminputcontexts"));
    const bool hasFcitxQt6Plugin = !inputPluginDirectory.entryList(
        {QStringLiteral("*fcitx*")}, QDir::Files).isEmpty();
    qputenv("QT_IM_MODULE", hasFcitxQt6Plugin ? QByteArray("fcitx")
                                               : QByteArray("ibus"));
}

void installClientStyle(QApplication &application)
{
    application.setStyle(new ClientProxyStyle);
    application.installEventFilter(new ComboBoxPopupFilter(&application));
}

} // namespace ev
