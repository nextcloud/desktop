/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <QObject>
#include <QString>

#include <memory>

namespace OCC {

class NotificationSoundPlayer : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString source READ source WRITE setSource NOTIFY sourceChanged)
    Q_PROPERTY(int loops READ loops WRITE setLoops NOTIFY loopsChanged)
    Q_PROPERTY(bool playing READ isPlaying NOTIFY playingChanged)

public:
    class Backend;

    explicit NotificationSoundPlayer(QObject *parent = nullptr);
    explicit NotificationSoundPlayer(std::unique_ptr<Backend> backend, QObject *parent);
    ~NotificationSoundPlayer() override;

    [[nodiscard]] QString source() const;
    [[nodiscard]] int loops() const;
    [[nodiscard]] bool isPlaying() const;

    // Translate a `qrc:`/`file:`/plain path into a real filesystem path,
    // materialising QRC-embedded sounds into the cache on first use.
    // Public so unit tests can exercise the cache + fallback paths.
    static QString resolveToFilesystemPath(const QString &source);

public slots:
    void setSource(const QString &source);
    void setLoops(int loops);
    void play();
    void stop();

signals:
    void sourceChanged();
    void loopsChanged();
    void playingChanged();

private:
    void onBackendPlaybackFinished();
    void setPlaying(bool playing);
    void bindBackend();

    std::unique_ptr<Backend> _backend;
    QString _source;
    int _loops = 1;
    int _remainingLoops = 0;
    bool _playing = false;
    bool _sourceAppliedToBackend = false;
};

}
