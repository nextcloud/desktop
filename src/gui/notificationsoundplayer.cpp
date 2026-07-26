/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "notificationsoundplayer.h"
#include "notificationsoundplayer_p.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QLoggingCategory>
#include <QMutex>
#include <QMutexLocker>
#include <QSaveFile>
#include <QStandardPaths>
#include <QString>
#include <QTemporaryFile>
#include <QUrl>

namespace OCC {

Q_LOGGING_CATEGORY(lcNotificationSoundPlayer, "nextcloud.gui.notificationsoundplayer", QtInfoMsg)

namespace {

QString extractQrcToCache(const QString &qrcResourcePath)
{
    static QMutex cacheMutex;
    static QHash<QString, QString> extractedPaths;

    QMutexLocker locker(&cacheMutex);
    if (const auto cached = extractedPaths.value(qrcResourcePath); !cached.isEmpty()) {
        if (QFile::exists(cached)) {
            return cached;
        }
        extractedPaths.remove(qrcResourcePath);
    }

    QFile source(qrcResourcePath);
    if (!source.open(QIODevice::ReadOnly)) {
        qCWarning(lcNotificationSoundPlayer) << "Cannot open embedded resource" << qrcResourcePath
                                             << ":" << source.errorString();
        return {};
    }
    const auto bytes = source.readAll();
    source.close();

    QByteArray fingerprint = QByteArrayLiteral("notificationsoundplayer:");
    fingerprint += qrcResourcePath.toUtf8();
    fingerprint += ':';
    fingerprint += QByteArray::number(bytes.size());
    const auto hash = QCryptographicHash::hash(fingerprint, QCryptographicHash::Sha1).toHex();

    const auto cacheDir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + QStringLiteral("/sounds");
    const auto suffix = QFileInfo(qrcResourcePath).suffix();
    const auto dottedSuffix = suffix.isEmpty() ? QString() : QStringLiteral(".") + suffix;
    const auto destinationPath = cacheDir + QStringLiteral("/") + QString::fromLatin1(hash) + dottedSuffix;

    if (QFile::exists(destinationPath)) {
        extractedPaths.insert(qrcResourcePath, destinationPath);
        return destinationPath;
    }

    if (QDir().mkpath(cacheDir)) {
        QSaveFile out(destinationPath);
        if (out.open(QIODevice::WriteOnly)) {
            if (out.write(bytes) == bytes.size() && out.commit()) {
                extractedPaths.insert(qrcResourcePath, destinationPath);
                return destinationPath;
            }
            qCWarning(lcNotificationSoundPlayer) << "Failed to write extracted sound to" << destinationPath
                                                 << ":" << out.errorString();
        } else {
            qCWarning(lcNotificationSoundPlayer) << "Failed to open" << destinationPath << "for writing:"
                                                 << out.errorString();
        }
    } else {
        qCWarning(lcNotificationSoundPlayer) << "Cannot create cache directory" << cacheDir
                                             << "for extracted sounds; falling back to QTemporaryFile";
    }

    QTemporaryFile fallback(QDir::tempPath() + QStringLiteral("/nc-sound-XXXXXX") + dottedSuffix);
    fallback.setAutoRemove(false);
    if (fallback.open() && fallback.write(bytes) == bytes.size()) {
        fallback.close();
        const auto fallbackPath = fallback.fileName();
        extractedPaths.insert(qrcResourcePath, fallbackPath);
        return fallbackPath;
    }

    qCWarning(lcNotificationSoundPlayer) << "Could not materialise embedded sound" << qrcResourcePath
                                         << "to a filesystem path";
    return {};
}

}

#if !defined(NEXTCLOUD_HAS_NATIVE_SOUND_BACKEND)
namespace {

class NoOpBackend : public NotificationSoundPlayer::Backend
{
public:
    void setSource(const QString &) override {}

    void play(int) override
    {
        if (!_warned) {
            qCWarning(lcNotificationSoundPlayer) << "No native audio backend compiled in; notification sound is silent";
            _warned = true;
        }
        if (onFinished) {
            // Invoke asynchronously so loop bookkeeping does not recurse
            // inside the dispatcher's play() call.
            auto cb = onFinished;
            QMetaObject::invokeMethod(qApp, [cb]() { cb(); }, Qt::QueuedConnection);
        }
    }

    void stop() override {}

    [[nodiscard]] bool handlesLoopsNatively() const override { return true; }
    [[nodiscard]] bool needsFilesystemPath() const override { return true; }

private:
    bool _warned = false;
};

}

std::unique_ptr<NotificationSoundPlayer::Backend> createNotificationSoundPlayerBackend()
{
    return std::make_unique<NoOpBackend>();
}
#endif

NotificationSoundPlayer::NotificationSoundPlayer(QObject *parent)
    : QObject(parent)
    , _backend(createNotificationSoundPlayerBackend())
{
    bindBackend();
}

NotificationSoundPlayer::NotificationSoundPlayer(std::unique_ptr<Backend> backend, QObject *parent)
    : QObject(parent)
    , _backend(std::move(backend))
{
    bindBackend();
}

NotificationSoundPlayer::~NotificationSoundPlayer()
{
    if (_backend) {
        _backend->stop();
    }
}

void NotificationSoundPlayer::bindBackend()
{
    _backend->onFinished = [this]() {
        // Backend may invoke this from a worker thread. Marshal to the
        // dispatcher's thread via the lambda-on-QObject overload of
        // invokeMethod, which silently drops the event if `this` is
        // destroyed before delivery.
        QMetaObject::invokeMethod(this, [this]() { onBackendPlaybackFinished(); }, Qt::QueuedConnection);
    };
}

QString NotificationSoundPlayer::source() const
{
    return _source;
}

int NotificationSoundPlayer::loops() const
{
    return _loops;
}

bool NotificationSoundPlayer::isPlaying() const
{
    return _playing;
}

void NotificationSoundPlayer::setSource(const QString &source)
{
    if (_source == source) {
        return;
    }
    _source = source;
    _sourceAppliedToBackend = false;
    Q_EMIT sourceChanged();
}

void NotificationSoundPlayer::setLoops(int loops)
{
    if (_loops == loops) {
        return;
    }
    _loops = loops;
    Q_EMIT loopsChanged();
}

void NotificationSoundPlayer::play()
{
    if (_source.isEmpty() || _loops <= 0) {
        return;
    }

    if (!_sourceAppliedToBackend) {
        if (_backend->needsFilesystemPath()) {
            const auto resolved = resolveToFilesystemPath(_source);
            if (resolved.isEmpty()) {
                return;
            }
            _backend->setSource(resolved);
        } else {
            // Backends that don't need a real file (e.g. libcanberra playing
            // an XDG sound-theme event) get the raw source for context, but
            // typically ignore it.
            _backend->setSource(_source);
        }
        _sourceAppliedToBackend = true;
    }

    if (_playing) {
        _backend->stop();
    }

    _remainingLoops = _loops;
    setPlaying(true);
    _backend->play(_loops);
}

void NotificationSoundPlayer::stop()
{
    if (!_playing) {
        return;
    }
    _remainingLoops = 0;
    _backend->stop();
    setPlaying(false);
}

void NotificationSoundPlayer::onBackendPlaybackFinished()
{
    if (!_playing) {
        return;
    }

    if (!_backend->handlesLoopsNatively() && --_remainingLoops > 0) {
        _backend->play(_remainingLoops);
        return;
    }

    _remainingLoops = 0;
    setPlaying(false);
}

void NotificationSoundPlayer::setPlaying(bool playing)
{
    if (_playing == playing) {
        return;
    }
    _playing = playing;
    Q_EMIT playingChanged();
}

QString NotificationSoundPlayer::resolveToFilesystemPath(const QString &source)
{
    if (source.isEmpty()) {
        return {};
    }

    QString candidate = source;
    if (source.startsWith(QStringLiteral("qrc://"))) {
        candidate = QStringLiteral(":") + source.mid(6);
    } else if (source.startsWith(QStringLiteral("qrc:"))) {
        candidate = QStringLiteral(":") + source.mid(4);
    } else if (source.startsWith(QStringLiteral("file://"))) {
        candidate = QUrl(source).toLocalFile();
    }

    if (candidate.startsWith(QStringLiteral(":"))) {
        const auto extracted = extractQrcToCache(candidate);
        if (extracted.isEmpty()) {
            return {};
        }
        return extracted;
    }

    if (!QFile::exists(candidate)) {
        qCWarning(lcNotificationSoundPlayer) << "Sound source does not exist:" << source;
        return {};
    }
    return candidate;
}

}
