#pragma once

#include <QFrame>
#include <QLabel>
#include <QMainWindow>
#include <QPauseAnimation>
#include <QPointer>
#include <QPropertyAnimation>
#include <QSequentialAnimationGroup>
#include <QTimer>
#include <QVBoxLayout>

namespace ev {

class SlideToast final {
public:
    static void show(QWidget *anchor,const QString &message,bool error=false)
    {
        if(!anchor || message.trimmed().isEmpty())return;

        QWidget *window=anchor->window();
        if(!window->isVisible()){
            const QPointer<QWidget> guard(anchor);
            QTimer::singleShot(0,window,[guard,message,error]{if(guard)show(guard,message,error);});
            return;
        }

        QWidget *host=window;
        if(auto *mainWindow=qobject_cast<QMainWindow *>(host)){
            if(mainWindow->centralWidget())host=mainWindow->centralWidget();
        }
        auto *toast=new QFrame(host);
        toast->setAttribute(Qt::WA_DeleteOnClose);toast->setObjectName(QStringLiteral("slideToast"));
        toast->setStyleSheet(QString("QFrame#slideToast{background:%1;border:1px solid %2;border-radius:14px;} QLabel{color:%3;font-size:14px;font-weight:600;background:transparent;}")
            .arg(error?QStringLiteral("#fff0f0"):QStringLiteral("#eaf8ef"),error?QStringLiteral("#e69a9a"):QStringLiteral("#8fcaa3"),error?QStringLiteral("#b42332"):QStringLiteral("#18733b")));
        auto *layout=new QVBoxLayout(toast);layout->setContentsMargins(22,13,22,13);auto *text=new QLabel(message,toast);text->setWordWrap(true);text->setAlignment(Qt::AlignCenter);layout->addWidget(text);
        toast->setFixedWidth(qBound(280,host->width()-80,520));toast->adjustSize();
        const int endX=(host->width()-toast->width())/2;const int endY=14;
        const QRect hidden(endX,-toast->height()-8,toast->width(),toast->height());const QRect shown(endX,endY,toast->width(),toast->height());toast->setGeometry(hidden);toast->show();toast->raise();
        auto *sequence=new QSequentialAnimationGroup(toast);auto *enter=new QPropertyAnimation(toast,"geometry",sequence);enter->setDuration(260);enter->setStartValue(hidden);enter->setEndValue(shown);enter->setEasingCurve(QEasingCurve::OutCubic);sequence->addAnimation(enter);sequence->addPause(2000);auto *leave=new QPropertyAnimation(toast,"geometry",sequence);leave->setDuration(240);leave->setStartValue(shown);leave->setEndValue(hidden);leave->setEasingCurve(QEasingCurve::InCubic);sequence->addAnimation(leave);QObject::connect(sequence,&QSequentialAnimationGroup::finished,toast,&QWidget::close);sequence->start();
    }
};

}
