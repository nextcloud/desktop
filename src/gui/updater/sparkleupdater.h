/*
 * Copyright (C) by Daniel Molkentin <danimo@owncloud.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
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
