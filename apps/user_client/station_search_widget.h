#pragma once

#include <QJsonObject>
#include <QVector>
#include <QWidget>

class QComboBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QStackedWidget;
class QTableWidget;
class QVBoxLayout;

namespace ev {
class UserApiClient;
}

class StationSearchWidget final : public QWidget {
    Q_OBJECT

public:
    explicit StationSearchWidget(ev::UserApiClient *api, QWidget *parent = nullptr);
    void refresh();

signals:
    // Emitted when the user picks an idle pile in the station detail page;
    // the charging flow (UML-025~032) is entered with this pile.
    void pileChosen(qint64 pileId);

private:
    void search();
    void loadStations(bool hasLocation = false, double longitude = 0.0,
                      double latitude = 0.0);
    void renderStations(const QJsonObject &result, bool locationAvailable);
    void showStationDetail(const QJsonObject &station);
    void renderStationDetail(const QJsonObject &result);
    void updateNavigationAvailability();
    void startNavigation();
    // Station comment area (list / like / post with star rating).
    void showComments(qint64 stationId);
    void loadComments();
    void renderComments(const QJsonObject &result);
    void publishComment();
    void onToggleLike(qint64 commentId);
    void setStarRating(int rating);
    void setBusy(bool busy, const QString &message = {});

    ev::UserApiClient *api_ = nullptr;
    QStackedWidget *pages_ = nullptr;
    QWidget *listPage_ = nullptr;
    QWidget *detailPage_ = nullptr;
    QComboBox *areaBox_ = nullptr;
    QLineEdit *addressEdit_ = nullptr;
    QPushButton *searchButton_ = nullptr;
    QLabel *statusLabel_ = nullptr;
    QVBoxLayout *stationListLayout_ = nullptr;
    QLabel *detailTitle_ = nullptr;
    QLabel *detailMeta_ = nullptr;
    QPushButton *startNavigationButton_ = nullptr;
    QTableWidget *pileTable_ = nullptr;
    QPushButton *commentsButton_ = nullptr;
    QWidget *commentsPage_ = nullptr;
    QLabel *commentsTitle_ = nullptr;
    QLabel *commentsSummary_ = nullptr;
    QVBoxLayout *commentsListLayout_ = nullptr;
    QPlainTextEdit *commentInput_ = nullptr;
    QPushButton *publishCommentButton_ = nullptr;
    QLabel *starValueLabel_ = nullptr;
    QVector<QPushButton *> starButtons_;
    qint64 detailStationId_ = 0;
    qint64 commentsStationId_ = 0;
    int starRating_ = 0;
    bool hasOriginLocation_ = false;
    double originLongitude_ = 0.0;
    double originLatitude_ = 0.0;
    bool hasDestinationLocation_ = false;
    double destinationLongitude_ = 0.0;
    double destinationLatitude_ = 0.0;
    QString destinationName_;
    QString originAddress_;
    QString destinationAddress_;
};
