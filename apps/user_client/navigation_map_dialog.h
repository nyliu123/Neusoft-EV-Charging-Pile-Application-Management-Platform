#pragma once

#include <QDialog>
#include <QPointF>
#include <QUrl>
#include <QVector>

class QComboBox;
class QLabel;
class QNetworkAccessManager;
class QNetworkReply;
class QPushButton;
class QTimer;
class QWebEngineProfile;
class QWebEngineView;

class NavigationMapDialog final : public QDialog {
    Q_OBJECT
public:
    NavigationMapDialog(double originLongitude, double originLatitude,
                        double destinationLongitude, double destinationLatitude,
                        const QString &originAddress, const QString &destinationName,
                        const QString &destinationAddress, int initialMode,
                        QWidget *parent = nullptr);
    ~NavigationMapDialog() override;

    static QUrl directionsUrl(double originLongitude, double originLatitude,
                              double destinationLongitude, double destinationLatitude,
                              int travelMode);

private:
    void loadMap();
    void requestRouteSegment(quint64 generation);
    void handleRouteReply(QNetworkReply *reply, quint64 generation);
    void renderRoute(const QVector<QPointF> &route, double distanceKm,
                     int durationSeconds);
    void showFailure(const QString &message);

    double originLongitude_;
    double originLatitude_;
    double destinationLongitude_;
    double destinationLatitude_;
    QString originAddress_;
    QString destinationName_;
    QString destinationAddress_;
    QWebEngineProfile *profile_;
    QWebEngineView *view_;
    QComboBox *mode_;
    QLabel *status_;
    QLabel *routeSummary_;
    QTimer *timeout_;
    QNetworkAccessManager *network_;
    QNetworkReply *reply_ = nullptr;
    quint64 generation_ = 0;
    int activeTravelMode_ = 0;
    int segmentIndex_ = 0;
    int segmentCount_ = 1;
    QVector<QPointF> accumulatedRoute_;
    double accumulatedDistanceKm_ = 0.0;
    int accumulatedDurationSeconds_ = 0;
};
