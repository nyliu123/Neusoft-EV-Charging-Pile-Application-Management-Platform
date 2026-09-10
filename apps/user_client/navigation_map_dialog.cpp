#include "navigation_map_dialog.h"

#include "client_ui/animated_combo_box.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPushButton>
#include <QTimer>
#include <QUrlQuery>
#include <QVBoxLayout>
#include <QWebEngineProfile>
#include <QWebEnginePage>
#include <QWebEngineSettings>
#include <QWebEngineView>
#include <QtMath>

#include <limits>

namespace {

bool validCoordinate(double longitude, double latitude)
{
    return qIsFinite(longitude) && qIsFinite(latitude)
        && longitude >= -180.0 && longitude <= 180.0
        && latitude >= -90.0 && latitude <= 90.0;
}

double directDistanceKm(double originLongitude, double originLatitude,
                        double destinationLongitude, double destinationLatitude)
{
    constexpr double earthRadiusKm = 6371.0;
    const double latitudeDelta = qDegreesToRadians(destinationLatitude - originLatitude);
    const double longitudeDelta = qDegreesToRadians(destinationLongitude - originLongitude);
    const double a = qPow(qSin(latitudeDelta / 2.0), 2)
        + qCos(qDegreesToRadians(originLatitude)) * qCos(qDegreesToRadians(destinationLatitude))
            * qPow(qSin(longitudeDelta / 2.0), 2);
    return earthRadiusKm * 2.0 * qAtan2(qSqrt(a), qSqrt(1.0 - a));
}

QString distanceText(double distanceKm)
{
    if (distanceKm < 0.1) {
        return QStringLiteral("< 100 米");
    }
    if (distanceKm < 1.0) {
        return QStringLiteral("%1 米").arg(qRound(distanceKm * 1000.0));
    }
    QString value = QString::number(distanceKm, 'f', 1);
    if (value.endsWith(QStringLiteral(".0"))) {
        value.chop(2);
    }
    return QStringLiteral("%1 公里").arg(value);
}

QVector<QPointF> decodePolyline6(const QString &encoded)
{
    QVector<QPointF> points;
    qint64 latitude = 0;
    qint64 longitude = 0;
    qsizetype index = 0;
    const auto decodeValue = [&encoded, &index](qint64 *value) {
        qint64 result = 0;
        int shift = 0;
        while (index < encoded.size()) {
            const int byte = encoded.at(index++).unicode() - 63;
            if (byte < 0) {
                return false;
            }
            result |= static_cast<qint64>(byte & 0x1f) << shift;
            shift += 5;
            if (byte < 0x20) {
                *value = (result & 1) ? ~(result >> 1) : (result >> 1);
                return true;
            }
            if (shift > 60) {
                return false;
            }
        }
        return false;
    };
    while (index < encoded.size()) {
        qint64 latitudeDelta = 0;
        qint64 longitudeDelta = 0;
        if (!decodeValue(&latitudeDelta) || !decodeValue(&longitudeDelta)) {
            return {};
        }
        latitude += latitudeDelta;
        longitude += longitudeDelta;
        points.append(QPointF(longitude / 1000000.0, latitude / 1000000.0));
    }
    return points;
}

QPointF worldPixel(const QPointF &coordinate, int zoom)
{
    const double scale = 256.0 * (1 << zoom);
    const double latitude = qBound(-85.05112878, coordinate.y(), 85.05112878);
    const double sine = qSin(qDegreesToRadians(latitude));
    return QPointF((coordinate.x() + 180.0) / 360.0 * scale,
                   (0.5 - qLn((1.0 + sine) / (1.0 - sine)) / (4.0 * M_PI)) * scale);
}

QString tileTemplate()
{
    const QString configured = qEnvironmentVariable("EV_NAVIGATION_TILE_TEMPLATE").trimmed();
    return configured.isEmpty()
        ? QStringLiteral("https://tile.openstreetmap.de/{z}/{x}/{y}.png")
        : configured;
}

} // namespace

QUrl NavigationMapDialog::directionsUrl(double originLongitude, double originLatitude,
                                        double destinationLongitude, double destinationLatitude,
                                        int travelMode)
{
    if (!validCoordinate(originLongitude, originLatitude)
        || !validCoordinate(destinationLongitude, destinationLatitude)) {
        return {};
    }
    const QString configured = qEnvironmentVariable("EV_NAVIGATION_ROUTE_ENDPOINT").trimmed();
    QUrl url(configured.isEmpty()
        ? QStringLiteral("https://valhalla1.openstreetmap.de/route") : configured);
    const QJsonObject request {
        {QStringLiteral("locations"), QJsonArray {
            QJsonObject {{QStringLiteral("lat"), originLatitude},
                         {QStringLiteral("lon"), originLongitude}},
            QJsonObject {{QStringLiteral("lat"), destinationLatitude},
                         {QStringLiteral("lon"), destinationLongitude}}
        }},
        {QStringLiteral("costing"), travelMode == 1 ? QStringLiteral("pedestrian")
                                   : travelMode == 2 ? QStringLiteral("bicycle")
                                                     : QStringLiteral("auto")},
        {QStringLiteral("units"), QStringLiteral("kilometers")},
        {QStringLiteral("language"), QStringLiteral("zh-CN")}
    };
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("json"), QString::fromUtf8(
        QJsonDocument(request).toJson(QJsonDocument::Compact)));
    url.setQuery(query);
    return url;
}

NavigationMapDialog::NavigationMapDialog(
    double originLongitude, double originLatitude, double destinationLongitude,
    double destinationLatitude, const QString &originAddress, const QString &destinationName,
    const QString &destinationAddress, int initialMode, QWidget *parent)
    : QDialog(parent), originLongitude_(originLongitude), originLatitude_(originLatitude),
      destinationLongitude_(destinationLongitude), destinationLatitude_(destinationLatitude),
      originAddress_(originAddress), destinationName_(destinationName),
      destinationAddress_(destinationAddress)
{
    setWindowTitle(QStringLiteral("地图导航 · %1").arg(destinationName));
    setObjectName(QStringLiteral("navigationMapDialog"));
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowModality(Qt::WindowModal);
    resize(410, 720);
    setMinimumSize(0, 0);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(12, 12, 12, 10);
    layout->setSpacing(8);
    auto *toolbar = new QVBoxLayout;
    auto *back = new QPushButton(QStringLiteral("< 返回站点"), this);
    toolbar->addWidget(back, 0, Qt::AlignLeft);
    auto *title = new QLabel(destinationName, this);
    title->setTextFormat(Qt::PlainText);
    title->setWordWrap(true);
    title->setProperty("uiClass", "pageTitle");
    toolbar->addWidget(title, 1);
    auto *actions = new QHBoxLayout;
    actions->addWidget(new QLabel(QStringLiteral("出行方式："), this));
    mode_ = new ev::AnimatedComboBox(this);
    mode_->setObjectName(QStringLiteral("navigationMode"));
    mode_->addItems({QStringLiteral("驾车"), QStringLiteral("步行"),
                     QStringLiteral("骑行")});
    mode_->setCurrentIndex(qBound(0, initialMode, 2));
    actions->addWidget(mode_, 1);
    toolbar->addLayout(actions);
    layout->addLayout(toolbar);

    status_ = new QLabel(this);
    status_->setObjectName(QStringLiteral("mapLoadStatus"));
    status_->setProperty("uiClass", "mapStatus");
    status_->setAlignment(Qt::AlignCenter);
    status_->setWordWrap(true);
    status_->hide();
    layout->addWidget(status_, 1);

    profile_ = new QWebEngineProfile(QStringLiteral("navigation"), this);
    profile_->setPersistentCookiesPolicy(QWebEngineProfile::NoPersistentCookies);
    profile_->setHttpUserAgent(QStringLiteral("EVChargingPlatform/1.0 QtEmbeddedMap"));
    view_ = new QWebEngineView(this);
    view_->setPage(new QWebEnginePage(profile_, view_));
    view_->setObjectName(QStringLiteral("embeddedMapView"));
    view_->settings()->setAttribute(QWebEngineSettings::JavascriptEnabled, true);
    view_->settings()->setAttribute(QWebEngineSettings::JavascriptCanOpenWindows, false);
    view_->settings()->setAttribute(QWebEngineSettings::LocalContentCanAccessRemoteUrls, true);
    layout->addWidget(view_, 1);
    routeSummary_ = new QLabel(this);
    routeSummary_->setObjectName(QStringLiteral("routeSummary"));
    routeSummary_->setProperty("uiClass", "stationMeta");
    routeSummary_->setWordWrap(true);
    routeSummary_->hide();
    layout->addWidget(routeSummary_);

    network_ = new QNetworkAccessManager(this);
    timeout_ = new QTimer(this);
    timeout_->setSingleShot(true);
    timeout_->setInterval(20000);
    connect(timeout_, &QTimer::timeout, this, [this] {
        ++generation_;
        if (reply_) {
            reply_->abort();
        }
        showFailure(QStringLiteral("路线规划超时，请检查网络后重试。"));
    });
    connect(mode_, &QComboBox::currentIndexChanged, this, &NavigationMapDialog::loadMap);
    connect(back, &QPushButton::clicked, this, &QDialog::close);
    QTimer::singleShot(0, this, &NavigationMapDialog::loadMap);
}

NavigationMapDialog::~NavigationMapDialog()
{
    delete view_;
}

void NavigationMapDialog::loadMap()
{
    const quint64 generation = ++generation_;
    timeout_->stop();
    if (reply_) {
        reply_->abort();
        reply_->deleteLater();
        reply_ = nullptr;
    }
    if (!validCoordinate(originLongitude_, originLatitude_)
        || !validCoordinate(destinationLongitude_, destinationLatitude_)) {
        showFailure(QStringLiteral("起终点坐标无效，请返回站点重新定位。"));
        return;
    }
    activeTravelMode_ = mode_->currentIndex();
    const double distance = directDistanceKm(originLongitude_, originLatitude_,
                                             destinationLongitude_, destinationLatitude_);
    const double maximumSegmentKm = activeTravelMode_ == 1 ? 65.0
        : activeTravelMode_ == 2 ? 100.0 : std::numeric_limits<double>::max();
    segmentCount_ = qMax(1, qCeil(distance / maximumSegmentKm));
    segmentIndex_ = 0;
    accumulatedRoute_.clear();
    accumulatedDistanceKm_ = 0.0;
    accumulatedDurationSeconds_ = 0;
    routeSummary_->hide();
    status_->setText(QStringLiteral("正在加载地图…\n正在规划%1路线").arg(mode_->currentText()));
    status_->show();
    view_->hide();
    requestRouteSegment(generation);
}

void NavigationMapDialog::requestRouteSegment(quint64 generation)
{
    const double fromRatio = static_cast<double>(segmentIndex_) / segmentCount_;
    const double toRatio = static_cast<double>(segmentIndex_ + 1) / segmentCount_;
    const auto interpolate = [](double from, double to, double ratio) {
        return from + (to - from) * ratio;
    };
    const QUrl url = directionsUrl(
        interpolate(originLongitude_, destinationLongitude_, fromRatio),
        interpolate(originLatitude_, destinationLatitude_, fromRatio),
        interpolate(originLongitude_, destinationLongitude_, toRatio),
        interpolate(originLatitude_, destinationLatitude_, toRatio), activeTravelMode_);
    status_->setText(segmentCount_ > 1
        ? QStringLiteral("正在规划%1路线（%2/%3）…")
              .arg(mode_->currentText()).arg(segmentIndex_ + 1).arg(segmentCount_)
        : QStringLiteral("正在规划%1路线…").arg(mode_->currentText()));
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      QStringLiteral("EVChargingPlatform/1.0 QtNavigation"));
    reply_ = network_->get(request);
    QNetworkReply *reply = reply_;
    connect(reply, &QNetworkReply::finished, this, [this, reply, generation] {
        handleRouteReply(reply, generation);
    });
    timeout_->start();
}

void NavigationMapDialog::handleRouteReply(QNetworkReply *reply, quint64 generation)
{
    if (reply == reply_) {
        reply_ = nullptr;
    }
    reply->deleteLater();
    if (generation != generation_) {
        return;
    }
    timeout_->stop();
    if (reply->error() != QNetworkReply::NoError) {
        showFailure(QStringLiteral("路线规划失败：%1").arg(reply->errorString()));
        return;
    }
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(reply->readAll(), &parseError);
    const QJsonObject trip = document.object().value(QStringLiteral("trip")).toObject();
    const QJsonObject summary = trip.value(QStringLiteral("summary")).toObject();
    const QJsonArray legs = trip.value(QStringLiteral("legs")).toArray();
    if (parseError.error != QJsonParseError::NoError
        || trip.value(QStringLiteral("status")).toInt(-1) != 0 || legs.isEmpty()) {
        showFailure(QStringLiteral("未找到可行路线，请尝试其他出行方式。"));
        return;
    }
    const QVector<QPointF> route = decodePolyline6(
        legs.first().toObject().value(QStringLiteral("shape")).toString());
    if (route.size() < 2) {
        showFailure(QStringLiteral("路线数据不完整，请切换出行方式后重试。"));
        return;
    }
    if (accumulatedRoute_.isEmpty()) {
        accumulatedRoute_ = route;
    } else {
        accumulatedRoute_.reserve(accumulatedRoute_.size() + route.size() - 1);
        for (qsizetype index = 1; index < route.size(); ++index) {
            accumulatedRoute_.append(route.at(index));
        }
    }
    accumulatedDistanceKm_ += summary.value(QStringLiteral("length")).toDouble();
    accumulatedDurationSeconds_ += qRound(summary.value(QStringLiteral("time")).toDouble());
    ++segmentIndex_;
    if (segmentIndex_ < segmentCount_) {
        requestRouteSegment(generation);
        return;
    }
    renderRoute(accumulatedRoute_, accumulatedDistanceKm_, accumulatedDurationSeconds_);
}

void NavigationMapDialog::renderRoute(const QVector<QPointF> &route, double distanceKm,
                                      int durationSeconds)
{
    const int viewportWidth = qMax(640, view_->width());
    const int viewportHeight = qMax(400, view_->height());
    constexpr int padding = 70;
    int zoom = 15;
    QPointF topLeft;
    QPointF bottomRight;
    for (; zoom >= 2; --zoom) {
        double minX = std::numeric_limits<double>::max();
        double minY = std::numeric_limits<double>::max();
        double maxX = std::numeric_limits<double>::lowest();
        double maxY = std::numeric_limits<double>::lowest();
        for (const QPointF &coordinate : route) {
            const QPointF pixel = worldPixel(coordinate, zoom);
            minX = qMin(minX, pixel.x());
            minY = qMin(minY, pixel.y());
            maxX = qMax(maxX, pixel.x());
            maxY = qMax(maxY, pixel.y());
        }
        topLeft = QPointF(minX, minY);
        bottomRight = QPointF(maxX, maxY);
        if (maxX - minX <= viewportWidth - padding * 2
            && maxY - minY <= viewportHeight - padding * 2) {
            break;
        }
    }
    const QPointF center = (topLeft + bottomRight) / 2.0;
    const double fittedWorldSize = 256.0 * (1 << zoom);
    QJsonArray routeJson;
    for (const QPointF &coordinate : route) {
        routeJson.append(QJsonArray {coordinate.x(), coordinate.y()});
    }
    const int minutes = qMax(1, qRound(durationSeconds / 60.0));
    const int hours = minutes / 60;
    const int remainingMinutes = minutes % 60;
    const QString formattedDuration = hours == 0
        ? QStringLiteral("%1 分钟").arg(minutes)
        : remainingMinutes == 0
            ? QStringLiteral("%1 小时").arg(hours)
            : QStringLiteral("%1 小时 %2 分钟").arg(hours).arg(remainingMinutes);
    const QString modeColor = mode_->currentIndex() == 1 ? QStringLiteral("#237746")
        : mode_->currentIndex() == 2 ? QStringLiteral("#9653c4")
                                     : QStringLiteral("#007aff");
    const auto jsonLiteral = [](const QString &value) {
        const QByteArray array = QJsonDocument(QJsonArray {value}).toJson(QJsonDocument::Compact);
        return QString::fromUtf8(array.mid(1, array.size() - 2));
    };
    const QString formattedDistance = distanceText(distanceKm);
    routeSummary_->setText(QStringLiteral("路线详情\n出行方式：%1    路线距离：%2\n预计耗时：%3")
        .arg(mode_->currentText(), formattedDistance, formattedDuration));
    QString html = QStringLiteral(R"HTML(
<!doctype html><html><head><meta charset="utf-8"><title>地图路线</title><style>
html,body{margin:0;width:100%;height:100%;overflow:hidden;background:#f5f5f7;font-family:-apple-system,BlinkMacSystemFont,'Segoe UI','Noto Sans CJK SC',sans-serif;user-select:none}
#map{position:absolute;inset:0;overflow:hidden;cursor:grab;touch-action:none}#map.dragging{cursor:grabbing}
#tiles,.overlay{position:absolute;inset:0}.tile{position:absolute;max-width:none}.overlay{width:100%;height:100%;pointer-events:none}
.route{fill:none;stroke:__COLOR__;stroke-width:6;stroke-linecap:round;stroke-linejoin:round;filter:drop-shadow(0 1px 2px #fff)}
.pin{position:absolute;width:30px;height:30px;margin:-18px;border-radius:50%;color:#fff;text-align:center;line-height:30px;font-weight:bold;border:3px solid #fff;box-shadow:0 2px 7px #555}
.start{background:#237746}.end{background:#d93025}.marker-label{position:absolute;color:#111;background:rgba(255,255,255,.9);padding:3px 6px;border-radius:4px;font-size:13px;font-weight:600;white-space:nowrap;box-shadow:0 1px 3px rgba(0,0,0,.25)}
#bottomControls{position:absolute;right:16px;bottom:16px;display:flex;align-items:flex-end;gap:8px}#scaleControl{min-width:112px;background:rgba(255,255,255,.94);border:1px solid #d9d9e1;border-radius:8px;box-shadow:0 4px 16px rgba(29,29,31,.12);padding:5px 9px;color:#1d1d1f;font-size:12px}
#scaleLine{height:6px;border-left:2px solid #1d1d1f;border-right:2px solid #1d1d1f;border-bottom:2px solid #1d1d1f;margin-top:2px}
#zoomControls{display:flex;align-items:center;background:#fff;border:1px solid #d9d9e1;border-radius:8px;box-shadow:0 4px 16px rgba(29,29,31,.12);overflow:hidden}
#zoomControls button{width:38px;height:38px;border:0;background:#fff;font-size:23px;cursor:pointer;color:#27313b}#zoomControls button:hover{background:#eef2f6}#zoomControls button:active{background:#dcecff}#zoomControls button:focus-visible{outline:2px solid #007aff;outline-offset:-3px}#zoomControls button:disabled{color:#a0a6af;background:#f3f4f6;cursor:default}#zoomControls #resetZoom{width:58px;font-size:14px;font-weight:600;border-left:1px solid #ddd}#zoomPercent{width:58px;text-align:center;font-size:13px;border-left:1px solid #ddd;border-right:1px solid #ddd}
.attribution{position:absolute;left:4px;bottom:3px;background:rgba(255,255,255,.88);padding:3px 6px;font-size:11px;color:#333}.attribution a{color:#075da8}
</style></head><body><div id="map"><div id="tiles"></div><svg class="overlay"><polyline id="route" class="route"/></svg>
<div id="start" class="pin start">起</div><div id="startLabel" class="marker-label"></div><div id="end" class="pin end">终</div><div id="endLabel" class="marker-label"></div>
<div class="attribution">© <a href="https://www.openstreetmap.org/copyright">OpenStreetMap contributors</a> · Route: Valhalla</div>
<div id="bottomControls"><div id="scaleControl"><span id="scaleText"></span><div id="scaleLine"></div></div><div id="zoomControls"><button id="zoomOut" title="缩小">−</button><span id="zoomPercent">100%</span><button id="zoomIn" title="放大">+</button><button id="resetZoom" title="重置初始显示">重置</button></div></div></div>
<script>
const routeCoordinates=__ROUTE__,tileTemplate=__TILES__,modeName=__MODE__,routeDistance=__DISTANCE_TEXT__,routeDuration=__DURATION_TEXT__,originAddress=__ORIGIN_ADDRESS__,destinationAddress=__DESTINATION_ADDRESS__;
const baseZoom=__ZOOM__,initialCenterX=__CENTER_X__,initialCenterY=__CENTER_Y__,zoomLevels=[25,50,75,100,125,150,200,300,400,500,750,1000,1500,2000];let centerX=initialCenterX,centerY=initialCenterY,zoomPercent=100,dragging=false,lastX=0,lastY=0,frame=0;
const map=document.getElementById('map'),tiles=document.getElementById('tiles'),routeLine=document.getElementById('route');
document.getElementById('startLabel').textContent=originAddress;document.getElementById('endLabel').textContent=destinationAddress;
function normalized(coordinate){const lon=coordinate[0],lat=Math.max(-85.05112878,Math.min(85.05112878,coordinate[1]));const sine=Math.sin(lat*Math.PI/180);return [(lon+180)/360,.5-Math.log((1+sine)/(1-sine))/(4*Math.PI)]}
const normalizedRoute=routeCoordinates.map(normalized);
function worldSize(){return 256*Math.pow(2,baseZoom)*(zoomPercent/100)}
function screenPoint(point){const size=worldSize();return [(point[0]-centerX)*size+map.clientWidth/2,(point[1]-centerY)*size+map.clientHeight/2]}
function formatScale(meters){if(meters<1000)return Math.round(meters)+' 米';const kilometers=meters/1000;return (kilometers>=10?Math.round(kilometers):Math.round(kilometers*10)/10)+' 公里'}
function scaleMetrics(){const latitude=Math.atan(Math.sinh(Math.PI*(1-2*centerY))),metersPerPixel=Math.cos(latitude)*40075016.686/worldSize(),target=metersPerPixel*100,power=Math.pow(10,Math.floor(Math.log10(target))),ratio=target/power,nice=Math.max(5,(ratio>=5?5:ratio>=2?2:1)*power);return {metersPerPixel,nice}}
function updateScale(){const scale=scaleMetrics();document.getElementById('scaleText').textContent=formatScale(scale.nice);document.getElementById('scaleLine').style.width=Math.min(140,Math.max(30,scale.nice/scale.metersPerPixel))+'px';document.getElementById('zoomIn').disabled=scale.nice<=5}
function positionMarker(pin,label,point){pin.style.transform='translate('+point[0]+'px,'+point[1]+'px)';const labelWidth=label.offsetWidth,labelX=point[0]+22+labelWidth>map.clientWidth?point[0]-22-labelWidth:point[0]+22;label.style.transform='translate('+labelX+'px,'+(point[1]-12)+'px)'}
function render(){frame=0;const effectiveZoom=baseZoom+Math.log2(zoomPercent/100),tileZoom=Math.max(2,Math.min(19,Math.floor(effectiveZoom))),scale=Math.pow(2,effectiveZoom-tileZoom),count=Math.pow(2,tileZoom),tileSize=256*scale;
 const centerTileX=centerX*256*count,centerTileY=centerY*256*count,left=centerTileX-map.clientWidth/(2*scale),top=centerTileY-map.clientHeight/(2*scale),right=centerTileX+map.clientWidth/(2*scale),bottom=centerTileY+map.clientHeight/(2*scale);
 tiles.replaceChildren();for(let y=Math.floor(top/256);y<=Math.floor(bottom/256);y++){if(y<0||y>=count)continue;for(let x=Math.floor(left/256);x<=Math.floor(right/256);x++){const image=document.createElement('img');image.className='tile';image.draggable=false;const wrapped=((x%count)+count)%count;image.src=tileTemplate.replace('{z}',tileZoom).replace('{x}',wrapped).replace('{y}',y);image.style.left=((x*256-centerTileX)*scale+map.clientWidth/2)+'px';image.style.top=((y*256-centerTileY)*scale+map.clientHeight/2)+'px';image.style.width=image.style.height=tileSize+'px';tiles.appendChild(image)}}
 routeLine.setAttribute('points',normalizedRoute.map(p=>screenPoint(p).join(',')).join(' '));const start=screenPoint(normalizedRoute[0]),end=screenPoint(normalizedRoute[normalizedRoute.length-1]);positionMarker(document.getElementById('start'),document.getElementById('startLabel'),start);positionMarker(document.getElementById('end'),document.getElementById('endLabel'),end);document.getElementById('zoomPercent').textContent=Math.round(zoomPercent)+'%';updateScale()}
function requestRender(){if(!frame)frame=requestAnimationFrame(render)}
function setZoom(next,x,y){if(!Number.isFinite(next)||next<=0||next===zoomPercent||(next>zoomPercent&&scaleMetrics().nice<=5))return;const oldSize=worldSize(),anchorX=centerX+(x-map.clientWidth/2)/oldSize,anchorY=centerY+(y-map.clientHeight/2)/oldSize;zoomPercent=next;document.getElementById('zoomPercent').textContent=(zoomPercent>=10?Math.round(zoomPercent):Math.round(zoomPercent*10)/10)+'%';const nextSize=worldSize();centerX=anchorX-(x-map.clientWidth/2)/nextSize;centerY=Math.max(0,Math.min(1,anchorY-(y-map.clientHeight/2)/nextSize));requestRender()}
function stepZoom(direction,x,y){let next=direction>0?zoomLevels.find(level=>level>zoomPercent):[...zoomLevels].reverse().find(level=>level<zoomPercent);if(next===undefined)next=direction>0?zoomPercent*2:zoomPercent/2;setZoom(next,x,y)}
map.addEventListener('pointerdown',event=>{if(event.button!==0)return;dragging=true;lastX=event.clientX;lastY=event.clientY;map.classList.add('dragging');if(event.isPrimary)map.setPointerCapture(event.pointerId)});
map.addEventListener('pointermove',event=>{if(!dragging)return;const size=worldSize();centerX-=(event.clientX-lastX)/size;centerY=Math.max(0,Math.min(1,centerY-(event.clientY-lastY)/size));lastX=event.clientX;lastY=event.clientY;requestRender()});
function stopDrag(){dragging=false;map.classList.remove('dragging')}map.addEventListener('pointerup',stopDrag);map.addEventListener('pointercancel',stopDrag);
map.addEventListener('wheel',event=>{event.preventDefault();stepZoom(event.deltaY<0?1:-1,event.clientX,event.clientY)},{passive:false});
document.getElementById('zoomIn').addEventListener('click',event=>{event.stopPropagation();stepZoom(1,map.clientWidth/2,map.clientHeight/2)});document.getElementById('zoomOut').addEventListener('click',event=>{event.stopPropagation();stepZoom(-1,map.clientWidth/2,map.clientHeight/2)});document.getElementById('resetZoom').addEventListener('click',event=>{event.stopPropagation();centerX=initialCenterX;centerY=initialCenterY;zoomPercent=100;render()});
document.getElementById('bottomControls').addEventListener('pointerdown',event=>event.stopPropagation());window.addEventListener('resize',requestRender);
window.navigationMapState={get centerX(){return centerX},get centerY(){return centerY},get zoomPercent(){return zoomPercent}};render();
</script></body></html>)HTML");
    html.replace(QStringLiteral("__COLOR__"), modeColor);
    html.replace(QStringLiteral("__ROUTE__"), QString::fromUtf8(
        QJsonDocument(routeJson).toJson(QJsonDocument::Compact)));
    html.replace(QStringLiteral("__TILES__"), jsonLiteral(tileTemplate()));
    html.replace(QStringLiteral("__MODE__"), jsonLiteral(mode_->currentText()));
    html.replace(QStringLiteral("__DISTANCE_TEXT__"), jsonLiteral(formattedDistance));
    html.replace(QStringLiteral("__DURATION_TEXT__"), jsonLiteral(formattedDuration));
    html.replace(QStringLiteral("__ORIGIN_ADDRESS__"), jsonLiteral(
        originAddress_.isEmpty() ? QStringLiteral("当前搜索位置") : originAddress_));
    html.replace(QStringLiteral("__DESTINATION_ADDRESS__"), jsonLiteral(
        destinationAddress_.isEmpty() ? destinationName_ : destinationAddress_));
    html.replace(QStringLiteral("__ZOOM__"), QString::number(zoom));
    html.replace(QStringLiteral("__CENTER_X__"), QString::number(center.x() / fittedWorldSize, 'g', 16));
    html.replace(QStringLiteral("__CENTER_Y__"), QString::number(center.y() / fittedWorldSize, 'g', 16));

    connect(view_, &QWebEngineView::loadFinished, this, [this](bool ok) {
        if (!ok) {
            showFailure(QStringLiteral("地图绘制失败，请重新打开地图。"));
        }
    }, Qt::SingleShotConnection);
    view_->setHtml(html.toUtf8(), QUrl(QStringLiteral("https://www.openstreetmap.org/")));
    view_->show();
    routeSummary_->show();
    status_->clear();
    status_->hide();
}

void NavigationMapDialog::showFailure(const QString &message)
{
    view_->hide();
    routeSummary_->hide();
    status_->show();
    status_->setText(message);
}
