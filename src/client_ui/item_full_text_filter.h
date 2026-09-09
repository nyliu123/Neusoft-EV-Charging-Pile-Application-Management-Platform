#pragma once

#include <QAbstractItemView>
#include <QApplication>
#include <QEvent>
#include <QFontMetrics>
#include <QMouseEvent>
#include <QObject>
#include <QPersistentModelIndex>
#include <QPointer>
#include <QToolTip>

namespace ev {

class ItemFullTextFilter final : public QObject {
public:
    explicit ItemFullTextFilter(QObject *parent=nullptr):QObject(parent){}

protected:
    bool eventFilter(QObject *watched,QEvent *event) override
    {
        if(event->type()==QEvent::Show){
            if(auto *view=qobject_cast<QAbstractItemView *>(watched))view->viewport()->setMouseTracking(true);
            return QObject::eventFilter(watched,event);
        }
        auto *viewport=qobject_cast<QWidget *>(watched);
        auto *view=viewport?qobject_cast<QAbstractItemView *>(viewport->parentWidget()):nullptr;
        if(!view || view->viewport()!=viewport)return QObject::eventFilter(watched,event);
        if(event->type()==QEvent::Leave){clear();return QObject::eventFilter(watched,event);}
        if(event->type()!=QEvent::MouseMove)return QObject::eventFilter(watched,event);
        auto *mouse=static_cast<QMouseEvent *>(event);
        const QModelIndex index=view->indexAt(mouse->position().toPoint());
        if(!index.isValid()){clear();return QObject::eventFilter(watched,event);}
        if(currentView_==view && currentIndex_==index)
            return QObject::eventFilter(watched,event);
        QString value=index.data(Qt::ToolTipRole).toString();
        if(value.isEmpty())value=index.data(Qt::DisplayRole).toString();
        const QString displayed=index.data(Qt::DisplayRole).toString();
        const QRect cell=view->visualRect(index);
        const bool explicitlyElided=displayed.contains(QChar(0x2026)) || displayed.contains(QStringLiteral("..."));
        const bool visuallyElided=!displayed.isEmpty() && QFontMetrics(view->font()).elidedText(
            displayed,view->textElideMode(),qMax(0,cell.width()-12))!=displayed;
        if(!value.trimmed().isEmpty() && (explicitlyElided || visuallyElided)){
            QString html=value.toHtmlEscaped();html.replace(QLatin1Char('\n'),QStringLiteral("<br>"));
            currentView_=view;currentIndex_=index;
            QToolTip::showText(mouse->globalPosition().toPoint(),QStringLiteral("<div style='white-space:pre-wrap'>%1</div>").arg(html),view,cell);
        }else clear();
        return QObject::eventFilter(watched,event);
    }
private:
    void clear(){QToolTip::hideText();currentView_.clear();currentIndex_=QPersistentModelIndex();}
    QPointer<QAbstractItemView> currentView_;
    QPersistentModelIndex currentIndex_;
};

inline void installItemFullTextFilter(QApplication &application)
{
    application.installEventFilter(new ItemFullTextFilter(&application));
}

}
