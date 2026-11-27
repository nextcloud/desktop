/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "assistantattachment.h"
#include "assistantimageprovider.h"

#include "account.h"
#include "common/utility.h"
#include "networkjobs.h"

#include <QBuffer>
#include <QImage>
#include <QImageReader>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QUrlQuery>

using namespace Qt::StringLiterals;

namespace OCC
{
namespace
{
constexpr auto previewExtent = 320;
constexpr auto maximumPreviewBytes = 4 * 1024 * 1024;
constexpr auto maximumPreviewPixels = 16 * 1024 * 1024;
constexpr auto attachmentTimeoutMs = 30000;
}

AssistantAttachment::AssistantAttachment(const AccountPtr &account, qint64 fileId, qint64 taskId, QObject *parent)
    : QObject(parent)
    , _account(account)
    , _fileId(fileId)
    , _taskId(taskId)
{
    //: %1 is the numeric identifier of a chat attachment whose filename is not available.
    _name = tr("File %1").arg(fileId);
    _deadline.setSingleShot(true);
    _deadline.setInterval(attachmentTimeoutMs);
    connect(&_deadline, &QTimer::timeout, this, [this] {
        stopRequest();
        finishPreview({});
    });
}

AssistantAttachment::~AssistantAttachment()
{
    stopRequest();
    AssistantImageProvider::remove(_previewSource);
}

QString AssistantAttachment::outputPath(const QString &action) const
{
    return u"/ocs/v2.php/apps/assistant/api/v1/task/%1/output-file/%2/%3"_s.arg(_taskId).arg(_fileId).arg(action);
}

QUrl AssistantAttachment::openUrl() const
{
    return _storedFile ? _account->internalLinkUrl(QByteArray::number(_fileId)) : QUrl{};
}

QUrl AssistantAttachment::downloadUrl() const
{
    if (!_storedFile && _taskId <= 0) {
        return {};
    }
    return Utility::concatUrlPath(_account->url(),
                                  _storedFile ? u"/ocs/v2.php/apps/assistant/api/v1/file/%1/display"_s.arg(_fileId) : outputPath(u"download"_s));
}

void AssistantAttachment::stopRequest()
{
    if (!_job) {
        return;
    }
    const auto job = _job.data();
    _job = nullptr;
    job->disconnect(this);
    if (job->reply()) {
        job->reply()->disconnect(this);
        job->reply()->abort();
    }
    job->deleteLater();
}

void AssistantAttachment::cancel()
{
    _deadline.stop();
    stopRequest();
    AssistantImageProvider::remove(_previewSource);
    _previewSource.clear();
    _loading = false;
    Q_EMIT changed();
}

void AssistantAttachment::load()
{
    cancel();
    _storedFile = false;
    _previewFailed = false;
    _loading = true;
    _deadline.start();
    Q_EMIT changed();

    // A task can reference a file already stored in Files; resolve that before using appdata output routes.
    const auto job = new JsonApiJob(_account, u"/ocs/v2.php/apps/assistant/api/v1/file/%1/info"_s.arg(_fileId), this);
    _job = job;
    job->setTimeout(attachmentTimeoutMs);
    job->setFollowRedirects(false);
    connect(job, &JsonApiJob::jsonReceived, this, [this, job](const QJsonDocument &json, int status) {
        _job = nullptr;
        if (job->reply()->error() == QNetworkReply::NoError && (status == 100 || status == 200)) {
            const auto info = json.object().value("ocs"_L1).toObject().value("data"_L1).toObject();
            const auto name = info.value("name"_L1).toString();
            if (!name.isEmpty()) {
                _name = name;
                _storedFile = true;
            }
        }
        if (!_storedFile && (status != 404 || _taskId <= 0)) {
            finishPreview({});
            return;
        }
        loadPreview();
    });
    job->start();
}

void AssistantAttachment::loadPreview()
{
    const auto job = new SimpleApiJob(_account, _storedFile ? u"/index.php/core/preview"_s : outputPath(u"preview"_s), this);
    _job = job;
    auto query = QUrlQuery{};
    query.addQueryItem(u"x"_s, QString::number(previewExtent));
    query.addQueryItem(u"y"_s, QString::number(previewExtent));
    query.addQueryItem(u"a"_s, u"1"_s);
    if (_storedFile) {
        query.addQueryItem(u"fileId"_s, QString::number(_fileId));
    }
    job->addQueryParams(query);
    job->setTimeout(attachmentTimeoutMs);
    // Unsupported previews may redirect to an icon; the card supplies its own fallback.
    job->setFollowRedirects(false);
    connect(job, &SimpleApiJob::resultReceived, this, [this, job](int status) {
        _job = nullptr;
        const auto reply = job->reply();
        finishPreview(status == 200 && reply->error() == QNetworkReply::NoError && reply->bytesAvailable() <= maximumPreviewBytes ? reply->readAll()
                                                                                                                                  : QByteArray{});
    });
    job->start();
    connect(job->reply(), &QIODevice::readyRead, this, [job] {
        if (job->reply()->bytesAvailable() > maximumPreviewBytes) {
            job->reply()->abort();
        }
    });
    Q_EMIT changed();
}

void AssistantAttachment::finishPreview(const QByteArray &data)
{
    _deadline.stop();
    auto input = QBuffer{};
    input.setData(data);
    input.open(QIODevice::ReadOnly);
    auto reader = QImageReader{&input};
    const auto size = reader.size();
    if (size.isValid() && qint64(size.width()) * size.height() <= maximumPreviewPixels) {
        reader.setScaledSize(size.scaled(previewExtent, previewExtent, Qt::KeepAspectRatio));
        const auto image = reader.read();
        if (!image.isNull()) {
            _previewSource = AssistantImageProvider::insert(image);
        }
    }
    _loading = false;
    _previewFailed = _previewSource.isEmpty();
    Q_EMIT changed();
}
}
