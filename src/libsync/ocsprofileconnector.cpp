#include "ocsprofileconnector.h"
#include "accountfwd.h"
#include "common/result.h"
#include "networkjobs.h"
#include "iconjob.h"
#include "theme.h"
#include "account.h"

#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonArray>
#include <QLoggingCategory>
#include <QIcon>
#include <QPainter>
#include <QImage>
#include <QSvgRenderer>
#include <QNetworkReply>
#include <QPixmap>
#include <QPixmapCache>

namespace {
Q_LOGGING_CATEGORY(lcOcsProfileConnector, "nextcloud.gui.ocsprofileconnector", QtInfoMsg)

OCC::HovercardAction jsonToAction(const QJsonObject &jsonActionObject)
{
    const auto iconUrl = jsonActionObject.value(QStringLiteral("icon")).toString(QStringLiteral("no-icon"));
    QPixmap iconPixmap;
    OCC::HovercardAction hovercardAction{
        jsonActionObject.value(QStringLiteral("title")).toString(QStringLiteral("No title")), iconUrl,
        jsonActionObject.value(QStringLiteral("hyperlink")).toString(QStringLiteral("no-link"))};
    if (QPixmapCache::find(iconUrl, &iconPixmap)) {
        hovercardAction._icon = iconPixmap;
    }
    return hovercardAction;
}

OCC::Hovercard jsonToHovercard(const QJsonArray &jsonDataArray)
{
    OCC::Hovercard hovercard;
    hovercard._actions.reserve(jsonDataArray.size());
    for (const auto &jsonEntry : jsonDataArray) {
        Q_ASSERT(jsonEntry.isObject());
        if (!jsonEntry.isObject()) {
            continue;
        }
        hovercard._actions.push_back(jsonToAction(jsonEntry.toObject()));
    }
    return hovercard;
}

OCC::Optional<QPixmap> createPixmapFromSvgData(const QByteArray &iconData)
{
    QSvgRenderer svgRenderer;
    if (!svgRenderer.load(iconData)) {
        return {};
    }
    QSize imageSize{16, 16};
    if (OCC::Theme::isHidpi()) {
        imageSize = QSize{32, 32};
    }
    QImage scaledSvg(imageSize, QImage::Format_ARGB32);
    scaledSvg.fill("transparent");
    QPainter svgPainter{&scaledSvg};
    svgRenderer.render(&svgPainter);
    return QPixmap::fromImage(scaledSvg);
}

OCC::Optional<QPixmap> iconDataToPixmap(const QByteArray iconData)
{
    if (!iconData.startsWith("<svg")) {
        return {};
    }
    return createPixmapFromSvgData(iconData);
}
}

namespace OCC {

HovercardAction::HovercardAction() = default;

HovercardAction::HovercardAction(QString title, QUrl iconUrl, QUrl link)
    : _title(std::move(title))
    , _iconUrl(std::move(iconUrl))
    , _link(std::move(link))
{
}

OcsProfileConnector::OcsProfileConnector(AccountPtr account, QObject *parent)
    : QObject(parent)
    , _account(account)
{
}

void OcsProfileConnector::fetchHovercard(const QString &userId)
{
    if (_account->serverVersionInt() < Account::makeServerVersion(23, 0, 0)) {
        qInfo(lcOcsProfileConnector) << "Server version" << _account->serverVersion()
                                     << "does not support profile page";
        emit error();
        return;
    }
    const QString url = QStringLiteral("/ocs/v2.php/hovercard/v1/%1").arg(userId);
    const auto job = new JsonApiJob(_account, url, this);
    connect(job, &JsonApiJob::jsonReceived, this, &OcsProfileConnector::onHovercardFetched);
    job->start();
}

void OcsProfileConnector::onHovercardFetched(const QJsonDocument &json, int statusCode)
{
    qCDebug(lcOcsProfileConnector) << "Hovercard fetched:" << json;

    if (statusCode != 200) {
        qCInfo(lcOcsProfileConnector) << "Fetching of hovercard finished with status code" << statusCode;
        return;
    }
    const auto jsonData = json.object().value("ocs").toObject().value("data").toObject().value("actions");
    Q_ASSERT(jsonData.isArray());
    _currentHovercard = jsonToHovercard(jsonData.toArray());
    fetchIcons();
    emit hovercardFetched();
}

void OcsProfileConnector::setHovercardActionIcon(const std::size_t index, const QPixmap &pixmap)
{
    auto &hovercardAction = _currentHovercard._actions[index];
    QPixmapCache::insert(hovercardAction._iconUrl.toString(), pixmap);
    hovercardAction._icon = pixmap;
    emit iconLoaded(index);
}

void OcsProfileConnector::loadHovercardActionIcon(const std::size_t hovercardActionIndex, const QByteArray &iconData)
{
    if (hovercardActionIndex >= _currentHovercard._actions.size()) {
        // Note: Probably could do more checking, like checking if the url is still the same.
        return;
    }
    const auto icon = iconDataToPixmap(iconData);
    if (icon.isValid()) {
        setHovercardActionIcon(hovercardActionIndex, icon.get());
        return;
    }
    qCWarning(lcOcsProfileConnector) << "Could not load Svg icon from data" << iconData;
}

void OcsProfileConnector::startFetchIconJob(const std::size_t hovercardActionIndex)
{
    const auto hovercardAction = _currentHovercard._actions[hovercardActionIndex];
    const auto iconJob = new IconJob{_account, hovercardAction._iconUrl, this};
    connect(iconJob, &IconJob::jobFinished,
        [this, hovercardActionIndex](QByteArray iconData) { loadHovercardActionIcon(hovercardActionIndex, iconData); });
    connect(iconJob, &IconJob::error, this, [](QNetworkReply::NetworkError errorType) {
        qCWarning(lcOcsProfileConnector) << "Could not fetch icon:" << errorType;
    });
}

void OcsProfileConnector::fetchIcons()
{
    for (auto hovercardActionIndex = 0u; hovercardActionIndex < _currentHovercard._actions.size();
         ++hovercardActionIndex) {
        startFetchIconJob(hovercardActionIndex);
    }
}

const Hovercard &OcsProfileConnector::hovercard() const
{
    return _currentHovercard;
}
}
