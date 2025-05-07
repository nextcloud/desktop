/*
 * SPDX-FileCopyrightText: 2018 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2014 ownCloud GmbH
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef SPARKLEUPDATER_H
#define SPARKLEUPDATER_H

#include "updater/updater.h"

#include <QObject>

namespace OCC {

class SparkleUpdater : public Updater
{
    Q_OBJECT
    Q_PROPERTY(QString statusString READ statusString NOTIFY statusChanged)
    Q_PROPERTY(State state READ state NOTIFY statusChanged)

public:
    class SparkleInterface;
    enum class State {
        Unknown = 0,
        Idle,
        Working,
        AwaitingUserInput
    };

    explicit SparkleUpdater(const QUrl &appCastUrl);
    ~SparkleUpdater() override;

    [[nodiscard]] static bool autoUpdaterAllowed();

    [[nodiscard]] bool handleStartup() override { return false; }
    [[nodiscard]] QString statusString() const;
    [[nodiscard]] State state() const;

public slots:
    void setUpdateUrl(const QUrl &url);
    void checkForUpdate() override;
    void backgroundCheckForUpdate() override;

signals:
    void statusChanged();

private:
    std::unique_ptr<SparkleInterface> _interface;
    QString _statusString;
    State _state = State::Unknown;
    friend class SparkleInterface;
};

} // namespace OCC

#endif // SPARKLEUPDATER_H
